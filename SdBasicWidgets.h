#pragma once

#include "SdRuntime.h"
#include "SdUtf8.h"

#include <algorithm>
#include <cmath>
#include <type_traits>
#include <utility>

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
			context.renderList.AddRect(context.animatedRect, SdApplyOpacity(context.style.border, context.opacity), context.clipRect, 1.0f);
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
			context.renderList.AddRect(context.animatedRect, SdApplyOpacity(context.style.border, context.opacity), context.clipRect, 1.0f);
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
			context.renderList.AddRectFilled(box, SdApplyOpacity(context.style.background, context.opacity), context.clipRect, context.style.radius);
			context.renderList.AddRect(box, SdApplyOpacity(context.style.border, context.opacity), context.clipRect, 1.0f);
			if (state.checked)
			{
				const SdRect mark = { box.min.x + 5.0f, box.min.y + 5.0f, box.max.x - 5.0f, box.max.y - 5.0f };
				context.renderList.AddRectFilled(mark, SdApplyOpacity(context.instance.GetStyleSystem().GetTheme().GetColor(SdStyleToken::ColorAccent), context.opacity), context.clipRect);
			}
		}
	};

	class SdSliderFloat final : public SdWidgetTag
	{
	public:
		struct State
		{
			float value = 0.0f;
			float minimum = 0.0f;
			float maximum = 1.0f;
			float normalized = 0.0f;
			bool hovered = false;
			bool dragging = false;
		};

		void OnUpdate(SdUpdateContext& context, float& value, float minimum, float maximum)
		{
			State& state = context.State<State>();
			if (maximum < minimum)
				std::swap(minimum, maximum);
			if (maximum == minimum)
				maximum = minimum + 1.0f;

			context.widgetState.styleClass = SdStyleWidgetClass::Slider;
			state.minimum = minimum;
			state.maximum = maximum;
			state.hovered = context.widgetState.inputEnabled && context.IsHovered();
			state.dragging = context.IsPressed() || context.IsCaptured();

			if (state.dragging || context.WasPressed())
			{
				const SdRect rect = context.widgetState.lastRect.Width() > 0.0f
					? context.widgetState.lastRect
					: context.widgetState.targetRect;
				const float trackWidth = std::max(1.0f, rect.Width() - 18.0f);
				const float local = std::clamp((context.input.GetMousePosition().x - rect.min.x - 9.0f) / trackWidth, 0.0f, 1.0f);
				value = minimum + (maximum - minimum) * local;
			}

			value = std::clamp(value, minimum, maximum);
			state.value = value;
			state.normalized = (value - minimum) / (maximum - minimum);
		}

		void OnLayout(SdLayoutContext& context)
		{
			context.SetDesiredSize({ 180.0f, 28.0f });
		}

		void OnPaint(SdPaintContext& context)
		{
			const State& state = context.State<State>();
			const SdTheme& theme = context.instance.GetStyleSystem().GetTheme();
			const SdRect rect = context.animatedRect;
			const float centerY = (rect.min.y + rect.max.y) * 0.5f;
			const SdRect track = { rect.min.x + 8.0f, centerY - 3.0f, rect.max.x - 8.0f, centerY + 3.0f };
			const float knobX = track.min.x + track.Width() * std::clamp(state.normalized, 0.0f, 1.0f);
			const SdRect fill = { track.min.x, track.min.y, knobX, track.max.y };
			const SdRect knob = { knobX - 7.0f, centerY - 9.0f, knobX + 7.0f, centerY + 9.0f };

			context.renderList.AddRectFilled(track, SdApplyOpacity(context.style.background, context.opacity), context.clipRect, context.style.radius);
			context.renderList.AddRectFilled(fill, SdApplyOpacity(theme.GetColor(SdStyleToken::ColorAccent), context.opacity), context.clipRect, context.style.radius);
			context.renderList.AddRectFilled(knob, SdApplyOpacity(context.style.color, context.opacity), context.clipRect, 4.0f);
			context.renderList.AddRect(knob, SdApplyOpacity(context.style.border, context.opacity), context.clipRect, 1.0f, 4.0f);
		}
	};

	class SdTextInput final : public SdWidgetTag
	{
	public:
		struct Model
		{
			SdUtf8String value = {};
		};

		struct State
		{
			SdUtf8String text = {};
			SdUtf8String composition = {};
			SdSize caretByteOffset = 0;
			SdSize selectionStart = 0;
			SdSize selectionEnd = 0;
			SdSize selectionAnchor = 0;
			bool focused = false;
		};

		void OnUpdate(SdUpdateContext& context, SdUtf8String& value)
		{
			State& state = context.State<State>();
			context.widgetState.styleClass = SdStyleWidgetClass::TextInput;
			NormalizeSelection(state, value);

			if (context.WasPressed())
			{
				context.instance.GetInputSystem().ActivateTextInput(static_cast<SdTextInputTargetId>(context.id));
				const SdRect rect = context.widgetState.lastRect.Width() > 0.0f
					? context.widgetState.lastRect
					: context.widgetState.targetRect;
				state.caretByteOffset = ByteOffsetFromPoint(value, rect, context.input.GetMousePosition().x);
				state.selectionAnchor = state.caretByteOffset;
				ClearSelection(state);
			}

			state.focused = context.IsFocused();
			if (state.focused)
			{
				const bool extendSelection = (context.input.GetModifierMask() & SdModifierMask::Shift) == SdModifierMask::Shift;
				if (context.input.IsChordPressed(SdModifierMask::Ctrl, SdKeyCode::A))
				{
					state.selectionAnchor = 0;
					state.caretByteOffset = value.size();
					UpdateSelectionFromAnchor(state);
				}
				else
				{
					if (context.input.IsKeyDown(SdKeyCode::LeftArrow))
						MoveCaret(value, state, PreviousCodepointOffset(value, state.caretByteOffset), extendSelection);
					if (context.input.IsKeyDown(SdKeyCode::RightArrow))
						MoveCaret(value, state, NextCodepointOffset(value, state.caretByteOffset), extendSelection);
					if (context.input.IsKeyDown(SdKeyCode::Home))
						MoveCaret(value, state, 0, extendSelection);
					if (context.input.IsKeyDown(SdKeyCode::End))
						MoveCaret(value, state, value.size(), extendSelection);
				}

				if (context.input.IsKeyDown(SdKeyCode::Backspace) && !value.empty())
				{
					if (!DeleteSelection(value, state))
						ErasePreviousCodepoint(value, state);
				}
				if (context.input.IsKeyDown(SdKeyCode::Delete) && !value.empty())
				{
					if (!DeleteSelection(value, state))
						EraseNextCodepoint(value, state);
				}

				const SdUtf8StringView committed = context.input.GetCommittedText();
				if (!committed.empty() && Utf8::IsValid(committed))
					InsertText(value, state, committed);
			}

			state.text = value;
			state.composition = state.focused ? SdUtf8String(context.input.GetCompositionText()) : SdUtf8String{};
			NormalizeSelection(state, value);

			const SdRect rect = context.widgetState.lastRect.Width() > 0.0f
				? context.widgetState.lastRect
				: context.widgetState.targetRect;
			const SdRect caret = BuildCaretRect(rect, state.text, state.caretByteOffset);
			context.instance.GetInputSystem().SetTextInputTarget(static_cast<SdTextInputTargetId>(context.id), caret, std::max(18.0f, rect.Height()));
		}

		void OnUpdate(SdUpdateContext& context, SdUtf8StringView value)
		{
			State& state = context.State<State>();
			state.text.assign(value.data(), value.size());
			context.widgetState.styleClass = SdStyleWidgetClass::TextInput;
		}

		void OnLayout(SdLayoutContext& context)
		{
			context.SetDesiredSize({ 220.0f, 32.0f });
		}

		void OnPaint(SdPaintContext& context)
		{
			const State& state = context.State<State>();
			const SdRect rect = context.animatedRect;
			const SdTheme& theme = context.instance.GetStyleSystem().GetTheme();
			context.renderList.AddRectFilled(rect, SdApplyOpacity(context.style.background, context.opacity), context.clipRect, context.style.radius);
			context.renderList.AddRect(rect, SdApplyOpacity(state.focused ? theme.GetColor(SdStyleToken::ColorAccent) : context.style.border, context.opacity), context.clipRect, 1.0f, context.style.radius);
			if (state.focused && HasSelection(state))
			{
				const SdRect selection = BuildSelectionRect(rect, state.text, state.selectionStart, state.selectionEnd);
				context.renderList.AddRectFilled(selection, SdApplyOpacity(theme.GetColor(SdStyleToken::ColorSelection), context.opacity), context.clipRect, 1.0f);
			}
			context.renderList.AddText(state.text, { rect.min.x + 9.0f, rect.min.y + 8.0f }, SdApplyOpacity(context.style.color, context.opacity), context.clipRect);
			if (!state.composition.empty())
				context.renderList.AddText(state.composition, { rect.min.x + 9.0f, rect.min.y + 23.0f }, SdApplyOpacity(theme.GetColor(SdStyleToken::ColorAccent), context.opacity), context.clipRect);
			if (state.focused)
			{
				const SdRect caret = BuildCaretRect(rect, state.text, state.caretByteOffset);
				context.renderList.AddLine(caret.min, { caret.min.x, caret.max.y }, SdApplyOpacity(context.style.color, context.opacity), context.clipRect, 1.0f);
			}
		}

	private:
		static bool HasSelection(const State& state) noexcept
		{
			return state.selectionStart != state.selectionEnd;
		}

		static SdSize ClampToCodepointBoundary(SdUtf8StringView text, SdSize offset)
		{
			offset = std::min(offset, text.size());
			while (offset > 0 && offset < text.size() && (static_cast<unsigned char>(text[offset]) & 0xC0u) == 0x80u)
				--offset;
			return offset;
		}

		static SdSize PreviousCodepointOffset(SdUtf8StringView text, SdSize offset)
		{
			offset = ClampToCodepointBoundary(text, offset);
			if (offset == 0)
				return 0;
			--offset;
			while (offset > 0 && (static_cast<unsigned char>(text[offset]) & 0xC0u) == 0x80u)
				--offset;
			return offset;
		}

		static SdSize NextCodepointOffset(SdUtf8StringView text, SdSize offset)
		{
			offset = ClampToCodepointBoundary(text, offset);
			if (offset >= text.size())
				return text.size();
			SdUInt32 codepoint = 0;
			SdSize next = offset;
			Utf8::TryReadCodepoint(text, next, codepoint);
			return std::min(next, text.size());
		}

		static SdSize CountCodepointsBefore(SdUtf8StringView text, SdSize byteOffset)
		{
			const SdSize clampedOffset = ClampToCodepointBoundary(text, byteOffset);
			SdSize offset = 0;
			SdSize count = 0;
			while (offset < clampedOffset)
			{
				SdUInt32 codepoint = 0;
				Utf8::TryReadCodepoint(text, offset, codepoint);
				++count;
			}
			return count;
		}

		static SdSize ByteOffsetFromPoint(SdUtf8StringView text, const SdRect& rect, float x)
		{
			const float local = std::max(0.0f, x - rect.min.x - 9.0f);
			const SdSize targetCodepoint = static_cast<SdSize>(std::floor((local / 7.5f) + 0.5f));
			SdSize offset = 0;
			for (SdSize index = 0; index < targetCodepoint && offset < text.size(); ++index)
			{
				SdUInt32 codepoint = 0;
				Utf8::TryReadCodepoint(text, offset, codepoint);
			}
			return offset;
		}

		static void NormalizeSelection(State& state, SdUtf8StringView text)
		{
			state.caretByteOffset = ClampToCodepointBoundary(text, state.caretByteOffset);
			state.selectionAnchor = ClampToCodepointBoundary(text, state.selectionAnchor);
			state.selectionStart = ClampToCodepointBoundary(text, std::min(state.selectionStart, text.size()));
			state.selectionEnd = ClampToCodepointBoundary(text, std::min(state.selectionEnd, text.size()));
			if (state.selectionStart > state.selectionEnd)
				std::swap(state.selectionStart, state.selectionEnd);
		}

		static void ClearSelection(State& state)
		{
			state.selectionStart = state.caretByteOffset;
			state.selectionEnd = state.caretByteOffset;
			state.selectionAnchor = state.caretByteOffset;
		}

		static void UpdateSelectionFromAnchor(State& state)
		{
			state.selectionStart = std::min(state.selectionAnchor, state.caretByteOffset);
			state.selectionEnd = std::max(state.selectionAnchor, state.caretByteOffset);
		}

		static void MoveCaret(SdUtf8StringView text, State& state, SdSize byteOffset, bool extendSelection)
		{
			state.caretByteOffset = ClampToCodepointBoundary(text, byteOffset);
			if (extendSelection)
			{
				UpdateSelectionFromAnchor(state);
				return;
			}
			ClearSelection(state);
		}

		static bool DeleteSelection(SdUtf8String& text, State& state)
		{
			if (!HasSelection(state))
				return false;
			text.erase(state.selectionStart, state.selectionEnd - state.selectionStart);
			state.caretByteOffset = state.selectionStart;
			ClearSelection(state);
			return true;
		}

		static void InsertText(SdUtf8String& text, State& state, SdUtf8StringView insertText)
		{
			DeleteSelection(text, state);
			text.insert(state.caretByteOffset, insertText.data(), insertText.size());
			state.caretByteOffset += insertText.size();
			ClearSelection(state);
		}

		static void ErasePreviousCodepoint(SdUtf8String& text, State& state)
		{
			if (text.empty() || state.caretByteOffset == 0)
				return;
			const SdSize previous = PreviousCodepointOffset(text, state.caretByteOffset);
			text.erase(previous, state.caretByteOffset - previous);
			state.caretByteOffset = previous;
			ClearSelection(state);
		}

		static void EraseNextCodepoint(SdUtf8String& text, State& state)
		{
			if (text.empty() || state.caretByteOffset >= text.size())
				return;
			const SdSize next = NextCodepointOffset(text, state.caretByteOffset);
			text.erase(state.caretByteOffset, next - state.caretByteOffset);
			ClearSelection(state);
		}

		static SdRect BuildCaretRect(const SdRect& rect, SdUtf8StringView text, SdSize byteOffset)
		{
			const float x = rect.min.x + 9.0f + static_cast<float>(CountCodepointsBefore(text, byteOffset)) * 7.5f;
			return { x, rect.min.y + 7.0f, x + 1.0f, rect.max.y - 7.0f };
		}

		static SdRect BuildSelectionRect(const SdRect& rect, SdUtf8StringView text, SdSize start, SdSize end)
		{
			const SdRect startCaret = BuildCaretRect(rect, text, start);
			const SdRect endCaret = BuildCaretRect(rect, text, end);
			return {
				startCaret.min.x,
				rect.min.y + 6.0f,
				std::max(startCaret.min.x, endCaret.min.x),
				rect.max.y - 6.0f
			};
		}
	};

	struct SdScrollViewOptions final
	{
		SdVec2 size = { 260.0f, 160.0f };
		SdSpacing padding = { 8.0f, 8.0f, 12.0f, 8.0f };
		float itemSpacing = 6.0f;
		float wheelStep = 24.0f;
	};

	class SdScrollView final : public SdWidgetTag
	{
	public:
		struct Model
		{
			float scrollY = 0.0f;
		};

		struct State
		{
			SdScrollViewOptions options = {};
			float scrollY = 0.0f;
			bool hovered = false;
		};

		void OnUpdate(SdUpdateContext& context)
		{
			UpdateScrollView(context, SdScrollViewOptions{});
		}

		void OnUpdate(SdUpdateContext& context, const SdScrollViewOptions& options)
		{
			UpdateScrollView(context, options);
		}

		template<class TContent>
			requires std::is_invocable_v<TContent&, SdUi&>
		void OnUpdate(SdUpdateContext& context, TContent&& content)
		{
			UpdateScrollView(context, SdScrollViewOptions{});
			std::forward<TContent>(content)(context.ui);
		}

		template<class TContent>
			requires std::is_invocable_v<TContent&, SdUi&>
		void OnUpdate(SdUpdateContext& context, const SdScrollViewOptions& options, TContent&& content)
		{
			UpdateScrollView(context, options);
			std::forward<TContent>(content)(context.ui);
		}

		void OnLayout(SdLayoutContext& context)
		{
			const State& state = context.State<State>();
			context.SetDesiredSize(state.options.size);
			context.widgetState.arrangeChildren = true;
			context.widgetState.clipChildren = true;
			context.widgetState.childPadding = state.options.padding;
			context.widgetState.childPadding.top -= state.scrollY;
			context.widgetState.childSpacing = state.options.itemSpacing;
		}

		void OnPaint(SdPaintContext& context)
		{
			const State& state = context.State<State>();
			const SdRect rect = context.animatedRect;
			context.renderList.AddRectFilled(rect, SdApplyOpacity(context.style.background, context.opacity), context.clipRect, context.style.radius);
			context.renderList.AddRect(rect, SdApplyOpacity(context.style.border, context.opacity), context.clipRect, 1.0f, context.style.radius);
			const float thumbHeight = std::max(18.0f, rect.Height() * 0.35f);
			const float thumbTop = rect.min.y + 5.0f + std::fmod(std::max(0.0f, state.scrollY), std::max(1.0f, rect.Height() - thumbHeight - 10.0f));
			const SdRect thumb = { rect.max.x - 8.0f, thumbTop, rect.max.x - 4.0f, thumbTop + thumbHeight };
			context.renderList.AddRectFilled(thumb, SdApplyOpacity(context.instance.GetStyleSystem().GetTheme().GetColor(SdStyleToken::ColorAccent), context.opacity), context.clipRect, 2.0f);
		}

	private:
		void UpdateScrollView(SdUpdateContext& context, SdScrollViewOptions options)
		{
			State& state = context.State<State>();
			Model& model = context.HasModelKey() ? context.Model<Model>() : context.State<Model>();
			options.size.x = std::max(1.0f, options.size.x);
			options.size.y = std::max(1.0f, options.size.y);
			options.itemSpacing = std::max(0.0f, options.itemSpacing);
			options.wheelStep = std::max(1.0f, options.wheelStep);
			context.widgetState.styleClass = SdStyleWidgetClass::ScrollView;
			state.options = options;
			state.hovered = context.widgetState.inputEnabled && context.IsHovered();
			if (state.hovered)
				model.scrollY = std::max(0.0f, model.scrollY - context.input.GetMouseWheelDelta().y * options.wheelStep);
			state.scrollY = model.scrollY;
		}
	};

	struct SdPopupOptions final
	{
		SdVec2 position = { 64.0f, 64.0f };
		SdVec2 size = { 220.0f, 120.0f };
		SdSpacing padding = { 8.0f, 8.0f, 8.0f, 8.0f };
		float itemSpacing = 6.0f;
	};

	class SdPopup final : public SdWidgetTag
	{
	public:
		struct State
		{
			bool open = false;
			SdPopupOptions options = {};
		};

		void OnUpdate(SdUpdateContext& context, bool open)
		{
			UpdatePopup(context, open, SdPopupOptions{});
		}

		void OnUpdate(SdUpdateContext& context, bool open, const SdPopupOptions& options)
		{
			UpdatePopup(context, open, options);
		}

		template<class TContent>
			requires std::is_invocable_v<TContent&, SdUi&>
		void OnUpdate(SdUpdateContext& context, bool open, TContent&& content)
		{
			if (UpdatePopup(context, open, SdPopupOptions{}))
				std::forward<TContent>(content)(context.ui);
		}

		template<class TContent>
			requires std::is_invocable_v<TContent&, SdUi&>
		void OnUpdate(SdUpdateContext& context, bool open, const SdPopupOptions& options, TContent&& content)
		{
			if (UpdatePopup(context, open, options))
				std::forward<TContent>(content)(context.ui);
		}

		void OnLayout(SdLayoutContext& context)
		{
			const State& state = context.State<State>();
			const SdVec2 size = state.open ? state.options.size : SdVec2{};
			context.SetDesiredSize(size);
			context.widgetState.manualLayout = true;
			context.widgetState.manualRect = { state.options.position, state.options.position + size };
			context.widgetState.arrangeChildren = true;
			context.widgetState.clipChildren = true;
			context.widgetState.childPadding = state.options.padding;
			context.widgetState.childSpacing = state.options.itemSpacing;
		}

		void OnPaint(SdPaintContext& context)
		{
			const State& state = context.State<State>();
			if (!state.open)
				return;
			context.renderList.AddRectFilled(context.animatedRect, SdApplyOpacity(context.style.background, context.opacity), context.clipRect, context.style.radius);
			context.renderList.AddRect(context.animatedRect, SdApplyOpacity(context.style.border, context.opacity), context.clipRect, 1.0f, context.style.radius);
		}

	protected:
		bool UpdatePopup(SdUpdateContext& context, bool open, SdPopupOptions options)
		{
			State& state = context.State<State>();
			options.size.x = std::max(1.0f, options.size.x);
			options.size.y = std::max(1.0f, options.size.y);
			options.itemSpacing = std::max(0.0f, options.itemSpacing);
			state.open = open;
			state.options = options;
			context.widgetState.layerPriority = SdLayerPriority::Popup;
			context.widgetState.styleClass = SdStyleWidgetClass::Popup;
			context.widgetState.inputEnabled = open;
			return open;
		}
	};

	class SdContextMenu final : public SdWidgetTag
	{
	public:
		struct State
		{
			bool open = false;
			SdPopupOptions options = {};
		};

		template<class TContent>
			requires std::is_invocable_v<TContent&, SdUi&>
		void OnUpdate(SdUpdateContext& context, bool open, const SdPopupOptions& options, TContent&& content)
		{
			if (UpdateContextMenu(context, open, options))
				std::forward<TContent>(content)(context.ui);
		}

		void OnUpdate(SdUpdateContext& context, bool open, const SdPopupOptions& options)
		{
			UpdateContextMenu(context, open, options);
		}

		void OnLayout(SdLayoutContext& context)
		{
			const State& state = context.State<State>();
			const SdVec2 size = state.open ? state.options.size : SdVec2{};
			context.SetDesiredSize(size);
			context.widgetState.manualLayout = true;
			context.widgetState.manualRect = { state.options.position, state.options.position + size };
			context.widgetState.arrangeChildren = true;
			context.widgetState.clipChildren = true;
			context.widgetState.childPadding = state.options.padding;
			context.widgetState.childSpacing = state.options.itemSpacing;
		}

		void OnPaint(SdPaintContext& context)
		{
			if (!context.State<State>().open)
				return;
			context.renderList.AddRectFilled(context.animatedRect, SdApplyOpacity(context.style.background, context.opacity), context.clipRect, context.style.radius);
			context.renderList.AddRect(context.animatedRect, SdApplyOpacity(context.style.border, context.opacity), context.clipRect, 1.0f, context.style.radius);
		}

	private:
		bool UpdateContextMenu(SdUpdateContext& context, bool open, SdPopupOptions options)
		{
			State& state = context.State<State>();
			state.open = open;
			state.options = options;
			context.widgetState.layerPriority = SdLayerPriority::Popup;
			context.widgetState.styleClass = SdStyleWidgetClass::ContextMenu;
			context.widgetState.inputEnabled = open;
			return open;
		}
	};

	class SdTooltip final : public SdWidgetTag
	{
	public:
		struct State
		{
			SdUtf8String text = {};
			SdVec2 position = {};
			bool visible = false;
		};

		void OnUpdate(SdUpdateContext& context, bool visible, SdUtf8StringView text)
		{
			State& state = context.State<State>();
			state.visible = visible;
			state.text.assign(text.data(), text.size());
			state.position = context.input.GetMousePosition() + SdVec2{ 14.0f, 16.0f };
			context.widgetState.layerPriority = SdLayerPriority::Overlay;
			context.widgetState.styleClass = SdStyleWidgetClass::Tooltip;
			context.widgetState.inputEnabled = false;
		}

		void OnLayout(SdLayoutContext& context)
		{
			const State& state = context.State<State>();
			const SdVec2 size = state.visible
				? SdVec2{ 18.0f + static_cast<float>(Utf8::DecodeToCodepoints(state.text).size()) * 7.5f, 28.0f }
				: SdVec2{};
			context.SetDesiredSize(size);
			context.widgetState.manualLayout = true;
			context.widgetState.manualRect = { state.position, state.position + size };
		}

		void OnPaint(SdPaintContext& context)
		{
			const State& state = context.State<State>();
			if (!state.visible)
				return;
			context.renderList.AddRectFilled(context.animatedRect, SdApplyOpacity(context.style.background, context.opacity), context.clipRect, context.style.radius);
			context.renderList.AddRect(context.animatedRect, SdApplyOpacity(context.style.border, context.opacity), context.clipRect, 1.0f, context.style.radius);
			context.renderList.AddText(state.text, { context.animatedRect.min.x + 9.0f, context.animatedRect.min.y + 7.0f }, SdApplyOpacity(context.style.color, context.opacity), context.clipRect);
		}
	};
}
