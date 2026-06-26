#pragma once

namespace Sodium
{
	inline void SdInstance::SolveLayoutAndPaint()
	{
		auto& widgets = context.stateStorage.GetWidgetRecords();
		context.scratch.BeginLayoutPaintFrame(widgets.size());
		std::vector<SdWidgetId>& liveIds = context.scratch.liveIds;
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

		auto hitTestRect = [](const SdWidgetRecord& record) noexcept
		{
			const SdRect& borderBox = record.rootStyleNode.layoutBox.borderBox;
			return borderBox.Width() > 0.0f || borderBox.Height() > 0.0f
				? borderBox
				: record.state.animatedRect;
		};

		auto overflowClipsChildren = [](SdOverflow overflow) noexcept
		{
			return overflow != SdOverflow::Visible;
		};

		auto createsStackingContext = [&widgets](const SdWidgetRecord& record) noexcept
		{
			if (record.state.createsStackingContext)
				return true;
			if (record.state.portalRoot != SdPortalRoot::None)
				return true;
			if (record.state.rootLayer == SdRootLayer::Floating || record.state.rootLayer == SdRootLayer::Popup || record.state.rootLayer == SdRootLayer::Tooltip)
				return true;
			if (record.styleCache.presentationStyle.zIndex != 0)
				return true;
			if (record.styleCache.presentationStyle.opacity < 1.0f)
				return true;
			(void)widgets;
			return false;
		};

		auto intersectRect = [](const SdRect& left, const SdRect& right) noexcept
		{
			SdRect result = {
				std::max(left.min.x, right.min.x),
				std::max(left.min.y, right.min.y),
				std::min(left.max.x, right.max.x),
				std::min(left.max.y, right.max.y)
			};
			result.max.x = std::max(result.min.x, result.max.x);
			result.max.y = std::max(result.min.y, result.max.y);
			return result;
		};

		context.boxTree.Clear();
		auto& boxIndexByWidgetId = context.scratch.boxIndexByWidgetId;
		auto& displayHiddenByWidgetId = context.scratch.displayHiddenByWidgetId;
		for (SdWidgetId id : liveIds)
		{
			SdWidgetRecord& record = widgets[id];
			const auto parentIt = widgets.find(record.parentId);
			if (record.parentId != 0 && parentIt != widgets.end())
			{
				const SdWidgetRecord& parentRecord = parentIt->second;
				if (static_cast<SdUInt8>(record.state.rootLayer) < static_cast<SdUInt8>(parentRecord.state.rootLayer))
					record.state.rootLayer = parentRecord.state.rootLayer;
				record.state.computedStackingOrder = std::max(record.state.computedStackingOrder, parentRecord.state.computedStackingOrder);
			}
			record.state.rootLayer = record.state.portalRoot != SdPortalRoot::None
				? SdRootLayerFromPortalRoot(record.state.portalRoot)
				: record.state.rootLayer;
			record.state.computedClipRect = displayRect;
			const SdStyleInteractionState styleInteraction = resolveStyleInteraction(id);
			ResolveWidgetStyle(record, styleInteraction, record.state.rootLayer);

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

			ResolveWidgetStyle(record, styleInteraction, record.state.rootLayer);
			record.state.measuredSize = result.desiredSize;

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
		for (SdWidgetId id : liveIds)
		{
			SdWidgetRecord& record = widgets[id];
			const SdBoxNode* box = context.boxTree.FindBoxByStyleNodeId(record.rootStyleNodeId);
			const SdRect targetRect = box ? box->borderBox : SdRect{};
			const SdRect childContentRect = box ? box->contentBox : SdRect{};
			const SdUsedBox layoutBox = box ? SdUsedBoxFromNode(*box) : SdUsedBox{};
			record.state.targetRect = targetRect;
			record.state.childContentRect = childContentRect;
			record.layoutCache.measuredSize = record.state.measuredSize;
			record.layoutCache.targetRect = targetRect;
			if (SdStyleNode* rootNode = context.stateStorage.FindStyleNodeById(record.rootStyleNodeId))
			{
				rootNode->usedBox = layoutBox;
				rootNode->layoutBox = layoutBox;
				record.rootStyleNode = *rootNode;
			}
		}

		for (SdWidgetId id : liveIds)
		{
			SdWidgetRecord& record = widgets[id];
			bool hiddenByDisplay = record.styleCache.resolvedStyle.display == SdDisplay::None;
			SdRect clipRect = displayRect;
			const auto parentIt = widgets.find(record.parentId);
			if (record.parentId != 0 && parentIt != widgets.end())
			{
				const SdWidgetRecord& parentRecord = parentIt->second;
				const auto parentHiddenIt = displayHiddenByWidgetId.find(record.parentId);
				hiddenByDisplay = hiddenByDisplay || (parentHiddenIt != displayHiddenByWidgetId.end() && parentHiddenIt->second);
				clipRect = record.state.escapesParentClip ? displayRect : parentRecord.state.computedClipRect;
				const SdBoxNode* parentBox = context.boxTree.FindBoxByStyleNodeId(parentRecord.rootStyleNodeId);
				const bool parentClipsChildren = parentRecord.state.clipChildren
					|| overflowClipsChildren(parentRecord.styleCache.resolvedStyle.overflowX)
					|| overflowClipsChildren(parentRecord.styleCache.resolvedStyle.overflowY);
				if (!record.state.escapesParentClip && parentBox && parentClipsChildren)
					clipRect = intersectRect(clipRect, parentBox->contentBox);
				if (static_cast<SdUInt8>(record.state.rootLayer) < static_cast<SdUInt8>(parentRecord.state.rootLayer))
					record.state.rootLayer = parentRecord.state.rootLayer;
				record.state.computedStackingOrder = std::max(record.state.computedStackingOrder, parentRecord.state.computedStackingOrder);
			}
			record.state.rootLayer = record.state.portalRoot != SdPortalRoot::None
				? SdRootLayerFromPortalRoot(record.state.portalRoot)
				: record.state.rootLayer;
			displayHiddenByWidgetId[id] = hiddenByDisplay;
			if (hiddenByDisplay)
				clipRect = {};
			record.state.computedClipRect = clipRect;
			record.layoutCache.clipRect = clipRect;
			ResolveWidgetStyle(record, resolveStyleInteraction(record.state.id), record.state.rootLayer);
			if (record.arrangeCallback)
			{
				if (void* widgetObject = context.stateStorage.GetWidgetObjectPointer(record))
				{
					const SdStyleNode& rootStyleNode = GetRootStyleNode(record.state.id);
					SdArrangeContext arrangeContext{
						*this,
						uiObject,
						record.state.id,
						record.parentId,
						record.state,
						context.theme,
						record.resolvedKey,
						rootStyleNode.layoutBox
					};
					record.arrangeCallback(widgetObject, arrangeContext);
				}
			}
			setAnimatedRectTarget(record, record.state.targetRect);
		}

		context.animationSystem.Update(context.frame.deltaTime, false, false, true, true, false, false);
		context.presentationChannels.Update(context.frame.deltaTime);
		for (SdWidgetId id : liveIds)
		{
			applyAnimatedRect(widgets[id]);
			ApplyWidgetStyleAnimation(widgets[id]);
			if (widgets[id].typedStyleAnimationCallback)
				widgets[id].typedStyleAnimationCallback(*this, widgets[id], context.frame.deltaTime);
		}

		SdUInt32 nextStackingContextId = 1;
		for (SdWidgetId id : liveIds)
		{
			SdWidgetRecord& record = widgets[id];
			const auto parentIt = widgets.find(record.parentId);
			const SdWidgetRecord* parentRecord = parentIt != widgets.end() ? &parentIt->second : nullptr;
			const bool newContext = createsStackingContext(record);
			record.state.parentStackingContextId = parentRecord ? parentRecord->state.stackingContextId : 0;
			record.state.stackingContextDepth = parentRecord ? parentRecord->state.stackingContextDepth : 0;
			record.state.createsStackingContext = newContext;
			if (newContext)
			{
				record.state.stackingContextId = nextStackingContextId++;
				record.state.stackingContextDepth = parentRecord ? static_cast<SdUInt16>(parentRecord->state.stackingContextDepth + 1) : 1;
				record.state.stackingContextZIndex = parentRecord ? parentRecord->state.stackingContextZIndex : record.styleCache.presentationStyle.zIndex;
				record.state.stackingContextActivationOrder = parentRecord ? parentRecord->state.stackingContextActivationOrder : record.state.computedStackingOrder;
				record.state.stackingContextTreeOrder = parentRecord ? parentRecord->state.stackingContextTreeOrder : record.order;
				context.layerSystem.AddStackingContext({
					record.state.stackingContextId,
					record.state.parentStackingContextId,
					record.state.id,
					record.state.stackingContextDepth,
					record.state.rootLayer,
					record.styleCache.presentationStyle.zIndex,
					record.state.computedStackingOrder,
					record.order
				});
			}
			else
			{
				record.state.stackingContextId = parentRecord ? parentRecord->state.stackingContextId : 0;
				record.state.stackingContextZIndex = parentRecord ? parentRecord->state.stackingContextZIndex : 0;
				record.state.stackingContextActivationOrder = parentRecord ? parentRecord->state.stackingContextActivationOrder : 0;
				record.state.stackingContextTreeOrder = parentRecord ? parentRecord->state.stackingContextTreeOrder : record.order;
			}

			if (record.state.portalRoot != SdPortalRoot::None)
			{
				context.layerSystem.AddPortalRecord({
					record.state.id,
					record.state.portalOwnerWidgetId,
					record.state.portalAnchorWidgetId,
					record.state.portalRoot,
					record.state.rootLayer,
					record.state.animatedRect,
					true,
					record.state.escapesParentClip
				});
			}
		}

		std::vector<SdWidgetId>& paintIds = context.scratch.paintIds;
		paintIds.assign(liveIds.begin(), liveIds.end());
		auto buildStackingKey = [&widgets](SdWidgetId id, SdUInt32 paintOrder)
		{
			const SdWidgetRecord& record = widgets[id];
			return SdMakeStackingKey(
				record.state.rootLayer,
				record.styleCache.presentationStyle.zIndex,
				record.order,
				paintOrder,
				record.state.stackingContextId,
				record.state.stackingContextDepth,
				record.state.computedStackingOrder,
				record.state.stackingContextZIndex,
				record.state.stackingContextActivationOrder,
				record.state.stackingContextTreeOrder);
		};
		auto buildWidgetPath = [&widgets](SdWidgetId id)
		{
			std::vector<SdWidgetId> path = {};
			for (SdWidgetId currentId = id; currentId != 0;)
			{
				path.push_back(currentId);
				const auto it = widgets.find(currentId);
				if (it == widgets.end())
					break;
				currentId = it->second.parentId;
			}
			std::reverse(path.begin(), path.end());
			return path;
		};
		std::sort(paintIds.begin(), paintIds.end(), [&widgets](SdWidgetId left, SdWidgetId right)
		{
			const SdWidgetRecord& leftRecord = widgets[left];
			const SdWidgetRecord& rightRecord = widgets[right];
			const SdStackingKey leftKey = SdMakeStackingKey(
				leftRecord.state.rootLayer,
				leftRecord.styleCache.presentationStyle.zIndex,
				leftRecord.order,
				leftRecord.order,
				leftRecord.state.stackingContextId,
				leftRecord.state.stackingContextDepth,
				leftRecord.state.computedStackingOrder,
				leftRecord.state.stackingContextZIndex,
				leftRecord.state.stackingContextActivationOrder,
				leftRecord.state.stackingContextTreeOrder);
			const SdStackingKey rightKey = SdMakeStackingKey(
				rightRecord.state.rootLayer,
				rightRecord.styleCache.presentationStyle.zIndex,
				rightRecord.order,
				rightRecord.order,
				rightRecord.state.stackingContextId,
				rightRecord.state.stackingContextDepth,
				rightRecord.state.computedStackingOrder,
				rightRecord.state.stackingContextZIndex,
				rightRecord.state.stackingContextActivationOrder,
				rightRecord.state.stackingContextTreeOrder);
			if (SdLessStackingKey(leftKey, rightKey))
				return true;
			if (SdLessStackingKey(rightKey, leftKey))
				return false;
			return left < right;
		});

		SdUInt32 paintOrder = 0;
		for (SdWidgetId id : paintIds)
		{
			SdWidgetRecord& record = widgets[id];
			const auto hiddenIt = displayHiddenByWidgetId.find(id);
			if (hiddenIt != displayHiddenByWidgetId.end() && hiddenIt->second)
				continue;
			const SdStackingKey stackingKey = buildStackingKey(id, paintOrder);
			SdLayerDrawRecord drawRecord = {};
			drawRecord.widgetId = id;
			drawRecord.rootLayer = record.state.rootLayer;
			drawRecord.clipRect = record.state.computedClipRect;
			drawRecord.paintOrder = paintOrder;
			drawRecord.key = stackingKey;
			drawRecord.bounds = record.state.animatedRect;
			drawRecord.escapesParentClip = record.state.escapesParentClip;
			drawRecord.portalRoot = record.state.portalRoot;
			context.layerSystem.AddDrawRecord(drawRecord);

			SdHitTestRecord hitRecord = {};
			hitRecord.widgetId = id;
			hitRecord.rootLayer = record.state.rootLayer;
			hitRecord.rect = hitTestRect(record);
			hitRecord.clipRect = record.state.computedClipRect;
			hitRecord.paintOrder = paintOrder++;
			hitRecord.inputEnabled = record.state.inputEnabled && record.state.lifePhase != SdWidgetLifePhase::Leaving;
			hitRecord.key = stackingKey;
			hitRecord.portalRoot = record.state.portalRoot;
			hitRecord.parentWidgetId = record.parentId;
			hitRecord.widgetPath = buildWidgetPath(id);
			context.layerSystem.AddHitTestRecord(hitRecord);
		}
		context.layerSystem.Finalize();

		for (SdWidgetId id : paintIds)
		{
			SdWidgetRecord& record = widgets[id];
			const auto hiddenIt = displayHiddenByWidgetId.find(id);
			if (hiddenIt != displayHiddenByWidgetId.end() && hiddenIt->second)
				continue;
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
				record.rootStyleNode.layoutBox,
				record.state.opacity,
				static_cast<SdLayerId>(static_cast<SdUInt64>(record.state.rootLayer))
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
			context.renderSharedData.fontBackend->DrainPendingUploads(renderList.GetDrawData().uploadRequests);
			context.renderSharedData.fontBackend->ConfigureRenderSharedData(context.renderSharedData);
		}
	}
}
