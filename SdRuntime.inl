#pragma once

#include <algorithm>

namespace Sodium
{
	inline SdUi::SdUi(SdInstance& owner)
		: instance(owner)
	{
		parentStack.reserve(32);
	}

	inline SdWidgetId SdUi::CurrentParentId() const noexcept
	{
		return parentStack.empty() ? 0 : parentStack.back();
	}

	inline void SdUi::BeginDeclarationFrame()
	{
		parentStack.clear();
		nextOrdinalByParent.clear();
	}

	inline SdWidgetId SdUi::ResolveWidgetId(SdUInt64 typeHash)
	{
		const SdWidgetId parentId = CurrentParentId();
		SdUInt32& ordinal = nextOrdinalByParent[parentId];
		++ordinal;

		SdUInt64 seed = 1469598103934665603ull;
		seed = Detail::SdHashCombine(seed, parentId);
		seed = Detail::SdHashCombine(seed, typeHash);
		seed = Detail::SdHashCombine(seed, ordinal);
		return seed == 0 ? 1 : seed;
	}

	inline SdInstance::SdInstance()
		: renderList(&renderStats, &renderSharedData), uiObject(*this), ui(uiObject)
	{
	}

	inline void SdInstance::BeginInputFrame()
	{
		++context.frame.frameIndex;
		const SdTimePoint now = std::chrono::time_point_cast<SdDuration>(std::chrono::system_clock::now());
		if (lastFrameTime.time_since_epoch().count() != 0)
			context.frame.deltaTime = now - lastFrameTime;
		lastFrameTime = now;
		input.BeginFrame(context.frame.frameIndex);
	}

	inline void SdInstance::FinishInputAndBeginUiFrame(SdVec2 newDisplaySize)
	{
		if (newDisplaySize.x > 0.0f && newDisplaySize.y > 0.0f)
			context.frame.displaySize = newDisplaySize;
		input.FinalizeFrame();
		interactionSystem.Update(layerSystem, input.GetSnapshot());
		renderList.SetSharedData(&renderSharedData);
		renderList.SetStats(&renderStats);
		renderList.Reset();
		layerSystem.BeginFrame();
		frameOrder.clear();
		nextOrder = 0;
		context.frame.diagnostics.ResetFrameTransient();
		uiObject.BeginDeclarationFrame();
	}

	inline void SdInstance::BeginFrame(SdVec2 newDisplaySize)
	{
		BeginInputFrame();
		FinishInputAndBeginUiFrame(newDisplaySize);
	}

	inline void SdInstance::EndFrame()
	{
		FinishWidgetFrame();
	}

	inline void SdInstance::Render()
	{
		if (!context.renderer)
			return;
		context.renderer->Render(SdRendererFrameInfo{ context.frame.displaySize }, GetDrawPacket());
	}

	inline void SdInstance::Shutdown()
	{
		widgets.clear();
		frameOrder.clear();
		renderList.Reset();
	}

	inline void SdInstance::SetPlatformBackend(ISdPlatformBackend* platform) noexcept
	{
		context.platform = platform;
	}

	inline void SdInstance::SetRendererBackend(ISdRendererBackend* renderer) noexcept
	{
		context.renderer = renderer;
	}

	inline void SdInstance::SetFontBackend(ISdFontBackend* fontBackend) noexcept
	{
		context.fontBackend = fontBackend;
		renderSharedData.fontBackend = fontBackend;
		if (fontBackend)
			fontBackend->ConfigureRenderSharedData(renderSharedData);
	}

