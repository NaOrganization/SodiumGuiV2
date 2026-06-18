#pragma once

#include "SdInput.h"
#include "SdRenderCore.h"
#include "SdUi.h"

#include <typeindex>
#include <unordered_map>
#include <vector>

namespace Sodium
{
	class SdInstance final
	{
	private:
		struct SdWidgetRecord final
		{
			SdWidgetState state = {};
			SdComputedStyle style = {};
			Detail::SdAnyObject widgetObject = {};
			std::unordered_map<std::type_index, Detail::SdAnyObject> userStates = {};
			std::type_index widgetType = std::type_index(typeid(void));
			SdWidgetId parentId = 0;
			SdUInt32 order = 0;
			void(*layoutCallback)(void*, SdLayoutContext&) = nullptr;
			void(*paintCallback)(void*, SdPaintContext&) = nullptr;
		};

		std::unordered_map<SdWidgetId, SdWidgetRecord> widgets = {};
		std::vector<SdWidgetId> frameOrder = {};
		SdInputSystem input{ 512 };
		SdRenderSharedData renderSharedData = {};
		SdRenderList renderList;
		SdUi uiObject;
		SdThemeView theme = {};
		SdVec2 displaySize = { 1280.0f, 720.0f };
		SdFrameIndex frameIndex = 0;
		SdDuration deltaTime = std::chrono::milliseconds(16);
		SdTimePoint lastFrameTime = {};
		SdUInt32 nextOrder = 0;

		SdWidgetRecord& GetOrCreateWidgetRecord(SdWidgetId id);
		void MarkSubmitted(SdWidgetRecord& record, SdWidgetId id, SdWidgetId parentId);
		void FinishWidgetFrame();
		void UpdateWidgetAnimation(SdWidgetRecord& record);
		void SolveLayoutAndPaint();
		void RemoveDeadWidgets();

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
			BeginInputFrame();
			platform.StartFrame(input);
			FinishInputAndBeginUiFrame(newDisplaySize);
		}

		void EndFrame();
		void Shutdown();
		void SetFontBackend(ISdFontBackend* fontBackend) noexcept;

		SdInputSystem& GetInputSystem() noexcept { return input; }
		const SdInputSnapshot& GetInput() const noexcept { return input.GetSnapshot(); }
		SdRenderList& GetRenderList() noexcept { return renderList; }
		SdRenderSharedData& GetRenderSharedData() noexcept { return renderSharedData; }
		const SdRenderSharedData& GetRenderSharedData() const noexcept { return renderSharedData; }
		const SdDrawData& GetRenderData() const noexcept { return renderList.GetDrawData(); }
		SdDrawPacket GetDrawPacket() const noexcept { return renderList.BuildPacket(static_cast<SdUInt32>(frameIndex)); }
		SdFrameIndex GetFrameIndex() const noexcept { return frameIndex; }
		SdDuration GetDeltaTime() const noexcept { return deltaTime; }
		SdVec2 GetDisplaySize() const noexcept { return displaySize; }

		template<class T>
		T& GetOrCreateUserState(SdWidgetId widgetId);

	private:
		void BeginInputFrame();
		void FinishInputAndBeginUiFrame(SdVec2 newDisplaySize);
	};
}
