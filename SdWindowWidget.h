#pragma once

#include "SdBasicWidgets.h"

#include <algorithm>
#include <functional>
#include <type_traits>
#include <utility>

namespace Sodium
{
	struct SdWindowOptions final
	{
		SdVec2 initialPosition = { 80.0f, 72.0f };
		SdVec2 initialSize = { 360.0f, 280.0f };
		SdVec2 minSize = { 180.0f, 72.0f };
		SdVec2 padding = { 12.0f, 10.0f };
		float titleBarHeight = 30.0f;
		float itemSpacing = 8.0f;
		float radius = 5.0f;
		bool movable = true;
		bool resizable = true;
		bool collapsible = true;
		bool closable = true;
	};

	class SdWindow final : public SdWidgetTag
	{
	public:
		struct State
		{
			SdUtf8String title = {};
			SdWindowOptions options = {};
			SdVec2 position = {};
			SdVec2 size = {};
			bool initialized = false;
			bool visible = true;
			bool hovered = false;
			bool titleHovered = false;
			bool closeHovered = false;
			bool collapseHovered = false;
			bool resizeHovered = false;
			bool dragging = false;
			bool resizing = false;
			bool collapsed = false;
			bool closeRequested = false;
		};

		void OnUpdate(SdUpdateContext& context, SdUtf8StringView title)
		{
			UpdateWindow(context, title, nullptr, SdWindowOptions{});
		}

		void OnUpdate(SdUpdateContext& context, SdUtf8StringView title, bool& open)
		{
			UpdateWindow(context, title, &open, SdWindowOptions{});
		}

		void OnUpdate(SdUpdateContext& context, SdUtf8StringView title, const SdWindowOptions& options)
		{
			UpdateWindow(context, title, nullptr, options);
		}

		void OnUpdate(SdUpdateContext& context, SdUtf8StringView title, bool& open, const SdWindowOptions& options)
		{
			UpdateWindow(context, title, &open, options);
		}

		template<class TContent>
			requires std::is_invocable_v<TContent&, SdUi&>
		void OnUpdate(SdUpdateContext& context, SdUtf8StringView title, TContent&& content)
		{
			if (UpdateWindow(context, title, nullptr, SdWindowOptions{}))
				std::forward<TContent>(content)(context.ui);
		}

		template<class TContent>
			requires std::is_invocable_v<TContent&, SdUi&>
		void OnUpdate(SdUpdateContext& context, SdUtf8StringView title, bool& open, TContent&& content)
		{
			if (UpdateWindow(context, title, &open, SdWindowOptions{}))
				std::forward<TContent>(content)(context.ui);
		}

		template<class TContent>
			requires std::is_invocable_v<TContent&, SdUi&>
		void OnUpdate(SdUpdateContext& context, SdUtf8StringView title, const SdWindowOptions& options, TContent&& content)
		{
			if (UpdateWindow(context, title, nullptr, options))
				std::forward<TContent>(content)(context.ui);
		}

		template<class TContent>
			requires std::is_invocable_v<TContent&, SdUi&>
		void OnUpdate(SdUpdateContext& context, SdUtf8StringView title, bool& open, const SdWindowOptions& options, TContent&& content)
		{
			if (UpdateWindow(context, title, &open, options))
				std::forward<TContent>(content)(context.ui);
		}

		void OnLayout(SdLayoutContext& context)
		{
			const State& state = context.State<State>();
			if (!state.visible)
			{
				context.SetDesiredSize({});
				context.widgetState.manualLayout = true;
				context.widgetState.manualRect = { state.position, state.position };
				context.widgetState.arrangeChildren = true;
				context.widgetState.clipChildren = true;
				return;
			}

			const float titleBarHeight = std::max(22.0f, state.options.titleBarHeight);
			const SdVec2 size = {
				std::max(state.options.minSize.x, state.size.x),
				state.collapsed ? titleBarHeight : std::max(state.options.minSize.y, state.size.y)
			};

			context.SetDesiredSize(size);
			context.widgetState.manualLayout = true;
			context.widgetState.manualRect = { state.position, state.position + size };
			context.widgetState.arrangeChildren = true;
			context.widgetState.clipChildren = true;
			context.widgetState.childPadding = {
				state.options.padding.x,
				titleBarHeight + state.options.padding.y,
				state.options.padding.x,
				state.options.padding.y
			};
			context.widgetState.childSpacing = state.options.itemSpacing;
		}

