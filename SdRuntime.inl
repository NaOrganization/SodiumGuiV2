#pragma once

#include <algorithm>

namespace Sodium
{
	inline SdUi::SdUi(SdInstance& owner)
		: instance(owner)
	{
	}

	inline void SdUi::BeginDeclarationFrame()
	{
		idStack.BeginFrame();
	}

	inline SdInstance::SdInstance()
		: renderList(&context.renderStats, &context.renderSharedData), uiObject(*this), ui(uiObject)
	{
	}

	inline void SdInstance::BeginInputFrame()
	{
		++context.frame.frameIndex;
		const SdTimePoint now = std::chrono::time_point_cast<SdDuration>(std::chrono::system_clock::now());
		if (lastFrameTime.time_since_epoch().count() != 0)
			context.frame.deltaTime = now - lastFrameTime;
		lastFrameTime = now;
		context.input.BeginFrame(context.frame.frameIndex);
	}

	inline void SdInstance::FinishInputAndBeginUiFrame(SdVec2 newDisplaySize)
	{
		if (newDisplaySize.x > 0.0f && newDisplaySize.y > 0.0f)
			context.frame.displaySize = newDisplaySize;
		context.input.FinalizeFrame();
		context.interactionSystem.Update(context.layerSystem, context.input.GetSnapshot());
		renderList.SetSharedData(&context.renderSharedData);
		renderList.SetStats(&context.renderStats);
		renderList.Reset();
		context.layerSystem.BeginFrame();
		context.frameOrder.clear();
		context.nextOrder = 0;
		context.stateStorage.BeginFrame();
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
		context.renderSystem.Render(context.renderer, SdRendererFrameInfo{ context.frame.displaySize }, GetDrawPacket());
	}

	inline void SdInstance::Shutdown()
	{
		context.stateStorage.Clear();
		context.frameOrder.clear();
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
		context.renderSharedData.fontBackend = fontBackend;
		if (fontBackend)
			fontBackend->ConfigureRenderSharedData(context.renderSharedData);
	}

	inline SdWidgetRecord& SdInstance::GetOrCreateWidgetRecord(SdWidgetId id)
	{
		SdWidgetRecord& record = context.stateStorage.GetOrCreateWidgetRecord(id);
		if (record.state.id == 0)
		{
			record.state.id = id;
			record.state.lifePhase = SdWidgetLifePhase::Entering;
			record.state.layoutWeight = 0.0f;
			record.state.opacity = 0.0f;
			record.animation.layoutWeight = context.animationSystem.EnsureChannel(id, SdAnimationChannelKind::LayoutWeight, 0.0f);
			record.animation.opacity = context.animationSystem.EnsureChannel(id, SdAnimationChannelKind::Opacity, 0.0f);
			record.animation.rectX = context.animationSystem.EnsureChannel(id, SdAnimationChannelKind::RectPosition, 0.0f);
			record.animation.rectY = context.animationSystem.EnsureChannel(id, SdAnimationChannelKind::RectPosition, 0.0f);
			record.animation.rectWidth = context.animationSystem.EnsureChannel(id, SdAnimationChannelKind::RectSize, 0.0f);
			record.animation.rectHeight = context.animationSystem.EnsureChannel(id, SdAnimationChannelKind::RectSize, 0.0f);
			record.animation.styleColorR = context.animationSystem.EnsureChannel(id, SdAnimationChannelKind::StyleColor, 0.0f);
			record.animation.styleColorG = context.animationSystem.EnsureChannel(id, SdAnimationChannelKind::StyleColor, 0.0f);
			record.animation.styleColorB = context.animationSystem.EnsureChannel(id, SdAnimationChannelKind::StyleColor, 0.0f);
			record.animation.styleColorA = context.animationSystem.EnsureChannel(id, SdAnimationChannelKind::StyleColor, 1.0f);
			record.animation.scrollOffset = context.animationSystem.EnsureChannel(id, SdAnimationChannelKind::ScrollOffset, 0.0f);
		}
		return record;
	}