	inline SdInstance::SdWidgetRecord& SdInstance::GetOrCreateWidgetRecord(SdWidgetId id)
	{
		SdWidgetRecord& record = widgets[id];
		if (record.state.id == 0)
		{
			record.state.id = id;
			record.state.lifePhase = SdWidgetLifePhase::Entering;
			record.state.layoutWeight = 0.0f;
			record.state.opacity = 0.0f;
			record.animation.layoutWeight = animationSystem.EnsureChannel(id, SdAnimationChannelKind::LayoutWeight, 0.0f);
			record.animation.opacity = animationSystem.EnsureChannel(id, SdAnimationChannelKind::Opacity, 0.0f);
			record.animation.rectX = animationSystem.EnsureChannel(id, SdAnimationChannelKind::RectPosition, 0.0f);
			record.animation.rectY = animationSystem.EnsureChannel(id, SdAnimationChannelKind::RectPosition, 0.0f);
			record.animation.rectWidth = animationSystem.EnsureChannel(id, SdAnimationChannelKind::RectSize, 0.0f);
			record.animation.rectHeight = animationSystem.EnsureChannel(id, SdAnimationChannelKind::RectSize, 0.0f);
		}
		return record;
	}

	inline void SdInstance::MarkSubmitted(SdWidgetRecord& record, SdWidgetId id, SdWidgetId parentId)
	{
		record.parentId = parentId;
		record.order = nextOrder++;
		record.state.id = id;
		record.state.submittedThisFrame = true;
		record.state.lastSubmittedFrame = context.frame.frameIndex;
		record.state.inputEnabled = true;
		record.state.styleClass = SdStyleWidgetClass::Default;
		record.state.manualLayout = false;
		record.state.arrangeChildren = false;
		record.state.clipChildren = false;
		record.state.manualRect = {};
		record.state.childContentRect = {};
		record.state.computedClipRect = {};
		record.state.childPadding = {};
		record.state.childSpacing = 0.0f;
		if (record.state.lifePhase == SdWidgetLifePhase::Leaving || record.state.lifePhase == SdWidgetLifePhase::Dead)
			record.state.lifePhase = SdWidgetLifePhase::Entering;
		UpdateWidgetAnimation(record);
		frameOrder.push_back(id);
	}

	inline void SdInstance::FinishWidgetFrame()
	{
		for (auto& [id, record] : widgets)
		{
			if (!record.state.submittedThisFrame && record.state.lifePhase != SdWidgetLifePhase::Dead)
			{
				record.state.lifePhase = SdWidgetLifePhase::Leaving;
				record.state.inputEnabled = false;
			}
			if (record.state.lifePhase == SdWidgetLifePhase::Leaving)
				UpdateWidgetAnimation(record);
		}

		animationSystem.Update(context.frame.deltaTime, true, true, false, false);
		for (auto& [id, record] : widgets)
		{
			record.state.layoutWeight = animationSystem.GetValue(record.animation.layoutWeight, record.state.layoutWeight);
			record.state.opacity = animationSystem.GetValue(record.animation.opacity, record.state.opacity);
			record.state.animationActive = animationSystem.GetChannel(record.animation.layoutWeight).active
				|| animationSystem.GetChannel(record.animation.opacity).active;

			const bool leaving = record.state.lifePhase == SdWidgetLifePhase::Leaving;
			if (!leaving && record.state.layoutWeight >= 1.0f && record.state.opacity >= 1.0f)
				record.state.lifePhase = SdWidgetLifePhase::Alive;
			if (leaving && record.state.layoutWeight <= 0.0f && record.state.opacity <= 0.0f)
				record.state.lifePhase = SdWidgetLifePhase::Dead;
		}

		SolveLayoutAndPaint();
		context.frame.diagnostics.removedWidgetCount = RemoveDeadWidgets();
		RefreshDiagnostics();

		for (auto& [id, record] : widgets)
			record.state.submittedThisFrame = false;
	}

	inline void SdInstance::UpdateWidgetAnimation(SdWidgetRecord& record)
	{
		const bool leaving = record.state.lifePhase == SdWidgetLifePhase::Leaving;
		const float target = leaving ? 0.0f : 1.0f;
		animationSystem.SetTarget(record.animation.layoutWeight, target);
		animationSystem.SetTarget(record.animation.opacity, target);
		record.state.animationActive = record.state.layoutWeight != target || record.state.opacity != target;
	}

