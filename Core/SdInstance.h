#pragma once

#include "Animation/SdAnimation.h"
#include "Input/SdInput.h"
#include "Layer/SdLayer.h"
#include "Layout/SdBoxLayout.h"
#include "Render/SdRenderCore.h"
#include "Core/SdRuntimeStorage.h"
#include "Style/SdStyle.h"
#include "Style/SdStyleAnimation.h"
#include "Widget/SdUi.h"

#include <unordered_map>
#include <vector>

namespace Sodium
{
	struct SdFrameDiagnostics final
	{
		SdUInt32 submittedWidgetCount = 0;
		SdUInt32 liveWidgetCount = 0;
		SdUInt32 enteringWidgetCount = 0;
		SdUInt32 leavingWidgetCount = 0;
		SdUInt32 deadWidgetCount = 0;
		SdUInt32 removedWidgetCount = 0;
		SdUInt32 boxNodeCount = 0;
		SdUInt32 hitTestRecordCount = 0;
		SdUInt32 layerDrawChannelCount = 0;
		SdUInt32 activeAnimationChannelCount = 0;
		SdUInt32 activeStyleNodeAnimationChannelCount = 0;
		SdUInt32 activeLayoutTransitionCount = 0;
		SdUInt32 drawCommandCount = 0;
		SdUInt32 drawVertexCount = 0;
		SdUInt32 drawIndexCount = 0;
		SdUInt32 drawBatchCount = 0;
		SdUInt32 resourceUploadCount = 0;
		SdUInt32 createdWidgetCount = 0;
		SdUInt32 reusedWidgetCount = 0;
		SdUInt32 modelCount = 0;
		SdUInt32 styleNodeCount = 0;
		SdUInt32 liveObjectCount = 0;
		SdUInt32 styleResolveCount = 0;
		SdUInt32 styleResolveCacheHitCount = 0;
		SdUInt32 styleResolveCacheMissCount = 0;
		SdUInt32 styleResolvedNodeCount = 0;
		SdUInt32 applyStyleAnimationCount = 0;
		SdUInt32 applyStyleAnimationSkipCount = 0;
		SdUInt32 styleAnimationEnsureCount = 0;
		SdUInt32 styleAnimationFindCount = 0;
		SdUInt32 styleAnimationChannelCreatedCount = 0;
		SdUInt32 styleAnimationTargetSetCount = 0;
		SdUInt32 styleAnimationTargetNoopCount = 0;
		SdUInt32 styleAnimationUpdateVisitedCount = 0;
		SdUInt32 styleAnimationUpdateActiveCount = 0;

		void ResetFrameTransient() noexcept
		{
			submittedWidgetCount = 0;
			liveWidgetCount = 0;
			enteringWidgetCount = 0;
			leavingWidgetCount = 0;
			deadWidgetCount = 0;
			removedWidgetCount = 0;
			boxNodeCount = 0;
			hitTestRecordCount = 0;
			layerDrawChannelCount = 0;
			activeAnimationChannelCount = 0;
			activeStyleNodeAnimationChannelCount = 0;
			activeLayoutTransitionCount = 0;
			drawCommandCount = 0;
			drawVertexCount = 0;
			drawIndexCount = 0;
			drawBatchCount = 0;
			resourceUploadCount = 0;
			createdWidgetCount = 0;
			reusedWidgetCount = 0;
			styleNodeCount = 0;
			liveObjectCount = 0;
			styleResolveCount = 0;
			styleResolveCacheHitCount = 0;
			styleResolveCacheMissCount = 0;
			styleResolvedNodeCount = 0;
			applyStyleAnimationCount = 0;
			applyStyleAnimationSkipCount = 0;
			styleAnimationEnsureCount = 0;
			styleAnimationFindCount = 0;
			styleAnimationChannelCreatedCount = 0;
			styleAnimationTargetSetCount = 0;
			styleAnimationTargetNoopCount = 0;
			styleAnimationUpdateVisitedCount = 0;
			styleAnimationUpdateActiveCount = 0;
		}
	};

	struct SdFrameState final
	{
		SdFrameIndex frameIndex = 0;
		SdDuration deltaTime = std::chrono::milliseconds(16);
		SdVec2 displaySize = { 1280.0f, 720.0f };
		SdFrameDiagnostics diagnostics = {};
	};

	struct SdRuntimeScratch final
	{
		std::vector<SdWidgetId> liveIds = {};
		std::vector<SdWidgetId> paintIds = {};
		std::unordered_map<SdWidgetId, SdUInt32> boxIndexByWidgetId = {};
		std::unordered_map<SdWidgetId, bool> displayHiddenByWidgetId = {};

		void BeginLayoutPaintFrame(SdSize widgetCapacity)
		{
			liveIds.clear();
			paintIds.clear();
			boxIndexByWidgetId.clear();
			displayHiddenByWidgetId.clear();

			liveIds.reserve(widgetCapacity);
			paintIds.reserve(widgetCapacity);
			boxIndexByWidgetId.reserve(widgetCapacity);
			displayHiddenByWidgetId.reserve(widgetCapacity);
		}
	};