	inline void SdInstance::MarkSubmitted(SdWidgetRecord& record, SdWidgetId id, SdWidgetId parentId, SdResolvedKey resolvedKey, SdUtf8StringView debugKey)
	{
		record.parentId = parentId;
		record.order = context.nextOrder++;
		record.resolvedKey = resolvedKey;
		record.debugKey.assign(debugKey.data(), debugKey.size());
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
		context.stateStorage.RegisterResolvedKey(resolvedKey, id);
		UpdateWidgetAnimation(record);
		context.frameOrder.push_back(id);
	}

	inline void SdInstance::FinishWidgetFrame()
	{
		EndDeclarationStage();
		RunLifecycleAnimationStage();
		RunLayoutAndPaintStage();
		RunSweepStage();
		RefreshDiagnostics();

		for (auto& [id, record] : context.stateStorage.GetWidgetRecords())
			record.state.submittedThisFrame = false;
	}

	inline void SdInstance::EndDeclarationStage()
	{
		for (auto& [id, record] : context.stateStorage.GetWidgetRecords())
		{
			(void)id;
			if (!record.state.submittedThisFrame && record.state.lifePhase != SdWidgetLifePhase::Dead)
			{
				record.state.lifePhase = SdWidgetLifePhase::Leaving;
				record.state.inputEnabled = false;
				++context.stateStorage.GetStats().leavingWidgetCount;
			}
			if (record.state.lifePhase == SdWidgetLifePhase::Leaving)
				UpdateWidgetAnimation(record);
		}
	}

	inline void SdInstance::RunLifecycleAnimationStage()
	{
		context.animationSystem.Update(context.frame.deltaTime, true, true, false, false);
		for (auto& [id, record] : context.stateStorage.GetWidgetRecords())
		{
			(void)id;
			record.state.layoutWeight = context.animationSystem.GetValue(record.animation.layoutWeight, record.state.layoutWeight);
			record.state.opacity = context.animationSystem.GetValue(record.animation.opacity, record.state.opacity);
			record.state.animationActive = context.animationSystem.GetChannel(record.animation.layoutWeight).active
				|| context.animationSystem.GetChannel(record.animation.opacity).active;

			const bool leaving = record.state.lifePhase == SdWidgetLifePhase::Leaving;
			if (!leaving && record.state.layoutWeight >= 1.0f && record.state.opacity >= 1.0f)
				record.state.lifePhase = SdWidgetLifePhase::Alive;
			if (leaving && record.state.layoutWeight <= 0.0f && record.state.opacity <= 0.0f)
				record.state.lifePhase = SdWidgetLifePhase::Dead;
		}
	}

	inline void SdInstance::RunLayoutAndPaintStage()
	{
		SolveLayoutAndPaint();
	}

	inline void SdInstance::RunSweepStage()
	{
		context.frame.diagnostics.removedWidgetCount = RemoveDeadWidgets();
	}

	inline SdTransition SdInstance::GetDefaultTransition() const noexcept
	{
		const float seconds = std::max(0.001f, context.styleSystem.GetTheme().GetMetric(SdStyleToken::DurationFast));
		return {
			std::chrono::duration_cast<SdDuration>(std::chrono::duration<float>(seconds)),
			SdAnimationEasing::OutCubic
		};
	}

	inline void SdInstance::UpdateWidgetAnimation(SdWidgetRecord& record)
	{
		const bool leaving = record.state.lifePhase == SdWidgetLifePhase::Leaving;
		const float target = leaving ? 0.0f : 1.0f;
		const SdTransition transition = GetDefaultTransition();
		context.animationSystem.SetTarget(record.animation.layoutWeight, target, transition);
		context.animationSystem.SetTarget(record.animation.opacity, target, transition);
		record.state.animationActive = record.state.layoutWeight != target || record.state.opacity != target;
	}