	inline void SdInstance::SolveLayoutAndPaint()
	{
		std::vector<SdWidgetId> liveIds;
		liveIds.reserve(widgets.size());
		for (const auto& [id, record] : widgets)
		{
			if (record.state.lifePhase != SdWidgetLifePhase::Dead)
				liveIds.push_back(id);
		}

		std::sort(liveIds.begin(), liveIds.end(), [this](SdWidgetId left, SdWidgetId right)
		{
			return widgets[left].order < widgets[right].order;
		});

		const SdVec2 displaySize = context.frame.displaySize;
		const SdRect displayRect = { 0.0f, 0.0f, displaySize.x, displaySize.y };

		auto setAnimatedRectTarget = [this](SdWidgetRecord& record, const SdRect& targetRect)
		{
			if (record.state.lastRect.Width() <= 0.0f || record.state.lastRect.Height() <= 0.0f)
			{
				animationSystem.SetImmediate(record.animation.rectX, targetRect.min.x);
				animationSystem.SetImmediate(record.animation.rectY, targetRect.min.y);
				animationSystem.SetImmediate(record.animation.rectWidth, targetRect.Width());
				animationSystem.SetImmediate(record.animation.rectHeight, targetRect.Height());
				record.state.animatedRect = targetRect;
				record.state.lastRect = targetRect;
				return;
			}

			if (interactionSystem.IsPressed(record.state.id))
			{
				animationSystem.SetImmediate(record.animation.rectX, targetRect.min.x);
				animationSystem.SetImmediate(record.animation.rectY, targetRect.min.y);
				animationSystem.SetImmediate(record.animation.rectWidth, targetRect.Width());
				animationSystem.SetImmediate(record.animation.rectHeight, targetRect.Height());
				return;
			}

			animationSystem.SetTarget(record.animation.rectX, targetRect.min.x);
			animationSystem.SetTarget(record.animation.rectY, targetRect.min.y);
			animationSystem.SetTarget(record.animation.rectWidth, targetRect.Width());
			animationSystem.SetTarget(record.animation.rectHeight, targetRect.Height());
		};

		auto applyAnimatedRect = [this](SdWidgetRecord& record)
		{
			const float x = animationSystem.GetValue(record.animation.rectX, record.state.targetRect.min.x);
			const float y = animationSystem.GetValue(record.animation.rectY, record.state.targetRect.min.y);
			const float width = std::max(0.0f, animationSystem.GetValue(record.animation.rectWidth, record.state.targetRect.Width()));
			const float height = std::max(0.0f, animationSystem.GetValue(record.animation.rectHeight, record.state.targetRect.Height()));
			record.state.animatedRect = { x, y, x + width, y + height };
			record.state.lastRect = record.state.animatedRect;
			record.state.animationActive = record.state.animationActive
				|| animationSystem.IsActive(record.animation.rectX)
				|| animationSystem.IsActive(record.animation.rectY)
				|| animationSystem.IsActive(record.animation.rectWidth)
				|| animationSystem.IsActive(record.animation.rectHeight);
		};

		layoutSystem.BeginFrame(liveIds.size());
		for (SdWidgetId id : liveIds)
		{
			SdWidgetRecord& record = widgets[id];
			record.state.computedClipRect = displayRect;
			SdStyleInteractionState styleInteraction = SdStyleInteractionState::Normal;
			if (interactionSystem.IsPressed(id))
				styleInteraction = SdStyleInteractionState::Pressed;
			else if (interactionSystem.IsHovered(id))
				styleInteraction = SdStyleInteractionState::Hovered;
			record.style = styleSystem.Resolve(record.state.styleClass, styleInteraction);

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
				theme,
				constraints,
				result
			};
			if (record.layoutCallback && record.widgetObject.value)
				record.layoutCallback(record.widgetObject.value, layoutContext);

			record.style = styleSystem.Resolve(record.state.styleClass, styleInteraction);
			record.state.measuredSize = result.desiredSize;

			layoutSystem.AddNode({
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

		layoutSystem.Measure(displaySize);
		layoutSystem.Arrange(displayRect);
		const std::vector<SdLayoutNode>& layoutNodes = layoutSystem.GetNodes();
		for (const SdLayoutNode& node : layoutNodes)
		{
			SdWidgetRecord& record = widgets[node.widgetId];
			record.state.measuredSize = node.result.desiredSize;
			record.state.targetRect = node.targetRect;
			record.state.childContentRect = node.childContentRect;
			record.state.computedClipRect = node.computedClipRect;
			if (node.parentIndex != SdInvalidIndex<SdUInt32> && node.parentIndex < layoutNodes.size())
			{
				const SdWidgetRecord& parentRecord = widgets[layoutNodes[node.parentIndex].widgetId];
				if (static_cast<SdUInt8>(record.state.layerPriority) < static_cast<SdUInt8>(parentRecord.state.layerPriority))
					record.state.layerPriority = parentRecord.state.layerPriority;
			}
			setAnimatedRectTarget(record, record.state.targetRect);
		}

		animationSystem.Update(context.frame.deltaTime, false, false, true, true);
		for (SdWidgetId id : liveIds)
			applyAnimatedRect(widgets[id]);

		std::vector<SdWidgetId> paintIds = liveIds;
		std::sort(paintIds.begin(), paintIds.end(), [this](SdWidgetId left, SdWidgetId right)
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
			layerSystem.AddHitTestRecord({
				id,
				record.state.layerPriority,
				record.state.animatedRect,
				record.state.computedClipRect,
				paintOrder++,
				record.state.inputEnabled && record.state.lifePhase != SdWidgetLifePhase::Leaving
			});
		}
		interactionSystem.Update(layerSystem, input.GetSnapshot());

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
				theme,
				renderList,
				record.state.targetRect,
				record.state.animatedRect,
				record.state.computedClipRect,
				record.state.opacity,
				static_cast<SdLayerId>(record.state.layerPriority)
			};
			if (record.paintCallback && record.widgetObject.value)
				record.paintCallback(record.widgetObject.value, paintContext);
		}

		if (renderSharedData.fontBackend)
		{
			renderSharedData.fontBackend->ConfigureRenderSharedData(renderSharedData);
			renderSharedData.fontBackend->DrainPendingUploads(renderList.GetDrawData().uploads);
			renderSharedData.fontBackend->ConfigureRenderSharedData(renderSharedData);
		}
	}

