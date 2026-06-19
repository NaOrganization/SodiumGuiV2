#pragma once

#include "SdRuntime.h"
#include "SdUtf8.h"

#include <algorithm>

namespace Sodium
{
	inline SdColor SdApplyOpacity(SdColor color, float opacity)
	{
		color.a = static_cast<SdUInt8>(static_cast<float>(color.a) * std::clamp(opacity, 0.0f, 1.0f));
		return color;
	}

	class SdText final : public SdWidgetTag
	{
	public:
		struct State
		{
			SdUtf8String text = {};
		};

		void OnUpdate(SdUpdateContext& context, SdUtf8StringView text)
		{
			context.widgetState.styleClass = SdStyleWidgetClass::Text;
			context.State<State>().text.assign(text.data(), text.size());
		}

		void OnLayout(SdLayoutContext& context)
		{
			const State& state = context.State<State>();
			ISdFontBackend* fontBackend = context.instance.GetRenderSharedData().fontBackend;
			if (fontBackend)
			{
				const SdVec2 size = fontBackend->MeasureText(state.text, context.instance.GetRenderSharedData().defaultTextStyle);
				context.SetDesiredSize({ std::max(18.0f, size.x), std::max(22.0f, size.y) });
				return;
			}
			context.SetDesiredSize({ 12.0f + static_cast<float>(Utf8::DecodeToCodepoints(state.text).size()) * 7.5f, 22.0f });
		}

		void OnPaint(SdPaintContext& context)
		{
			const State& state = context.State<State>();
			context.renderList.AddText(state.text, context.animatedRect.min, SdApplyOpacity(context.style.color, context.opacity), context.clipRect);
		}
	};

	class SdPanel final : public SdWidgetTag
	{
	public:
		void OnLayout(SdLayoutContext& context)
		{
			context.widgetState.styleClass = SdStyleWidgetClass::Panel;
			context.SetDesiredSize({ 280.0f, 132.0f });
		}

		void OnPaint(SdPaintContext& context)
		{
			context.renderList.AddRectFilled(context.animatedRect, SdApplyOpacity(context.style.background, context.opacity), context.clipRect, context.style.radius);
			context.renderList.AddRect(context.animatedRect, SdApplyOpacity({ 86, 101, 124, 255 }, context.opacity), context.clipRect, 1.0f);
		}
	};

	class SdButton final : public SdWidgetTag
	{
	public:
		struct State
		{
			SdUtf8String text = {};
			bool hovered = false;
			bool pressed = false;
			bool clicked = false;
		};

		void OnUpdate(SdUpdateContext& context, SdUtf8StringView text)
		{
			State& state = context.State<State>();
			context.widgetState.styleClass = SdStyleWidgetClass::Button;
			state.text.assign(text.data(), text.size());
			state.hovered = context.widgetState.inputEnabled && context.IsHovered();
			state.pressed = context.IsPressed();
			state.clicked = context.WasClicked();
		}

		void OnUpdate(SdUpdateContext& context, SdUtf8StringView text, bool& clicked)
		{
			OnUpdate(context, text);
			clicked = context.State<State>().clicked;
		}

		void OnLayout(SdLayoutContext& context)
		{
			const State& state = context.State<State>();
			ISdFontBackend* fontBackend = context.instance.GetRenderSharedData().fontBackend;
			if (fontBackend)
			{
				const SdVec2 size = fontBackend->MeasureText(state.text, context.instance.GetRenderSharedData().defaultTextStyle);
				context.SetDesiredSize({ 32.0f + size.x, std::max(34.0f, size.y + 14.0f) });
				return;
			}
			context.SetDesiredSize({ 48.0f + static_cast<float>(Utf8::DecodeToCodepoints(state.text).size()) * 7.5f, 34.0f });
		}

		void OnPaint(SdPaintContext& context)
		{
			const State& state = context.State<State>();
			context.renderList.AddRectFilled(context.animatedRect, SdApplyOpacity(context.style.background, context.opacity), context.clipRect);
			context.renderList.AddRect(context.animatedRect, SdApplyOpacity({ 128, 154, 180, 255 }, context.opacity), context.clipRect, 1.0f);
			context.renderList.AddText(state.text, { context.animatedRect.min.x + 16.0f, context.animatedRect.min.y + 9.0f }, SdApplyOpacity(context.style.color, context.opacity), context.clipRect);
		}

	};

	class SdCheckBox final : public SdWidgetTag
	{
	public:
		struct State
		{
			bool checked = false;
			bool hovered = false;
		};

		void OnUpdate(SdUpdateContext& context, bool& value)
		{
			State& state = context.State<State>();
			const bool hovered = context.widgetState.inputEnabled && context.IsHovered();
			context.widgetState.styleClass = SdStyleWidgetClass::CheckBox;
			if (context.WasClicked())
				value = !value;
			state.checked = value;
			state.hovered = hovered;
		}

		void OnLayout(SdLayoutContext& context)
		{
			context.SetDesiredSize({ 34.0f, 28.0f });
		}

		void OnPaint(SdPaintContext& context)
		{
			const State& state = context.State<State>();
			const SdRect box = {
				context.animatedRect.min.x,
				context.animatedRect.min.y + 2.0f,
				context.animatedRect.min.x + 24.0f,
				context.animatedRect.min.y + 26.0f
			};
			context.renderList.AddRectFilled(box, SdApplyOpacity(state.hovered ? SdColor{ 62, 86, 108, 255 } : context.style.background, context.opacity), context.clipRect);
			context.renderList.AddRect(box, SdApplyOpacity({ 130, 146, 164, 255 }, context.opacity), context.clipRect, 1.0f);
			if (state.checked)
			{
				const SdRect mark = { box.min.x + 5.0f, box.min.y + 5.0f, box.max.x - 5.0f, box.max.y - 5.0f };
				context.renderList.AddRectFilled(mark, SdApplyOpacity(context.instance.GetStyleSystem().GetTheme().GetColor(SdStyleToken::ColorAccent), context.opacity), context.clipRect);
			}
		}
	};
}