	struct SdContext final
	{
		SdFrameState frame = {};
		SdRuntimeScratch scratch = {};
		SdStateStorage stateStorage = {};
		std::vector<SdWidgetId> frameOrder = {};
		SdInputSystem input{ 512 };
		SdAnimationSystem animationSystem = {};
		SdStyleAnimationChannels presentationChannels = {};
		SdStyleSystem styling = {};
		SdBoxTree boxTree = {};
		SdLayerSystem layerSystem = {};
		SdInteractionSystem interactionSystem = {};
		SdRenderSystem renderSystem = {};
		SdRenderSharedData renderSharedData = {};
		SdRenderStats renderStats = {};
		SdThemeView theme = {};
		ISdPlatformBackend* platform = nullptr;
		ISdRendererBackend* renderer = nullptr;
		ISdFontBackend* fontBackend = nullptr;
		SdUInt32 nextOrder = 0;
		SdUInt32 nextActivationOrder = 0;
	};

	class SdInstance final
	{
	private:
		SdContext context = {};
		SdRenderList renderList;
		SdUi uiObject;
		SdTimePoint lastFrameTime = {};

		SdWidgetRecord& GetOrCreateWidgetRecord(SdWidgetId id);
		void MarkSubmitted(SdWidgetRecord& record, SdWidgetId id, SdWidgetId parentId, SdResolvedKey resolvedKey, SdUtf8StringView debugKey);
		void FinishWidgetFrame();
		void EndDeclarationStage();
		void RunLifecycleAnimationStage();
		void RunLayoutAndPaintStage();
		void RunSweepStage();
		SdTransition GetDefaultTransition() const noexcept;
		void UpdateWidgetAnimation(SdWidgetRecord& record);
		void SetWidgetStyleIdentity(SdWidgetRecord& record, SdSpan<const SdStyleClassId> styleClasses, SdStyleScopeId styleScope);
		void ResolveWidgetStyle(SdWidgetRecord& record, SdStyleInteractionState interactionState, SdRootLayer rootLayer);
		void SetBoxStyleAnimationTarget(
			SdWidgetRecord& record,
			SdStyleNode& node,
			const SdBoxStyle& style,
			SdStyleInteractionState interactionState,
			SdRootLayer rootLayer,
			bool immediate);
		void ApplyBoxStyleAnimation(SdStyleNode& node);
		void SetWidgetStyleAnimationTarget(
			SdWidgetRecord& record,
			const SdWidgetRootStyle& style,
			SdStyleInteractionState interactionState,
			SdRootLayer rootLayer,
			bool immediate);
		void ApplyWidgetStyleAnimation(SdWidgetRecord& record);
		void SolveLayoutAndPaint();
		SdUInt32 RemoveDeadWidgets();
		void RefreshDiagnostics();

		template<class T>
		static void StyleThunk(SdInstance& instance, SdWidgetRecord& record, SdStyleInteractionState interactionState, SdRootLayer rootLayer)
		{
			if constexpr (requires { typename T::Style; })
			{
				if constexpr (!std::same_as<typename T::Style, SdWidgetRootStyle>)
					instance.ResolveTypedWidgetStyle<T>(record, interactionState, rootLayer);
			}
		}

		template<class T>
		static void TypedStyleAnimationThunk(SdInstance& instance, SdWidgetRecord& record, SdDuration deltaTime)
		{
			if constexpr (requires { typename T::Style; })
			{
				if constexpr (!std::same_as<typename T::Style, SdWidgetRootStyle>)
					instance.AdvanceTypedWidgetStyleAnimations<T>(record, deltaTime);
			}
		}

		template<class T>
		static void LayoutThunk(void* object, SdLayoutContext& context)
		{
			if constexpr (requires(T& widget, SdLayoutContext& layoutContext) { widget.OnLayout(layoutContext); })
				static_cast<T*>(object)->OnLayout(context);
		}

		template<class T>
		static void ArrangeThunk(void* object, SdArrangeContext& context)
		{
			if constexpr (requires(T& widget, SdArrangeContext& arrangeContext) { widget.OnArrange(arrangeContext); })
				static_cast<T*>(object)->OnArrange(context);
		}

		template<class T>
		static void PaintThunk(void* object, SdPaintContext& context)
		{
			if constexpr (requires(T& widget, SdPaintContext& paintContext) { widget.OnPaint(paintContext); })
				static_cast<T*>(object)->OnPaint(context);
		}

		friend class SdUi;

	public:
		SdUi& ui;

		SdInstance();

		bool Initialize(ISdPlatformBackend* platform, ISdRendererBackend* renderer, ISdFontBackend* fontBackend) noexcept;
		bool Initialize(ISdPlatformBackend& platform, ISdRendererBackend& renderer, ISdFontBackend& fontBackend) noexcept
		{
			return Initialize(&platform, &renderer, &fontBackend);
		}