	inline SdUInt32 SdInstance::RemoveDeadWidgets()
	{
		SdUInt32 removedCount = 0;
		for (auto it = widgets.begin(); it != widgets.end();)
		{
			if (it->second.state.lifePhase == SdWidgetLifePhase::Dead)
			{
				animationSystem.RemoveWidget(it->first);
				it = widgets.erase(it);
				++removedCount;
			}
			else
				++it;
		}
		return removedCount;
	}

	inline void SdInstance::RefreshDiagnostics()
	{
		SdFrameDiagnostics& diagnostics = context.frame.diagnostics;
		diagnostics.submittedWidgetCount = static_cast<SdUInt32>(frameOrder.size());
		diagnostics.liveWidgetCount = 0;
		diagnostics.enteringWidgetCount = 0;
		diagnostics.leavingWidgetCount = 0;
		diagnostics.deadWidgetCount = 0;

		for (const auto& [id, record] : widgets)
		{
			(void)id;
			switch (record.state.lifePhase)
			{
			case SdWidgetLifePhase::Entering:
				++diagnostics.enteringWidgetCount;
				++diagnostics.liveWidgetCount;
				break;
			case SdWidgetLifePhase::Alive:
				++diagnostics.liveWidgetCount;
				break;
			case SdWidgetLifePhase::Leaving:
				++diagnostics.leavingWidgetCount;
				++diagnostics.liveWidgetCount;
				break;
			case SdWidgetLifePhase::Dead:
				++diagnostics.deadWidgetCount;
				break;
			default:
				break;
			}
		}

		const SdDrawData& drawData = renderList.GetDrawData();
		diagnostics.hitTestRecordCount = static_cast<SdUInt32>(layerSystem.GetHitTestRecords().size());
		diagnostics.activeAnimationChannelCount = static_cast<SdUInt32>(animationSystem.GetActiveChannelCount());
		diagnostics.drawCommandCount = static_cast<SdUInt32>(drawData.commands.size());
		diagnostics.drawVertexCount = static_cast<SdUInt32>(drawData.vertices.size());
		diagnostics.drawIndexCount = static_cast<SdUInt32>(drawData.indices.size());
		diagnostics.drawBatchCount = static_cast<SdUInt32>(drawData.batches.size());
		diagnostics.resourceUploadCount = static_cast<SdUInt32>(drawData.uploads.size());
	}