	inline void SdInstance::ResolveWidgetStyle(SdWidgetRecord& record, SdStyleInteractionState interactionState, SdLayerPriority layerPriority)
	{
		if (!record.state.styleDirty
			&& record.styleCache.valid
			&& record.styleCache.widgetClass == record.state.styleClass
			&& record.styleCache.interactionState == interactionState
			&& record.styleCache.layerPriority == layerPriority)
		{
			record.style = record.styleCache.computed;
			ApplyWidgetStyleAnimation(record);
			return;
		}

		const bool firstStyle = !record.styleCache.valid;
		record.style = context.styleSystem.Resolve(record.state.styleClass, interactionState, layerPriority);
		record.styleCache.computed = record.style;
		record.styleCache.widgetClass = record.state.styleClass;
		record.styleCache.interactionState = interactionState;
		record.styleCache.layerPriority = layerPriority;
		record.styleCache.valid = true;
		record.state.styleDirty = false;
		SetWidgetStyleAnimationTarget(record, record.styleCache.computed, firstStyle);
		ApplyWidgetStyleAnimation(record);
	}

	inline void SdInstance::SetWidgetStyleAnimationTarget(SdWidgetRecord& record, const SdComputedStyle& style, bool immediate)
	{
		const auto setChannel = [this, immediate](SdAnimationChannelId channelId, float target)
		{
			if (immediate)
			{
				context.animationSystem.SetImmediate(channelId, target);
				return;
			}
			context.animationSystem.SetTarget(channelId, target, GetDefaultTransition());
		};

		setChannel(record.animation.styleColorR, static_cast<float>(style.background.r));
		setChannel(record.animation.styleColorG, static_cast<float>(style.background.g));
		setChannel(record.animation.styleColorB, static_cast<float>(style.background.b));
		setChannel(record.animation.styleColorA, static_cast<float>(style.background.a));
	}

	inline void SdInstance::ApplyWidgetStyleAnimation(SdWidgetRecord& record)
	{
		const auto toByte = [](float value) noexcept
		{
			return static_cast<SdUInt8>(std::clamp(value, 0.0f, 255.0f));
		};

		record.style.background = {
			toByte(context.animationSystem.GetValue(record.animation.styleColorR, static_cast<float>(record.style.background.r))),
			toByte(context.animationSystem.GetValue(record.animation.styleColorG, static_cast<float>(record.style.background.g))),
			toByte(context.animationSystem.GetValue(record.animation.styleColorB, static_cast<float>(record.style.background.b))),
			toByte(context.animationSystem.GetValue(record.animation.styleColorA, static_cast<float>(record.style.background.a)))
		};

		record.state.animationActive = record.state.animationActive
			|| context.animationSystem.IsActive(record.animation.styleColorR)
			|| context.animationSystem.IsActive(record.animation.styleColorG)
			|| context.animationSystem.IsActive(record.animation.styleColorB)
			|| context.animationSystem.IsActive(record.animation.styleColorA);
	}

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
			if (record.layoutCallback && record.widgetObject.value)
				record.layoutCallback(record.widgetObject.value, layoutContext);

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

		context.animationSystem.Update(context.frame.deltaTime, false, false, true, true);
		for (SdWidgetId id : liveIds)
		{
			applyAnimatedRect(widgets[id]);
			ApplyWidgetStyleAnimation(widgets[id]);
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
			if (record.paintCallback && record.widgetObject.value)
				record.paintCallback(record.widgetObject.value, paintContext);
		}