		void OnPaint(SdPaintContext& context)
		{
			const State& state = context.State<State>();
			if (!state.visible)
				return;

			const float titleBarHeight = std::max(22.0f, state.options.titleBarHeight);
			const SdRect rect = context.animatedRect;
			const SdRect titleRect = { rect.min.x, rect.min.y, rect.max.x, rect.min.y + titleBarHeight };
			const SdColor bodyFill = state.hovered ? SdColor{ 28, 35, 45, 245 } : context.style.background;
			const SdColor titleFill = state.dragging ? SdColor{ 62, 100, 138, 255 }
				: state.titleHovered ? SdColor{ 46, 68, 91, 255 }
				: SdColor{ 38, 49, 64, 255 };

			context.renderList.AddRectFilled(rect, SdApplyOpacity(bodyFill, context.opacity), context.clipRect, state.options.radius);
			context.renderList.AddRectFilled(titleRect, SdApplyOpacity(titleFill, context.opacity), context.clipRect, state.options.radius);
			context.renderList.AddRect(rect, SdApplyOpacity({ 102, 119, 140, 255 }, context.opacity), context.clipRect, 1.0f, state.options.radius);
			context.renderList.AddText(state.title, { rect.min.x + 12.0f, rect.min.y + 8.0f }, SdApplyOpacity(SdColorWhite, context.opacity), context.clipRect);

			if (state.options.collapsible)
				DrawCollapseButton(context, BuildCollapseRect(rect, titleBarHeight), state);
			if (state.options.closable)
				DrawCloseButton(context, BuildCloseRect(rect, titleBarHeight), state);
			if (state.options.resizable && !state.collapsed)
				DrawResizeGrip(context, rect, state);
		}

	private:
		bool UpdateWindow(SdUpdateContext& context, SdUtf8StringView title, bool* open, SdWindowOptions options)
		{
			State& state = context.State<State>();
			NormalizeOptions(options);
			if (!state.initialized)
			{
				state.position = options.initialPosition;
				state.size = options.initialSize;
				state.initialized = true;
			}

			state.title.assign(title.data(), title.size());
			state.options = options;
			state.options.closable = options.closable && open != nullptr;
			state.visible = !open || *open;
			state.closeRequested = false;
			state.size.x = std::max(options.minSize.x, state.size.x);
			state.size.y = std::max(options.minSize.y, state.size.y);
			context.widgetState.layerPriority = SdLayerPriority::Floating;
			context.widgetState.styleClass = SdStyleWidgetClass::Window;

			if (!state.visible)
			{
				return false;
			}

			const SdVec2 mousePosition = context.input.GetMousePosition();
			const SdVec2 mouseDelta = context.input.GetMouseDelta();
			const SdRect windowRect = BuildCurrentRect(context.widgetState.lastRect, state);
			const float titleBarHeight = std::max(22.0f, options.titleBarHeight);
			const SdRect titleRect = { windowRect.min.x, windowRect.min.y, windowRect.max.x, windowRect.min.y + titleBarHeight };
			const SdRect closeRect = BuildCloseRect(windowRect, titleBarHeight);
			const SdRect collapseRect = BuildCollapseRect(windowRect, titleBarHeight);
			const SdRect resizeRect = BuildResizeRect(windowRect);

			const bool windowHovered = context.widgetState.inputEnabled && context.IsHovered();
			state.hovered = windowHovered;
			state.titleHovered = windowHovered && titleRect.Contains(mousePosition);
			state.closeHovered = windowHovered && options.closable && open && closeRect.Contains(mousePosition);
			state.collapseHovered = windowHovered && options.collapsible && collapseRect.Contains(mousePosition);
			state.resizeHovered = windowHovered && options.resizable && !state.collapsed && resizeRect.Contains(mousePosition);

			if (context.input.IsMouseButtonDown(SdMouseButton::Left))
			{
				if (state.resizeHovered)
				{
					state.resizing = true;
				}
				else if (options.movable && state.titleHovered && !state.closeHovered && !state.collapseHovered)
				{
					state.dragging = true;
				}
			}

			if (!context.IsPressed())
			{
				state.dragging = false;
				state.resizing = false;
			}

			if (state.dragging)
				state.position += mouseDelta;
			if (state.resizing)
			{
				state.size.x = std::max(options.minSize.x, state.size.x + mouseDelta.x);
				state.size.y = std::max(options.minSize.y, state.size.y + mouseDelta.y);
			}

			KeepTitleBarReachable(state, context.instance.GetDisplaySize());

			if (context.input.IsMouseButtonUp(SdMouseButton::Left))
			{
				if (state.collapseHovered)
					state.collapsed = !state.collapsed;
				if (state.closeHovered && open)
				{
					*open = false;
					state.closeRequested = true;
					state.visible = false;
				}
				state.dragging = false;
				state.resizing = false;
			}

			return state.visible && !state.collapsed;
		}

		static void NormalizeOptions(SdWindowOptions& options)
		{
			options.titleBarHeight = std::max(22.0f, options.titleBarHeight);
			options.minSize.x = std::max(80.0f, options.minSize.x);
			options.minSize.y = std::max(options.titleBarHeight, options.minSize.y);
			options.initialSize.x = std::max(options.minSize.x, options.initialSize.x);
			options.initialSize.y = std::max(options.minSize.y, options.initialSize.y);
			options.padding.x = std::max(0.0f, options.padding.x);
			options.padding.y = std::max(0.0f, options.padding.y);
			options.itemSpacing = std::max(0.0f, options.itemSpacing);
			options.radius = std::max(0.0f, options.radius);
		}