	template<class T>
	T& SdInstance::GetOrCreateUserState(SdWidgetId widgetId)
	{
		SdWidgetRecord& record = GetOrCreateWidgetRecord(widgetId);
		Detail::SdAnyObject& object = record.userStates[std::type_index(typeid(T))];
		if (!object.value)
			return object.Create<T>();
		if (T* state = object.Get<T>())
			return *state;
		return object.Create<T>();
	}

	template<class T>
	T& SdWidgetContextBase::State()
	{
		return instance.GetOrCreateUserState<T>(id);
	}

	inline bool SdWidgetContextBase::IsHovered() const noexcept
	{
		return instance.GetInteractionSystem().IsHovered(id);
	}

	inline bool SdWidgetContextBase::IsPressed() const noexcept
	{
		return instance.GetInteractionSystem().IsPressed(id);
	}

	inline bool SdWidgetContextBase::WasClicked() const noexcept
	{
		return instance.GetInteractionSystem().WasClicked(id);
	}

	inline bool SdWidgetContextBase::IsFocused() const noexcept
	{
		return instance.GetInteractionSystem().IsFocused(id);
	}

	template<SdDeclarableWidget T, class... TArgs>
	T& SdUi::Declare(TArgs&&... args)
	{
		const SdWidgetId parentId = CurrentParentId();
		const SdWidgetId id = ResolveWidgetId(Detail::SdTypeHash<T>());
		SdInstance::SdWidgetRecord& record = instance.GetOrCreateWidgetRecord(id);
		const bool created = record.widgetType != std::type_index(typeid(T)) || !record.widgetObject.value;

		if (created)
		{
			record.widgetType = std::type_index(typeid(T));
			record.widgetObject.Create<T>();
			record.layoutCallback = &SdInstance::LayoutThunk<T>;
			record.paintCallback = &SdInstance::PaintThunk<T>;
		}

		instance.MarkSubmitted(record, id, parentId);
		T& widget = *record.widgetObject.Get<T>();

		SdCreateContext createContext{
			instance,
			*this,
			id,
			parentId,
			record.state,
			record.style,
			instance.theme
		};
		if (created)
		{
			if constexpr (requires(T& value, SdCreateContext& context, TArgs&&... values) { value.OnCreate(context, std::forward<TArgs>(values)...); })
				widget.OnCreate(createContext, std::forward<TArgs>(args)...);
			else if constexpr (requires(T& value, SdCreateContext& context) { value.OnCreate(context); })
				widget.OnCreate(createContext);
		}

		SdUpdateContext updateContext{
			instance,
			*this,
			id,
			parentId,
			record.state,
			record.style,
			instance.theme,
			instance.input.GetSnapshot(),
			instance.context.frame.deltaTime,
			instance.context.frame.frameIndex
		};

		parentStack.push_back(id);
		if constexpr (requires(T& value, SdUpdateContext& context, TArgs&&... values) { value.OnUpdate(context, std::forward<TArgs>(values)...); })
			widget.OnUpdate(updateContext, std::forward<TArgs>(args)...);
		else if constexpr (requires(T& value, SdUpdateContext& context) { value.OnUpdate(context); })
			widget.OnUpdate(updateContext);
		parentStack.pop_back();

		return widget;
	}
}
