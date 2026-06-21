#pragma once

#include "SdAnimation.h"
#include "SdInput.h"
#include "SdLayer.h"
#include "SdLayout.h"
#include "SdRenderCore.h"
#include "SdRuntimeStorage.h"
#include "SdStyle.h"
#include "SdUi.h"

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
		SdUInt32 layoutNodeCount = 0;
		SdUInt32 hitTestRecordCount = 0;
		SdUInt32 layerDrawChannelCount = 0;
		SdUInt32 activeAnimationChannelCount = 0;
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

		void ResetFrameTransient() noexcept
		{
			submittedWidgetCount = 0;
			liveWidgetCount = 0;
			enteringWidgetCount = 0;
			leavingWidgetCount = 0;
			deadWidgetCount = 0;
			removedWidgetCount = 0;
			layoutNodeCount = 0;
			hitTestRecordCount = 0;
			layerDrawChannelCount = 0;
			activeAnimationChannelCount = 0;
			drawCommandCount = 0;
			drawVertexCount = 0;
			drawIndexCount = 0;
			drawBatchCount = 0;
			resourceUploadCount = 0;
			createdWidgetCount = 0;
			reusedWidgetCount = 0;
			styleNodeCount = 0;
			liveObjectCount = 0;
		}
	};

	struct SdFrameState final
	{
		SdFrameIndex frameIndex = 0;
		SdDuration deltaTime = std::chrono::milliseconds(16);
		SdVec2 displaySize = { 1280.0f, 720.0f };
		SdFrameDiagnostics diagnostics = {};
	};

	struct SdContext final
	{
		SdFrameState frame = {};
		SdStateStorage stateStorage = {};
		std::vector<SdWidgetId> frameOrder = {};
		SdInputSystem input{ 512 };
		SdAnimationSystem animationSystem = {};
		SdStyleSystem styleSystem = {};
		SdLayoutSystem layoutSystem = {};
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
		void ResolveWidgetStyle(SdWidgetRecord& record, SdStyleInteractionState interactionState, SdLayerPriority layerPriority);
		void SetWidgetStyleAnimationTarget(SdWidgetRecord& record, const SdComputedStyle& style, bool immediate);
		void ApplyWidgetStyleAnimation(SdWidgetRecord& record);
		void SolveLayoutAndPaint();
		SdUInt32 RemoveDeadWidgets();
		void RefreshDiagnostics();

		template<class T>
		static void StyleThunk(SdInstance& instance, SdWidgetRecord& record, SdStyleInteractionState interactionState, SdLayerPriority layerPriority)
		{
			if constexpr (requires { typename T::Style; })
				instance.ResolveTypedWidgetStyle<T>(record, interactionState, layerPriority);
		}

		template<class T>
		static void TypedStyleAnimationThunk(SdInstance& instance, SdWidgetRecord& record, SdDuration deltaTime)
		{
			if constexpr (requires { typename T::Style; })
				instance.AdvanceTypedWidgetStyleAnimations<T>(record, deltaTime);
		}

		template<class T>
		static void LayoutThunk(void* object, SdLayoutContext& context)
		{
			if constexpr (requires(T& widget, SdLayoutContext& layoutContext) { widget.OnLayout(layoutContext); })
				static_cast<T*>(object)->OnLayout(context);
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

		void BeginFrame(SdVec2 newDisplaySize = {});

		template<class TPlatform>
		void BeginFrame(TPlatform& platform, SdVec2 newDisplaySize = {})
		{
			if constexpr (std::derived_from<TPlatform, ISdPlatformBackend>)
				SetPlatformBackend(&platform);
			BeginInputFrame();
			platform.StartFrame(context.input);
			FinishInputAndBeginUiFrame(newDisplaySize);
		}

		void EndFrame();
		void Render();
		void Shutdown();
		void SetPlatformBackend(ISdPlatformBackend* platform) noexcept;
		void SetRendererBackend(ISdRendererBackend* renderer) noexcept;
		void SetFontBackend(ISdFontBackend* fontBackend) noexcept;

		SdInputSystem& GetInputSystem() noexcept { return context.input; }
		const SdInputSnapshot& GetInput() const noexcept { return context.input.GetSnapshot(); }
		SdRenderList& GetRenderList() noexcept { return renderList; }
		SdRenderSharedData& GetRenderSharedData() noexcept { return context.renderSharedData; }
		const SdRenderSharedData& GetRenderSharedData() const noexcept { return context.renderSharedData; }
		const SdInteractionSystem& GetInteractionSystem() const noexcept { return context.interactionSystem; }
		const SdLayoutSystem& GetLayoutSystem() const noexcept { return context.layoutSystem; }
		const SdLayerSystem& GetLayerSystem() const noexcept { return context.layerSystem; }
		const SdAnimationSystem& GetAnimationSystem() const noexcept { return context.animationSystem; }
		const SdStateStorage& GetStateStorage() const noexcept { return context.stateStorage; }
		SdStyleSystem& GetStyleSystem() noexcept { return context.styleSystem; }
		const SdStyleSystem& GetStyleSystem() const noexcept { return context.styleSystem; }
		const SdRenderSystem& GetRenderSystem() const noexcept { return context.renderSystem; }
		const SdDrawData& GetRenderData() const noexcept { return renderList.GetDrawData(); }
		SdDrawPacket GetDrawPacket() const noexcept { return renderList.BuildPacket(static_cast<SdUInt32>(context.frame.frameIndex)); }
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

		template<class T>
		T& GetOrCreateUserState(SdWidgetId widgetId);

		template<class T>
		T& GetOrCreateModel(SdResolvedKey resolvedKey);

		template<class TWidget>
		void ResolveTypedWidgetStyle(SdWidgetRecord& record, SdStyleInteractionState interactionState, SdLayerPriority layerPriority);

		template<class TWidget>
		void AdvanceTypedWidgetStyleAnimations(SdWidgetRecord& record, SdDuration deltaTime);

		template<class TWidget>
		const typename TWidget::Style& GetTargetStyle(SdWidgetId widgetId);

		template<class TWidget>
		const typename TWidget::Style& GetComputedStyle(SdWidgetId widgetId);

		template<class TWidget>
		const typename TWidget::Style& GetResolvedStyle(SdWidgetId widgetId);

		template<class TWidget>
		const typename TWidget::Style& GetPresentationStyle(SdWidgetId widgetId);

		const SdStyleNode& GetRootStyleNode(SdWidgetId widgetId) const;
		const SdStyleNode& GetStylePart(SdWidgetId widgetId, SdStylePart part) const;
		SdStyleNode& EnsureStylePart(SdWidgetId widgetId, SdStylePart part);

	private:
		void BeginInputFrame();
		void FinishInputAndBeginUiFrame(SdVec2 newDisplaySize);
	};
}