		static SdRect BuildCurrentRect(const SdRect& lastRect, const State& state)
		{
			if (lastRect.Width() > 0.0f && lastRect.Height() > 0.0f)
				return lastRect;

			const float height = state.collapsed
				? std::max(22.0f, state.options.titleBarHeight)
				: state.size.y;
			return { state.position, state.position + SdVec2{ state.size.x, height } };
		}

		static SdRect BuildCloseRect(const SdRect& rect, float titleBarHeight)
		{
			const float size = std::min(22.0f, titleBarHeight - 6.0f);
			const float top = rect.min.y + (titleBarHeight - size) * 0.5f;
			return { rect.max.x - size - 6.0f, top, rect.max.x - 6.0f, top + size };
		}

		static SdRect BuildCollapseRect(const SdRect& rect, float titleBarHeight)
		{
			const float size = std::min(22.0f, titleBarHeight - 6.0f);
			const float top = rect.min.y + (titleBarHeight - size) * 0.5f;
			return { rect.max.x - size * 2.0f - 10.0f, top, rect.max.x - size - 10.0f, top + size };
		}

		static SdRect BuildResizeRect(const SdRect& rect)
		{
			return { rect.max.x - 18.0f, rect.max.y - 18.0f, rect.max.x, rect.max.y };
		}

		static void KeepTitleBarReachable(State& state, const SdVec2& displaySize)
		{
			const float titleBarHeight = std::max(22.0f, state.options.titleBarHeight);
			const float minimumVisibleWidth = std::min(48.0f, state.size.x);
			state.position.x = std::clamp(state.position.x, -state.size.x + minimumVisibleWidth, std::max(0.0f, displaySize.x - minimumVisibleWidth));
			state.position.y = std::clamp(state.position.y, 0.0f, std::max(0.0f, displaySize.y - titleBarHeight));
		}

		static void DrawCloseButton(SdPaintContext& context, const SdRect& rect, const State& state)
		{
			const SdColor fill = state.closeHovered ? SdColor{ 164, 66, 66, 255 } : SdColor{ 65, 77, 94, 255 };
			context.renderList.AddRectFilled(rect, SdApplyOpacity(fill, context.opacity), context.clipRect, 3.0f);
			context.renderList.AddLine({ rect.min.x + 6.0f, rect.min.y + 6.0f }, { rect.max.x - 6.0f, rect.max.y - 6.0f }, SdApplyOpacity(SdColorWhite, context.opacity), context.clipRect, 1.5f);
			context.renderList.AddLine({ rect.max.x - 6.0f, rect.min.y + 6.0f }, { rect.min.x + 6.0f, rect.max.y - 6.0f }, SdApplyOpacity(SdColorWhite, context.opacity), context.clipRect, 1.5f);
		}

		static void DrawCollapseButton(SdPaintContext& context, const SdRect& rect, const State& state)
		{
			const SdColor fill = state.collapseHovered ? SdColor{ 82, 110, 140, 255 } : SdColor{ 65, 77, 94, 255 };
			context.renderList.AddRectFilled(rect, SdApplyOpacity(fill, context.opacity), context.clipRect, 3.0f);
			const SdVec2 center = { (rect.min.x + rect.max.x) * 0.5f, (rect.min.y + rect.max.y) * 0.5f };
			if (state.collapsed)
			{
				context.renderList.AddTriangleFilled(
					{ center.x - 4.0f, center.y - 6.0f },
					{ center.x - 4.0f, center.y + 6.0f },
					{ center.x + 5.0f, center.y },
					SdApplyOpacity(SdColorWhite, context.opacity),
					context.clipRect);
				return;
			}

			context.renderList.AddTriangleFilled(
				{ center.x - 6.0f, center.y - 3.0f },
				{ center.x + 6.0f, center.y - 3.0f },
				{ center.x, center.y + 5.0f },
				SdApplyOpacity(SdColorWhite, context.opacity),
				context.clipRect);
		}

		static void DrawResizeGrip(SdPaintContext& context, const SdRect& rect, const State& state)
		{
			const SdColor color = state.resizeHovered || state.resizing ? SdColor{ 118, 196, 255, 255 } : SdColor{ 110, 126, 145, 210 };
			const SdColor tinted = SdApplyOpacity(color, context.opacity);
			context.renderList.AddLine({ rect.max.x - 14.0f, rect.max.y - 5.0f }, { rect.max.x - 5.0f, rect.max.y - 14.0f }, tinted, context.clipRect, 1.0f);
			context.renderList.AddLine({ rect.max.x - 10.0f, rect.max.y - 5.0f }, { rect.max.x - 5.0f, rect.max.y - 10.0f }, tinted, context.clipRect, 1.0f);
		}
	};

}
