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

		auto overflowClipsChildren = [](SdOverflow overflow) noexcept
		{
			return overflow != SdOverflow::Visible;
		};

		context.layoutSystem.BeginFrame(liveIds.size());
		context.boxTree.Clear();
		std::unordered_map<SdWidgetId, SdUInt32> boxIndexByWidgetId = {};
		boxIndexByWidgetId.reserve(liveIds.size());
		for (SdWidgetId id : liveIds)
		{
			SdWidgetRecord& record = widgets[id];
			record.state.computedClipRect = displayRect;
			const SdStyleInteractionState styleInteraction = resolveStyleInteraction(id);
			ResolveWidgetStyle(record, styleInteraction, record.state.layerPriority);

			SdLayoutResult result = {};
			result.desiredSize = {
				SdResolveLength(record.styleCache.resolvedStyle.width, displaySize.x, 160.0f),
				SdResolveLength(record.styleCache.resolvedStyle.height, displaySize.y, 28.0f)
			};

			SdLayoutConstraints constraints = {};
			constraints.maxSize = displaySize;
			SdLayoutContext layoutContext{
				*this,
				uiObject,
				id,
				record.parentId,
				record.state,
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
			const SdResolvedBoxStyle rootUsedStyle = SdResolveBoxStyle(record.styleCache.resolvedStyle, constraints.maxSize, result.desiredSize);
			const SdResolvedBoxEdges rootBorder = SdResolveBorderEdges(record.styleCache.resolvedStyle.border, record.state.measuredSize);
			const bool styleArrangesChildren = rootUsedStyle.display == SdDisplay::Flex;
			const bool styleClipsChildren = overflowClipsChildren(rootUsedStyle.overflowX)
				|| overflowClipsChildren(rootUsedStyle.overflowY);

			context.layoutSystem.AddNode({
				id,
				record.parentId,
				constraints,
				result,
				record.state.manualRect,
				{ rootBorder.left, rootBorder.top, rootBorder.right, rootBorder.bottom },
				{
					rootUsedStyle.padding.left,
					rootUsedStyle.padding.top,
					rootUsedStyle.padding.right,
					rootUsedStyle.padding.bottom
				},
				record.state.layoutWeight,
				std::max(0.0f, rootUsedStyle.gap),
				record.state.manualLayout,
				record.state.arrangeChildren || styleArrangesChildren,
				record.state.clipChildren || styleClipsChildren
			});

			SdUInt32 parentBoxIndex = SdInvalidIndex<SdUInt32>;
			const auto parentBoxIt = boxIndexByWidgetId.find(record.parentId);
			if (record.parentId != 0 && parentBoxIt != boxIndexByWidgetId.end())
				parentBoxIndex = parentBoxIt->second;
			SdBoxStyle boxStyle = record.styleCache.resolvedStyle;
			SdRect explicitBorderBox = {};
			if (record.state.manualLayout)
			{
				boxStyle.position = SdPosition::Absolute;
				explicitBorderBox = record.state.manualRect;
			}
			const SdUInt32 boxIndex = context.boxTree.AddBox(record.rootStyleNodeId, parentBoxIndex, boxStyle, result.desiredSize, explicitBorderBox);
			boxIndexByWidgetId[id] = boxIndex;
		}

		context.boxTree.Layout(displayRect);
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
			if (SdStyleNode* rootNode = context.stateStorage.FindStyleNodeById(record.rootStyleNodeId))
			{
				rootNode->usedBox = SdBuildUsedBox(node.targetRect, rootNode->resolvedStyle);
				if (const SdBoxNode* box = context.boxTree.FindBoxByStyleNodeId(record.rootStyleNodeId))
				{
					rootNode->layoutBox = {
						box->marginBox,
						box->borderBox,
						box->paddingBox,
						box->contentBox
					};
				}
				record.rootStyleNode = *rootNode;
				for (SdStyleNodeId partNodeId : record.partStyleNodeIds)
				{
					if (SdStyleNode* partNode = context.stateStorage.FindStyleNodeById(partNodeId))
					{
						partNode->usedBox = rootNode->usedBox;
						partNode->layoutBox = rootNode->layoutBox;
					}
				}
			}
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
		context.styleAnimationChannels.Update(context.frame.deltaTime);
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