		if (context.renderSharedData.fontBackend)
		{
			context.renderSharedData.fontBackend->ConfigureRenderSharedData(context.renderSharedData);
			context.renderSharedData.fontBackend->DrainPendingUploads(renderList.GetDrawData().uploads);
			context.renderSharedData.fontBackend->ConfigureRenderSharedData(context.renderSharedData);
		}
	}

	inline SdUInt32 SdInstance::RemoveDeadWidgets()
	{
		return context.stateStorage.RemoveDeadWidgets([this](SdWidgetId widgetId)
		{
			context.animationSystem.RemoveWidget(widgetId);
		});
	}

	inline void SdInstance::RefreshDiagnostics()
	{
		SdFrameDiagnostics& diagnostics = context.frame.diagnostics;
		const auto& widgets = context.stateStorage.GetWidgetRecords();
		diagnostics.submittedWidgetCount = static_cast<SdUInt32>(context.frameOrder.size());
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
		diagnostics.layoutNodeCount = static_cast<SdUInt32>(context.layoutSystem.GetNodes().size());
		diagnostics.hitTestRecordCount = static_cast<SdUInt32>(context.layerSystem.GetHitTestRecords().size());
		diagnostics.layerDrawChannelCount = static_cast<SdUInt32>(context.layerSystem.GetDrawChannels().size());
		diagnostics.activeAnimationChannelCount = static_cast<SdUInt32>(context.animationSystem.GetActiveChannelCount());
		diagnostics.drawCommandCount = static_cast<SdUInt32>(drawData.commands.size());
		diagnostics.drawVertexCount = static_cast<SdUInt32>(drawData.vertices.size());
		diagnostics.drawIndexCount = static_cast<SdUInt32>(drawData.indices.size());
		diagnostics.drawBatchCount = static_cast<SdUInt32>(drawData.batches.size());
		diagnostics.resourceUploadCount = static_cast<SdUInt32>(drawData.uploads.size());
		diagnostics.createdWidgetCount = context.stateStorage.GetStats().createdWidgetCount;
		diagnostics.reusedWidgetCount = context.stateStorage.GetStats().reusedWidgetCount;
		diagnostics.modelCount = context.stateStorage.GetStats().modelCount;
	}

	template<class T>
	T& SdInstance::GetOrCreateUserState(SdWidgetId widgetId)
	{
		return context.stateStorage.GetOrCreateUserState<T>(widgetId);
	}

	template<class T>
	T& SdInstance::GetOrCreateModel(SdResolvedKey resolvedKey)
	{
		return context.stateStorage.GetOrCreateModel<T>(resolvedKey);
	}

	template<class T>
	T& SdWidgetContextBase::State()
	{
		return instance.GetOrCreateUserState<T>(id);
	}

	template<class T>
	T& SdWidgetContextBase::Model()
	{
		assert(resolvedKey != 0);
		return instance.GetOrCreateModel<T>(resolvedKey);
	}

	inline bool SdWidgetContextBase::HasModelKey() const noexcept
	{
		return resolvedKey != 0;
	}

	inline bool SdWidgetContextBase::IsHovered() const noexcept
	{
		return instance.GetInteractionSystem().IsHovered(id);
	}

	inline bool SdWidgetContextBase::IsPressed() const noexcept
	{
		return instance.GetInteractionSystem().IsPressed(id);
	}

	inline bool SdWidgetContextBase::WasPressed() const noexcept
	{
		return instance.GetInteractionSystem().WasPressed(id);
	}

	inline bool SdWidgetContextBase::WasReleased() const noexcept
	{
		return instance.GetInteractionSystem().WasReleased(id);
	}

	inline bool SdWidgetContextBase::WasClicked() const noexcept
	{
		return instance.GetInteractionSystem().WasClicked(id);
	}

	inline bool SdWidgetContextBase::IsCaptured() const noexcept
	{
		return instance.GetInteractionSystem().IsCaptured(id);
	}

	inline bool SdWidgetContextBase::IsFocused() const noexcept
	{
		return instance.GetInteractionSystem().IsFocused(id);
	}

	template<SdDeclarableWidget T, class... TArgs>
	T& SdUi::Declare(TArgs&&... args)
	{
		const SdWidgetId parentId = idStack.CurrentParentId();
		const SdWidgetId id = idStack.ResolveAnonymousWidgetId(Detail::SdTypeHash<T>());
		SdWidgetRecord& record = instance.GetOrCreateWidgetRecord(id);
		const bool created = record.widgetType != std::type_index(typeid(T)) || !record.widgetObject.value;

		if (created)
		{
			record.widgetType = std::type_index(typeid(T));
			record.widgetObject.Create<T>();
			record.layoutCallback = &SdInstance::LayoutThunk<T>;
			record.paintCallback = &SdInstance::PaintThunk<T>;
		}

		instance.MarkSubmitted(record, id, parentId, 0, {});
		T& widget = *record.widgetObject.Get<T>();

		SdCreateContext createContext{
			instance,
			*this,
			id,
			parentId,
			record.state,
			record.style,
			instance.context.theme,
			record.resolvedKey
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
			instance.context.theme,
			record.resolvedKey,
			instance.context.input.GetSnapshot(),
			instance.context.frame.deltaTime,
			instance.context.frame.frameIndex
		};

		idStack.PushParent(id);
		if constexpr (requires(T& value, SdUpdateContext& context, TArgs&&... values) { value.OnUpdate(context, std::forward<TArgs>(values)...); })
			widget.OnUpdate(updateContext, std::forward<TArgs>(args)...);
		else if constexpr (requires(T& value, SdUpdateContext& context) { value.OnUpdate(context); })
			widget.OnUpdate(updateContext);
		idStack.PopParent();

		return widget;
	}

	template<SdDeclarableWidget T, class... TArgs>
	T& SdUi::DeclareKeyed(SdUtf8StringView key, TArgs&&... args)
	{
		const SdWidgetId parentId = idStack.CurrentParentId();
		SdResolvedKey resolvedKey = 0;
		const SdWidgetId id = idStack.ResolveKeyedWidgetId(Detail::SdTypeHash<T>(), key, resolvedKey);
		SdWidgetRecord& record = instance.GetOrCreateWidgetRecord(id);
		const bool created = record.widgetType != std::type_index(typeid(T)) || !record.widgetObject.value;

		if (created)
		{
			record.widgetType = std::type_index(typeid(T));
			record.widgetObject.Create<T>();
			record.layoutCallback = &SdInstance::LayoutThunk<T>;
			record.paintCallback = &SdInstance::PaintThunk<T>;
		}

		instance.MarkSubmitted(record, id, parentId, resolvedKey, key);
		T& widget = *record.widgetObject.Get<T>();

		SdCreateContext createContext{
			instance,
			*this,
			id,
			parentId,
			record.state,
			record.style,
			instance.context.theme,
			record.resolvedKey
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
			instance.context.theme,
			record.resolvedKey,
			instance.context.input.GetSnapshot(),
			instance.context.frame.deltaTime,
			instance.context.frame.frameIndex
		};

		idStack.PushParent(id);
		if constexpr (requires(T& value, SdUpdateContext& context, TArgs&&... values) { value.OnUpdate(context, std::forward<TArgs>(values)...); })
			widget.OnUpdate(updateContext, std::forward<TArgs>(args)...);
		else if constexpr (requires(T& value, SdUpdateContext& context) { value.OnUpdate(context); })
			widget.OnUpdate(updateContext);
		idStack.PopParent();

		return widget;
	}

	template<SdDeclarableWidget TWidget, class TModel>
	TModel& SdUi::Model(SdUtf8StringView key)
	{
		const SdResolvedKey resolvedKey = idStack.ResolveModelKey(Detail::SdTypeHash<TWidget>(), key);
		return instance.GetOrCreateModel<TModel>(resolvedKey);
	}

	template<SdDeclarableWidget TWidget, class TConfigure, class TModel>
	void SdUi::ConfigureModel(SdUtf8StringView key, TConfigure&& configure)
	{
		TModel& model = Model<TWidget, TModel>(key);
		std::forward<TConfigure>(configure)(model);
	}
}