		void BeginFrame()
		{
			BeginInputFrame();
			context.platform->StartFrame(context.input);
			FinishInputAndBeginUiFrame();
		}
		void BeginFrame(SdVec2 displaySize)
		{
			BeginInputFrame();
			context.input.GetMutableSnapshot().display.size = displaySize;
			FinishInputAndBeginUiFrame();
		}

		void EndFrame();
		void Render();
		void Shutdown();
		SdUInt32 AllocateActivationOrder() noexcept;
		bool IsWidgetDescendantOf(SdWidgetId widgetId, SdWidgetId ancestorWidgetId) const noexcept;

		SdInputSystem& GetInputSystem() noexcept { return context.input; }
		const SdInputSnapshot& GetInput() const noexcept { return context.input.GetSnapshot(); }
		SdRenderList& GetRenderList() noexcept { return renderList; }
		SdRenderSharedData& GetRenderSharedData() noexcept { return context.renderSharedData; }
		const SdRenderSharedData& GetRenderSharedData() const noexcept { return context.renderSharedData; }
		const SdInteractionSystem& GetInteractionSystem() const noexcept { return context.interactionSystem; }
		const SdBoxTree& GetBoxTree() const noexcept { return context.boxTree; }
		const SdLayerSystem& GetLayerSystem() const noexcept { return context.layerSystem; }
		const SdAnimationSystem& GetAnimationSystem() const noexcept { return context.animationSystem; }
		const SdStateStorage& GetStateStorage() const noexcept { return context.stateStorage; }
		SdStyleSystem& GetStyleSystem() noexcept { return context.styling; }
		const SdStyleSystem& GetStyleSystem() const noexcept { return context.styling; }
		const SdRenderSystem& GetRenderSystem() const noexcept { return context.renderSystem; }
		SdRenderPacket GetDrawPacket() const noexcept { return renderList.BuildPacket(static_cast<SdUInt32>(context.frame.frameIndex)); }
		SdFrameIndex GetFrameIndex() const noexcept { return context.frame.frameIndex; }
		SdDuration GetDeltaTime() const noexcept { return context.frame.deltaTime; }
		SdVec2 GetDisplaySize() const noexcept { return context.frame.displaySize; }
		SdRenderStats& GetRenderStats() noexcept { return context.renderStats; }
		const SdRenderStats& GetRenderStats() const noexcept { return context.renderStats; }
		const SdFrameDiagnostics& GetDiagnostics() const noexcept { return context.frame.diagnostics; }
		SdContext& GetContext() noexcept { return context; }
		const SdContext& GetContext() const noexcept { return context; }
		ISdPlatformBackend* GetPlatformBackend() const noexcept { return context.platform; }
		ISdRendererBackend* GetRendererBackend() const noexcept { return context.renderer; }
		ISdFontBackend* GetFontBackend() const noexcept { return context.fontBackend; }
		void SetPlatformBackend(ISdPlatformBackend* platform) noexcept;
		void SetRendererBackend(ISdRendererBackend* renderer) noexcept;
		void SetFontBackend(ISdFontBackend* fontBackend) noexcept;

		template<class T>
		T& GetOrCreateUserState(SdWidgetId widgetId);

		template<class T>
		T& GetOrCreateModel(SdResolvedKey resolvedKey, SdModelLifetime lifetime = SdModelLifetime::Manual, SdWidgetId ownerWidgetId = 0);

		template<class TWidget>
		void ResolveTypedWidgetStyle(SdWidgetRecord& record, SdStyleInteractionState interactionState, SdRootLayer rootLayer);

		template<class TWidget>
		void AdvanceTypedWidgetStyleAnimations(SdWidgetRecord& record, SdDuration deltaTime);

		template<class TWidget>
		const typename TWidget::Style& GetResolvedStyle(SdWidgetId widgetId);

		template<class TWidget>
		const typename TWidget::Style& GetPresentationStyle(SdWidgetId widgetId);

		SdWidgetRootStyle ResolveRootStyleForWidget(
			SdWidgetId widgetId,
			SdStyleInteractionState interactionState,
			SdRootLayer rootLayer) const;
		const SdStyleNode& GetRootStyleNode(SdWidgetId widgetId) const;
		const SdStyleNode& GetStylePart(SdWidgetId widgetId, SdStylePart part) const;
		SdStyleNode& EnsureStylePart(SdWidgetId widgetId, SdStylePart part);
		void SetPartUsedBox(SdWidgetId widgetId, SdStylePart part, const SdUsedBox& usedBox);
		void SetPartLayoutBox(SdWidgetId widgetId, SdStylePart part, const SdUsedBox& layoutBox);
		void SetPartBorderBox(SdWidgetId widgetId, SdStylePart part, SdRect borderBox);

		void BeginInputFrame();
		void FinishInputAndBeginUiFrame();
	};
}
