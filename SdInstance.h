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
		SdUInt32 hitTestRecordCount = 0;
		SdUInt32 activeAnimationChannelCount = 0;
		SdUInt32 drawCommandCount = 0;
		SdUInt32 drawVertexCount = 0;
		SdUInt32 drawIndexCount = 0;
		SdUInt32 drawBatchCount = 0;
		SdUInt32 resourceUploadCount = 0;
		SdUInt32 createdWidgetCount = 0;
		SdUInt32 reusedWidgetCount = 0;
		SdUInt32 modelCount = 0;

		void ResetFrameTransient() noexcept
		{
			submittedWidgetCount = 0;
			liveWidgetCount = 0;
			enteringWidgetCount = 0;
			leavingWidgetCount = 0;
			deadWidgetCount = 0;
			removedWidgetCount = 0;
			hitTestRecordCount = 0;
			activeAnimationChannelCount = 0;
			drawCommandCount = 0;
			drawVertexCount = 0;
			drawIndexCount = 0;
			drawBatchCount = 0;
			resourceUploadCount = 0;
			createdWidgetCount = 0;
			reusedWidgetCount = 0;
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
		ISdPlatformBackend* platform = nullptr;
		ISdRendererBackend* renderer = nullptr;
		ISdFontBackend* fontBackend = nullptr;
	};

	class SdInstance final
	{
	private:
		SdStateStorage stateStorage = {};
		std::vector<SdWidgetId> frameOrder = {};
		SdInputSystem input{ 512 };
		SdAnimationSystem animationSystem = {};
		SdStyleSystem styleSystem = {};
		SdLayoutSystem layoutSystem = {};
		SdLayerSystem layerSystem = {};
		SdInteractionSystem interactionSystem = {};
		SdRenderSharedData renderSharedData = {};
		SdRenderStats renderStats = {};
		SdRenderList renderList;
		SdUi uiObject;
		SdContext context = {};
		SdThemeView theme = {};
		SdTimePoint lastFrameTime = {};
		SdUInt32 nextOrder = 0;

		SdWidgetRecord& GetOrCreateWidgetRecord(SdWidgetId id);
		void MarkSubmitted(SdWidgetRecord& record, SdWidgetId id, SdWidgetId parentId, SdResolvedKey resolvedKey, SdUtf8StringView debugKey);
		void FinishWidgetFrame();
		SdTransition GetDefaultTransition() const noexcept;
		void UpdateWidgetAnimation(SdWidgetRecord& record);
		void ResolveWidgetStyle(SdWidgetRecord& record, SdStyleInteractionState interactionState);
		void SolveLayoutAndPaint();
		SdUInt32 RemoveDeadWidgets();
		void RefreshDiagnostics();

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
			platform.StartFrame(input);
			FinishInputAndBeginUiFrame(newDisplaySize);
		}

		void EndFrame();
		void Render();
		void Shutdown();
		void SetPlatformBackend(ISdPlatformBackend* platform) noexcept;
		void SetRendererBackend(ISdRendererBackend* renderer) noexcept;
		void SetFontBackend(ISdFontBackend* fontBackend) noexcept;

		SdInputSystem& GetInputSystem() noexcept { return input; }
		const SdInputSnapshot& GetInput() const noexcept { return input.GetSnapshot(); }
		SdRenderList& GetRenderList() noexcept { return renderList; }
		SdRenderSharedData& GetRenderSharedData() noexcept { return renderSharedData; }
		const SdRenderSharedData& GetRenderSharedData() const noexcept { return renderSharedData; }
		const SdInteractionSystem& GetInteractionSystem() const noexcept { return interactionSystem; }
		const SdLayoutSystem& GetLayoutSystem() const noexcept { return layoutSystem; }
		const SdLayerSystem& GetLayerSystem() const noexcept { return layerSystem; }
		const SdAnimationSystem& GetAnimationSystem() const noexcept { return animationSystem; }
		const SdStateStorage& GetStateStorage() const noexcept { return stateStorage; }
		const SdStyleSystem& GetStyleSystem() const noexcept { return styleSystem; }
		const SdDrawData& GetRenderData() const noexcept { return renderList.GetDrawData(); }
		SdDrawPacket GetDrawPacket() const noexcept { return renderList.BuildPacket(static_cast<SdUInt32>(context.frame.frameIndex)); }
		SdFrameIndex GetFrameIndex() const noexcept { return context.frame.frameIndex; }
		SdDuration GetDeltaTime() const noexcept { return context.frame.deltaTime; }
		SdVec2 GetDisplaySize() const noexcept { return context.frame.displaySize; }
		SdRenderStats& GetRenderStats() noexcept { return renderStats; }
		const SdRenderStats& GetRenderStats() const noexcept { return renderStats; }
		const SdFrameDiagnostics& GetDiagnostics() const noexcept { return context.frame.diagnostics; }
		ISdPlatformBackend* GetPlatformBackend() const noexcept { return context.platform; }
		ISdRendererBackend* GetRendererBackend() const noexcept { return context.renderer; }
		ISdFontBackend* GetFontBackend() const noexcept { return context.fontBackend; }

		template<class T>
		T& GetOrCreateUserState(SdWidgetId widgetId);

		template<class T>
		T& GetOrCreateModel(SdResolvedKey resolvedKey);

	private:
		void BeginInputFrame();
		void FinishInputAndBeginUiFrame(SdVec2 newDisplaySize);
	};
}
