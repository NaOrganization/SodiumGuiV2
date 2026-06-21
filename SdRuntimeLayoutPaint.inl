#pragma once

namespace Sodium
{
	inline void SdInstance::SolveLayoutAndPaint()
	{
		std::vector<SdWidgetId> liveIds;
		auto& widgets = context.stateStorage.GetWidgetRecords();
		liveIds.reserve(widgets.size());
		for (const auto& [id, record] : widgets)
		{
			if (record.state.lifePhase != SdWidgetLifePhase::Dead)
				liveIds.push_back(id);
		}

		std::sort(liveIds.begin(), liveIds.end(), [&widgets](SdWidgetId left, SdWidgetId right)
		{
			return widgets[left].order < widgets[right].order;
		});

		const SdVec2 displaySize = context.frame.displaySize;
		const SdRect displayRect = { 0.0f, 0.0f, displaySize.x, displaySize.y };

		auto resolveStyleInteraction = [this](SdWidgetId id)
		{
			if (context.interactionSystem.IsPressed(id))
				return SdStyleInteractionState::Pressed;
			if (context.interactionSystem.IsHovered(id))
				return SdStyleInteractionState::Hovered;
			if (context.interactionSystem.IsFocused(id))
				return SdStyleInteractionState::Focused;
			return SdStyleInteractionState::Normal;
		};

		auto setAnimatedRectTarget = [this](SdWidgetRecord& record, const SdRect& targetRect)
		{
			if (record.state.lastRect.Width() <= 0.0f || record.state.lastRect.Height() <= 0.0f)
			{
				context.animationSystem.SetImmediate(record.animation.rectX, targetRect.min.x);
				context.animationSystem.SetImmediate(record.animation.rectY, targetRect.min.y);
				context.animationSystem.SetImmediate(record.animation.rectWidth, targetRect.Width());
				context.animationSystem.SetImmediate(record.animation.rectHeight, targetRect.Height());
				record.state.animatedRect = targetRect;
				record.state.lastRect = targetRect;
				return;
			}

			if (context.interactionSystem.IsPressed(record.state.id))
			{
				context.animationSystem.SetImmediate(record.animation.rectX, targetRect.min.x);
				context.animationSystem.SetImmediate(record.animation.rectY, targetRect.min.y);
				context.animationSystem.SetImmediate(record.animation.rectWidth, targetRect.Width());
				context.animationSystem.SetImmediate(record.animation.rectHeight, targetRect.Height());
				return;
			}

			const SdTransition transition = GetDefaultTransition();
			context.animationSystem.SetTarget(record.animation.rectX, targetRect.min.x, transition);
			context.animationSystem.SetTarget(record.animation.rectY, targetRect.min.y, transition);
			context.animationSystem.SetTarget(record.animation.rectWidth, targetRect.Width(), transition);
			context.animationSystem.SetTarget(record.animation.rectHeight, targetRect.Height(), transition);
		};

		auto applyAnimatedRect = [this](SdWidgetRecord& record)
		{
			const float x = context.animationSystem.GetValue(record.animation.rectX, record.state.targetRect.min.x);
			const float y = context.animationSystem.GetValue(record.animation.rectY, record.state.targetRect.min.y);
			const float width = std::max(0.0f, context.animationSystem.GetValue(record.animation.rectWidth, record.state.targetRect.Width()));
			const float height = std::max(0.0f, context.animationSystem.GetValue(record.animation.rectHeight, record.state.targetRect.Height()));
			record.state.animatedRect = { x, y, x + width, y + height };
			record.state.lastRect = record.state.animatedRect;
			record.layoutCache.animatedRect = record.state.animatedRect;
			record.state.animationActive = record.state.animationActive
				|| context.animationSystem.IsActive(record.animation.rectX)
				|| context.animationSystem.IsActive(record.animation.rectY)
				|| context.animationSystem.IsActive(record.animation.rectWidth)
				|| context.animationSystem.IsActive(record.animation.rectHeight);
		};

		context.layoutSystem.BeginFrame(liveIds.size());
		for (SdWidgetId id : liveIds)
		{
			SdWidgetRecord& record = widgets[id];
			record.state.computedClipRect = displayRect;
			const SdStyleInteractionState styleInteraction = resolveStyleInteraction(id);
			ResolveWidgetStyle(record, styleInteraction, record.state.layerPriority);

			SdLayoutResult result = {};
			result.desiredSize = {
				record.style.width > 0.0f ? record.style.width : 160.0f,
				record.style.height > 0.0f ? record.style.height : 28.0f
			};

			SdLayoutConstraints constraints = {};
			constraints.maxSize = displaySize;
			SdLayoutContext layoutContext{
				*this,
				uiObject,
				id,
				record.parentId,
				record.state,
				record.style,
				context.theme,
				record.resolvedKey,
				constraints,
				result
			};
			if (record.layoutCallback)
			{
				if (void* widgetObject = context.stateStorage.GetWidgetObjectPointer(record))
					record.layoutCallback(widgetObject, layoutContext);
			}

			ResolveWidgetStyle(record, styleInteraction, record.state.layerPriority);
			record.state.measuredSize = result.desiredSize;

			context.layoutSystem.AddNode({
				id,
				record.parentId,
				constraints,
				result,
				record.state.manualRect,
				record.state.childPadding,
				record.state.layoutWeight,
				record.state.childSpacing,
				record.state.manualLayout,
				record.state.arrangeChildren,
				record.state.clipChildren
			});
		}

		context.layoutSystem.Measure(displaySize);
		context.layoutSystem.Arrange(displayRect);
		const std::vector<SdLayoutNode>& layoutNodes = context.layoutSystem.GetNodes();
		for (const SdLayoutNode& node : layoutNodes)
		{
			SdWidgetRecord& record = widgets[node.widgetId];
			record.state.measuredSize = node.result.desiredSize;
			record.state.targetRect = node.targetRect;
			record.state.childContentRect = node.childContentRect;
			record.state.computedClipRect = node.computedClipRect;
			record.layoutCache.measuredSize = node.result.desiredSize;
			record.layoutCache.targetRect = node.targetRect;
			record.layoutCache.clipRect = node.computedClipRect;
			if (node.parentIndex != SdInvalidIndex<SdUInt32> && node.parentIndex < layoutNodes.size())
			{
				const SdWidgetRecord& parentRecord = widgets[layoutNodes[node.parentIndex].widgetId];
				if (static_cast<SdUInt8>(record.state.layerPriority) < static_cast<SdUInt8>(parentRecord.state.layerPriority))
					record.state.layerPriority = parentRecord.state.layerPriority;
			}
			ResolveWidgetStyle(record, resolveStyleInteraction(record.state.id), record.state.layerPriority);
			setAnimatedRectTarget(record, record.state.targetRect);
		}

		context.animationSystem.Update(context.frame.deltaTime, false, false, true, true, false, false);
		context.animationSystem.Update(context.frame.deltaTime, false, false, false, false, true, false);
		for (SdWidgetId id : liveIds)
		{
			applyAnimatedRect(widgets[id]);
			ApplyWidgetStyleAnimation(widgets[id]);
			if (widgets[id].typedStyleAnimationCallback)
				widgets[id].typedStyleAnimationCallback(*this, widgets[id], context.frame.deltaTime);
		}

		std::vector<SdWidgetId> paintIds = liveIds;
		std::sort(paintIds.begin(), paintIds.end(), [&widgets](SdWidgetId left, SdWidgetId right)
		{
			const SdWidgetRecord& leftRecord = widgets[left];
			const SdWidgetRecord& rightRecord = widgets[right];
			if (leftRecord.state.layerPriority != rightRecord.state.layerPriority)
				return static_cast<SdUInt8>(leftRecord.state.layerPriority) < static_cast<SdUInt8>(rightRecord.state.layerPriority);
			return leftRecord.order < rightRecord.order;
		});

		SdUInt32 paintOrder = 0;
		for (SdWidgetId id : paintIds)
		{
			SdWidgetRecord& record = widgets[id];
			context.layerSystem.AddDrawRecord({
				id,
				record.state.layerPriority,
				record.state.computedClipRect,
				paintOrder
			});
			context.layerSystem.AddHitTestRecord({
				id,
				record.state.layerPriority,
				record.state.animatedRect,
				record.state.computedClipRect,
				paintOrder++,
				record.state.inputEnabled && record.state.lifePhase != SdWidgetLifePhase::Leaving
			});
		}
		context.interactionSystem.Update(context.layerSystem, context.input.GetSnapshot());

		for (SdWidgetId id : paintIds)
		{
			SdWidgetRecord& record = widgets[id];
			if (record.state.opacity <= 0.0f)
				continue;

			SdPaintContext paintContext{
				*this,
				uiObject,
				id,
				record.parentId,
				record.state,
				record.style,
				context.theme,
				record.resolvedKey,
				renderList,
				record.state.targetRect,
				record.state.animatedRect,
				record.state.computedClipRect,
				record.state.opacity,
				static_cast<SdLayerId>(record.state.layerPriority)
			};
			if (record.paintCallback)
			{
				if (void* widgetObject = context.stateStorage.GetWidgetObjectPointer(record))
					record.paintCallback(widgetObject, paintContext);
			}
		}

		if (context.renderSharedData.fontBackend)
		{
			context.renderSharedData.fontBackend->ConfigureRenderSharedData(context.renderSharedData);
			context.renderSharedData.fontBackend->DrainPendingUploads(renderList.GetDrawData().uploads);
			context.renderSharedData.fontBackend->ConfigureRenderSharedData(context.renderSharedData);
		}
	}
}
