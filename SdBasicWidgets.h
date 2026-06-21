#pragma once

#include "SdRuntime.h"
#include "SdText.h"
#include "SdUtf8.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <type_traits>
#include <utility>

namespace Sodium
{
	namespace BasicWidgetDetail
	{
		inline constexpr float kDefaultFontSize = 16.0f;
		inline constexpr float kDefaultLineHeight = 20.0f;

		inline SdColor ApplyOpacity(SdColor color, float opacity) noexcept
		{
			const float clampedOpacity = std::clamp(opacity, 0.0f, 1.0f);
			color.a = static_cast<SdUInt8>(static_cast<float>(color.a) * clampedOpacity);
			return color;
		}

		inline SdRect InsetRect(const SdRect& rect, const SdSpacing& padding) noexcept
		{
			SdRect result = {
				rect.min.x + padding.left,
				rect.min.y + padding.top,
				rect.max.x - padding.right,
				rect.max.y - padding.bottom
			};
			result.max.x = std::max(result.min.x, result.max.x);
			result.max.y = std::max(result.min.y, result.max.y);
			return result;
		}

		inline SdRect OffsetRect(const SdRect& rect, const SdVec2& offset) noexcept
		{
			return {
				rect.min.x + offset.x,
				rect.min.y + offset.y,
				rect.max.x + offset.x,
				rect.max.y + offset.y
			};
		}

		inline float ResolveLineHeight(const SdTextStyle& textStyle) noexcept
		{
			return textStyle.lineHeight > 0.0f ? textStyle.lineHeight : textStyle.pixelSize;
		}

		inline SdTextStyle BuildTextStyle(const SdTextStyle& declaredStyle, float fontSize, float lineHeight)
		{
			SdTextStyle result = declaredStyle;
			result.pixelSize = fontSize > 0.0f ? fontSize : (declaredStyle.pixelSize > 0.0f ? declaredStyle.pixelSize : kDefaultFontSize);
			result.lineHeight = lineHeight > 0.0f ? lineHeight : declaredStyle.lineHeight;
			return result;
		}

		inline SdSize CountCodepoints(SdUtf8StringView text)
		{
			SdSize count = 0;
			SdSize offset = 0;
			while (offset < text.size())
			{
				SdUInt32 codepoint = 0;
				Utf8::TryReadCodepoint(text, offset, codepoint);
				++count;
			}
			return count;
		}

		inline SdVec2 EstimateTextSize(SdUtf8StringView text, const SdTextStyle& textStyle)
		{
			const float width = static_cast<float>(CountCodepoints(text)) * textStyle.pixelSize * 0.5f;
			return { width, ResolveLineHeight(textStyle) };
		}

		inline SdVec2 MeasureText(SdInstance& instance, SdUtf8StringView text, const SdTextStyle& textStyle)
		{
			if (ISdFontBackend* fontBackend = instance.GetFontBackend())
				return fontBackend->MeasureText(text, textStyle);
			return EstimateTextSize(text, textStyle);
		}

		inline SdVec2 MeasureText(SdLayoutContext& context, SdUtf8StringView text, const SdTextStyle& textStyle)
		{
			return MeasureText(context.instance, text, textStyle);
		}

		inline SdVec2 MeasureText(SdUpdateContext& context, SdUtf8StringView text, const SdTextStyle& textStyle)
		{
			return MeasureText(context.instance, text, textStyle);
		}

		inline void PopBackUtf8(SdUtf8String& text)
		{
			if (text.empty())
				return;

			SdSize offset = text.size() - 1;
			while (offset > 0 && Utf8::IsContinuation(static_cast<unsigned char>(text[offset])))
				--offset;
			text.erase(offset);
		}

		inline SdTextInputTargetId MakeTextInputTargetId(SdWidgetId id) noexcept
		{
			SdTextInputTargetId result = static_cast<SdTextInputTargetId>((id & 0xFFFFFFFFull) ^ (id >> 32ull));
			return result == 0 ? 1u : result;
		}

		inline SdRect MakeRect(SdVec2 position, SdVec2 size) noexcept
		{
			return { position.x, position.y, position.x + size.x, position.y + size.y };
		}

		inline float Normalize(float value, float minValue, float maxValue) noexcept
		{
			if (maxValue == minValue)
				return 0.0f;
			if (maxValue < minValue)
				std::swap(maxValue, minValue);
			return std::clamp((value - minValue) / (maxValue - minValue), 0.0f, 1.0f);
		}

		inline float Lerp(float a, float b, float t) noexcept
		{
			return a + ((b - a) * t);
		}
	}

	struct SdText final : SdWidgetTag
	{
		struct Style final
		{
			static constexpr SdStyleTokenTag TokenTag = SdStyleTargetTags::Text;
			static constexpr SdStyleTokenTag Color = SdStylePropertyTags::Color;
			static constexpr SdStyleTokenTag Opacity = SdStylePropertyTags::Opacity;
			SdSpacing padding = {};
			float fontSize = 16.0f;
			float lineHeight = 0.0f;
			SdColor color = SdColorWhite;
			float opacity = 1.0f;

			static Style Default(const SdStyleContext& context)
			{
				Style style = {};
				style.fontSize = 16.0f;
				style.lineHeight = 0.0f;
				style.color = context.theme.GetColor(SdStyleToken::ColorText);
				style.opacity = 1.0f;
				return style;
			}

			static void Describe(SdStyleContract<Style>& contract)
			{
				contract.Layout(&Style::padding);
				contract.Layout(&Style::fontSize);
				contract.Layout(&Style::lineHeight);
				contract.Paint(&Style::color).InterpolatesAsColor();
				contract.Composite(&Style::opacity).InterpolatesAsFloat();
			}
		};

		struct Model final
		{
			std::optional<SdUtf8String> text = {};

			void SetText(SdUtf8StringView value)
			{
				text.emplace(value.data(), value.size());
			}

			void ClearText() noexcept
			{
				text.reset();
			}

			SdUtf8StringView ResolveText(SdUtf8StringView fallback) const noexcept
			{
				if (!text)
					return fallback;
				return SdUtf8StringView(text->data(), text->size());
			}
		};

		struct State final
		{
			SdUtf8String text = {};
			SdTextStyle textStyle = {};
		};

		void OnUpdate(SdUpdateContext& context)
		{
			OnUpdate(context, {});
		}

		void OnUpdate(SdUpdateContext& context, SdUtf8StringView text)
		{
			OnUpdate(context, text, {});
		}

		void OnUpdate(SdUpdateContext& context, SdUtf8StringView text, const SdTextStyle& textStyle)
		{
			State& state = context.State<State>();
			SdUtf8StringView resolvedText = text;
			if (context.HasModelKey())
				resolvedText = context.Model<Model>().ResolveText(text);

			state.text.assign(resolvedText.data(), resolvedText.size());
			state.textStyle = textStyle;
			context.widgetState.styleTokenTag = Style::TokenTag;
			context.widgetState.inputEnabled = false;
			context.widgetState.layoutWeight = 1.0f;
		}

		void OnLayout(SdLayoutContext& context)
		{
			const State& state = context.State<State>();
			const Style& style = context.TargetStyle<SdText>();
			const SdTextStyle textStyle = BasicWidgetDetail::BuildTextStyle(state.textStyle, style.fontSize, style.lineHeight);
			SdVec2 desiredSize = BasicWidgetDetail::MeasureText(context, state.text, textStyle);

			desiredSize.y = std::max(desiredSize.y, BasicWidgetDetail::ResolveLineHeight(textStyle));
			desiredSize.x += style.padding.left + style.padding.right;
			desiredSize.y += style.padding.top + style.padding.bottom;
			if (context.constraints.maxSize.x > 0.0f)
				desiredSize.x = std::min(desiredSize.x, context.constraints.maxSize.x);
			if (context.constraints.maxSize.y > 0.0f)
				desiredSize.y = std::min(desiredSize.y, context.constraints.maxSize.y);
			context.SetDesiredSize(desiredSize);
		}

		void OnPaint(SdPaintContext& context)
		{
			const State& state = context.State<State>();
			if (state.text.empty())
				return;

			const Style& style = context.ComputedStyle<SdText>();
			const SdTextStyle textStyle = BasicWidgetDetail::BuildTextStyle(state.textStyle, style.fontSize, style.lineHeight);
			const SdColor color = BasicWidgetDetail::ApplyOpacity(
				style.color,
				context.opacity * style.opacity);
			const SdVec2 position = {
				context.animatedRect.min.x + style.padding.left,
				context.animatedRect.min.y + style.padding.top
			};
			context.renderList.AddText(state.text, textStyle, position, color, context.clipRect);
		}
	};

	struct SdPanel final : SdWidgetTag
	{
		struct Style final
		{
			static constexpr SdStyleTokenTag TokenTag = SdStyleTargetTags::Panel;
			SdSpacing padding = { 10.0f, 10.0f, 10.0f, 10.0f };
			float width = 240.0f;
			float height = 120.0f;
			float childSpacing = 6.0f;
			float radius = 5.0f;
			float opacity = 1.0f;

			static Style Default(const SdStyleContext& context)
			{
				Style style = {};
				style.padding = {
					context.theme.GetMetric(SdStyleToken::SpacingMedium),
					context.theme.GetMetric(SdStyleToken::SpacingMedium),
					context.theme.GetMetric(SdStyleToken::SpacingMedium),
					context.theme.GetMetric(SdStyleToken::SpacingMedium)
				};
				style.width = 240.0f;
				style.height = 120.0f;
				style.childSpacing = context.theme.GetMetric(SdStyleToken::SpacingSmall);
				style.radius = context.theme.GetMetric(SdStyleToken::RadiusSmall);
				style.opacity = 1.0f;
				return style;
			}

			static void Describe(SdStyleContract<Style>& contract)
			{
				contract.Layout(&Style::padding);
				contract.Layout(&Style::width);
				contract.Layout(&Style::height);
				contract.Layout(&Style::childSpacing);
				contract.Paint(&Style::radius).InterpolatesAsFloat();
				contract.Composite(&Style::opacity).InterpolatesAsFloat();
			}
		};

		void OnUpdate(SdUpdateContext& context)
		{
			Configure(context);
		}

		template<class TContent>
			requires std::is_invocable_v<TContent&, SdUi&>
		void OnUpdate(SdUpdateContext& context, TContent&& content)
		{
			Configure(context);
			std::forward<TContent>(content)(context.ui);
		}

		void OnLayout(SdLayoutContext& context)
		{
			const Style& style = context.TargetStyle<SdPanel>();
			context.SetDesiredSize({
				std::max(0.0f, style.width),
				std::max(0.0f, style.height)
			});
			context.widgetState.arrangeChildren = true;
			context.widgetState.clipChildren = true;
			context.widgetState.childPadding = style.padding;
			context.widgetState.childSpacing = std::max(0.0f, style.childSpacing);
		}

		void OnPaint(SdPaintContext& context)
		{
			const Style& style = context.ComputedStyle<SdPanel>();
			context.renderList.AddRectFilled(
				context.animatedRect,
				BasicWidgetDetail::ApplyOpacity(context.style.background, context.opacity * style.opacity),
				context.clipRect,
				style.radius);
			context.renderList.AddRect(
				context.animatedRect,
				BasicWidgetDetail::ApplyOpacity(context.style.border, context.opacity * style.opacity),
				context.clipRect,
				1.0f,
				style.radius);
		}

	private:
		static void Configure(SdUpdateContext& context)
		{
			context.widgetState.styleTokenTag = Style::TokenTag;
			context.widgetState.inputEnabled = false;
			context.widgetState.layoutWeight = 1.0f;
		}
	};

	struct SdButton final : SdWidgetTag
	{
		struct Style final
		{
			static constexpr SdStyleTokenTag TokenTag = SdStyleTargetTags::Button;
			SdSpacing padding = { 10.0f, 6.0f, 10.0f, 6.0f };
			float minWidth = 82.0f;
			float minHeight = 30.0f;
			float fontSize = 16.0f;
			float lineHeight = 0.0f;
			float radius = 5.0f;
			float opacity = 1.0f;

			static Style Default(const SdStyleContext& context)
			{
				Style style = {};
				const float smallSpacing = context.theme.GetMetric(SdStyleToken::SpacingSmall);
				const float mediumSpacing = context.theme.GetMetric(SdStyleToken::SpacingMedium);
				style.padding = { mediumSpacing, smallSpacing, mediumSpacing, smallSpacing };
				style.minWidth = 82.0f;
				style.minHeight = 30.0f;
				style.fontSize = BasicWidgetDetail::kDefaultFontSize;
				style.lineHeight = 0.0f;
				style.radius = context.theme.GetMetric(SdStyleToken::RadiusSmall);
				style.opacity = 1.0f;
				return style;
			}

			static void Describe(SdStyleContract<Style>& contract)
			{
				contract.Layout(&Style::padding);
				contract.Layout(&Style::minWidth);
				contract.Layout(&Style::minHeight);
				contract.Layout(&Style::fontSize);
				contract.Layout(&Style::lineHeight);
				contract.Paint(&Style::radius).InterpolatesAsFloat();
				contract.Composite(&Style::opacity).InterpolatesAsFloat();
			}
		};

		struct State final
		{
			SdUtf8String label = {};
			bool clicked = false;
		};

		bool clickedThisFrame = false;

		bool WasClicked() const noexcept
		{
			return clickedThisFrame;
		}

		void OnUpdate(SdUpdateContext& context, SdUtf8StringView label)
		{
			State& state = context.State<State>();
			state.label.assign(label.data(), label.size());
			state.clicked = context.WasClicked();
			clickedThisFrame = state.clicked;
			Configure(context);
		}

		void OnUpdate(SdUpdateContext& context, SdUtf8StringView label, bool& clicked)
		{
			OnUpdate(context, label);
			clicked = clickedThisFrame;
		}

		void OnLayout(SdLayoutContext& context)
		{
			const State& state = context.State<State>();
			const Style& style = context.TargetStyle<SdButton>();
			const SdTextStyle textStyle = BasicWidgetDetail::BuildTextStyle({}, style.fontSize, style.lineHeight);
			SdVec2 textSize = BasicWidgetDetail::MeasureText(context, state.label, textStyle);
			textSize.y = std::max(textSize.y, BasicWidgetDetail::ResolveLineHeight(textStyle));

			context.SetDesiredSize({
				std::max(style.minWidth, textSize.x + style.padding.left + style.padding.right),
				std::max(style.minHeight, textSize.y + style.padding.top + style.padding.bottom)
			});
		}

		void OnPaint(SdPaintContext& context)
		{
			const State& state = context.State<State>();
			const Style& style = context.ComputedStyle<SdButton>();
			const SdTextStyle textStyle = BasicWidgetDetail::BuildTextStyle({}, style.fontSize, style.lineHeight);
			const float lineHeight = BasicWidgetDetail::ResolveLineHeight(textStyle);
			const SdVec2 textSize = BasicWidgetDetail::MeasureText(context.instance, state.label, textStyle);
			const SdColor background = BasicWidgetDetail::ApplyOpacity(context.style.background, context.opacity * style.opacity);
			const SdColor border = BasicWidgetDetail::ApplyOpacity(context.style.border, context.opacity * style.opacity);
			const SdColor color = BasicWidgetDetail::ApplyOpacity(context.style.color, context.opacity * style.opacity);

			context.renderList.AddRectFilled(context.animatedRect, background, context.clipRect, style.radius);
			context.renderList.AddRect(context.animatedRect, border, context.clipRect, 1.0f, style.radius);
			const SdVec2 position = {
				context.animatedRect.min.x + std::max(style.padding.left, (context.animatedRect.Width() - textSize.x) * 0.5f),
				context.animatedRect.min.y + std::max(style.padding.top, (context.animatedRect.Height() - lineHeight) * 0.5f)
			};
			context.renderList.AddText(state.label, textStyle, position, color, context.clipRect);
		}

	private:
		static void Configure(SdUpdateContext& context)
		{
			context.widgetState.styleTokenTag = Style::TokenTag;
			context.widgetState.inputEnabled = true;
			context.widgetState.layoutWeight = 1.0f;
		}
	};

	struct SdCheckBox final : SdWidgetTag
	{
		struct Style final
		{
			static constexpr SdStyleTokenTag TokenTag = SdStyleTargetTags::CheckBox;
			SdSpacing padding = { 8.0f, 5.0f, 8.0f, 5.0f };
			float boxSize = 18.0f;
			float gap = 8.0f;
			float minHeight = 28.0f;
			float fontSize = 16.0f;
			float lineHeight = 0.0f;
			float radius = 4.0f;
			float opacity = 1.0f;

			static Style Default(const SdStyleContext& context)
			{
				Style style = {};
				const float smallSpacing = context.theme.GetMetric(SdStyleToken::SpacingSmall);
				style.padding = { smallSpacing, smallSpacing, smallSpacing, smallSpacing };
				style.boxSize = 18.0f;
				style.gap = smallSpacing;
				style.minHeight = 28.0f;
				style.fontSize = BasicWidgetDetail::kDefaultFontSize;
				style.lineHeight = 0.0f;
				style.radius = std::max(2.0f, context.theme.GetMetric(SdStyleToken::RadiusSmall) - 1.0f);
				style.opacity = 1.0f;
				return style;
			}

			static void Describe(SdStyleContract<Style>& contract)
			{
				contract.Layout(&Style::padding);
				contract.Layout(&Style::boxSize);
				contract.Layout(&Style::gap);
				contract.Layout(&Style::minHeight);
				contract.Layout(&Style::fontSize);
				contract.Layout(&Style::lineHeight);
				contract.Paint(&Style::radius).InterpolatesAsFloat();
				contract.Composite(&Style::opacity).InterpolatesAsFloat();
			}
		};

		struct State final
		{
			SdUtf8String label = {};
			bool checked = false;
			bool changed = false;
		};

		bool changedThisFrame = false;

		bool Changed() const noexcept
		{
			return changedThisFrame;
		}

		void OnUpdate(SdUpdateContext& context, SdUtf8StringView label)
		{
			State& state = context.State<State>();
			state.label.assign(label.data(), label.size());
			state.changed = false;
			if (context.WasClicked())
			{
				state.checked = !state.checked;
				state.changed = true;
			}
			changedThisFrame = state.changed;
			Configure(context);
		}

		void OnUpdate(SdUpdateContext& context, SdUtf8StringView label, bool& checked)
		{
			State& state = context.State<State>();
			state.checked = checked;
			OnUpdate(context, label);
			checked = state.checked;
		}

		void OnUpdate(SdUpdateContext& context, bool& checked)
		{
			OnUpdate(context, {}, checked);
		}

		void OnLayout(SdLayoutContext& context)
		{
			const State& state = context.State<State>();
			const Style& style = context.TargetStyle<SdCheckBox>();
			const SdTextStyle textStyle = BasicWidgetDetail::BuildTextStyle({}, style.fontSize, style.lineHeight);
			SdVec2 textSize = BasicWidgetDetail::MeasureText(context, state.label, textStyle);
			textSize.y = std::max(textSize.y, BasicWidgetDetail::ResolveLineHeight(textStyle));

			context.SetDesiredSize({
				style.padding.left + style.boxSize + style.gap + textSize.x + style.padding.right,
				std::max(style.minHeight, std::max(style.boxSize, textSize.y) + style.padding.top + style.padding.bottom)
			});
		}

		void OnPaint(SdPaintContext& context)
		{
			const State& state = context.State<State>();
			const Style& style = context.ComputedStyle<SdCheckBox>();
			const SdTextStyle textStyle = BasicWidgetDetail::BuildTextStyle({}, style.fontSize, style.lineHeight);
			const float lineHeight = BasicWidgetDetail::ResolveLineHeight(textStyle);
			const float boxY = context.animatedRect.min.y + (context.animatedRect.Height() - style.boxSize) * 0.5f;
			const SdRect boxRect = {
				context.animatedRect.min.x + style.padding.left,
				boxY,
				context.animatedRect.min.x + style.padding.left + style.boxSize,
				boxY + style.boxSize
			};
			const SdColor background = BasicWidgetDetail::ApplyOpacity(context.style.background, context.opacity * style.opacity);
			const SdColor border = BasicWidgetDetail::ApplyOpacity(context.style.border, context.opacity * style.opacity);
			const SdColor textColor = BasicWidgetDetail::ApplyOpacity(context.style.color, context.opacity * style.opacity);
			const SdColor accent = BasicWidgetDetail::ApplyOpacity(
				context.instance.GetStyleSystem().GetTheme().GetColor(SdStyleToken::ColorAccent),
				context.opacity * style.opacity);

			context.renderList.AddRectFilled(boxRect, background, context.clipRect, style.radius);
			context.renderList.AddRect(boxRect, border, context.clipRect, 1.0f, style.radius);
			if (state.checked)
			{
				context.renderList.AddRectFilled(
					BasicWidgetDetail::InsetRect(boxRect, { 4.0f, 4.0f, 4.0f, 4.0f }),
					accent,
					context.clipRect,
					std::max(0.0f, style.radius - 2.0f));
			}

			const SdVec2 textPosition = {
				boxRect.max.x + style.gap,
				context.animatedRect.min.y + std::max(style.padding.top, (context.animatedRect.Height() - lineHeight) * 0.5f)
			};
			context.renderList.AddText(state.label, textStyle, textPosition, textColor, context.clipRect);
		}

	private:
		static void Configure(SdUpdateContext& context)
		{
			context.widgetState.styleTokenTag = Style::TokenTag;
			context.widgetState.inputEnabled = true;
			context.widgetState.layoutWeight = 1.0f;
		}
	};

	using SdCheckbox = SdCheckBox;

	struct SdSliderFloat final : SdWidgetTag
	{
		struct Style final
		{
			static constexpr SdStyleTokenTag TokenTag = SdStyleTargetTags::Slider;
			SdSpacing padding = { 8.0f, 8.0f, 8.0f, 8.0f };
			float width = 180.0f;
			float height = 30.0f;
			float trackHeight = 6.0f;
			float thumbRadius = 8.0f;
			float labelGap = 8.0f;
			float fontSize = 16.0f;
			float lineHeight = 0.0f;
			float radius = 5.0f;
			float opacity = 1.0f;

			static Style Default(const SdStyleContext& context)
			{
				Style style = {};
				const float smallSpacing = context.theme.GetMetric(SdStyleToken::SpacingSmall);
				style.padding = { smallSpacing, smallSpacing, smallSpacing, smallSpacing };
				style.width = 180.0f;
				style.height = 30.0f;
				style.trackHeight = 6.0f;
				style.thumbRadius = 8.0f;
				style.labelGap = smallSpacing;
				style.fontSize = BasicWidgetDetail::kDefaultFontSize;
				style.lineHeight = 0.0f;
				style.radius = context.theme.GetMetric(SdStyleToken::RadiusSmall);
				style.opacity = 1.0f;
				return style;
			}

			static void Describe(SdStyleContract<Style>& contract)
			{
				contract.Layout(&Style::padding);
				contract.Layout(&Style::width);
				contract.Layout(&Style::height);
				contract.Layout(&Style::trackHeight);
				contract.Layout(&Style::thumbRadius);
				contract.Layout(&Style::labelGap);
				contract.Layout(&Style::fontSize);
				contract.Layout(&Style::lineHeight);
				contract.Paint(&Style::radius).InterpolatesAsFloat();
				contract.Composite(&Style::opacity).InterpolatesAsFloat();
			}
		};

		struct State final
		{
			SdUtf8String label = {};
			float value = 0.0f;
			float minValue = 0.0f;
			float maxValue = 1.0f;
			bool changed = false;
		};

		bool changedThisFrame = false;

		bool Changed() const noexcept
		{
			return changedThisFrame;
		}

		void OnUpdate(SdUpdateContext& context, float& value, float minValue, float maxValue)
		{
			OnUpdate(context, {}, value, minValue, maxValue);
		}

		void OnUpdate(SdUpdateContext& context, SdUtf8StringView label, float& value, float minValue, float maxValue)
		{
			State& state = context.State<State>();
			state.label.assign(label.data(), label.size());
			state.minValue = minValue;
			state.maxValue = maxValue;
			state.changed = false;
			Configure(context);

			if (context.IsPressed() && (context.input.IsMouseButtonHeld(SdMouseButton::Left) || context.input.IsMouseButtonDown(SdMouseButton::Left)))
			{
				const float oldValue = value;
				value = PositionToValue(context, context.input.GetMousePosition().x, minValue, maxValue);
				state.changed = oldValue != value;
			}
			state.value = value;
			changedThisFrame = state.changed;
		}

		void OnLayout(SdLayoutContext& context)
		{
			const State& state = context.State<State>();
			const Style& style = context.TargetStyle<SdSliderFloat>();
			SdVec2 labelSize = {};
			if (!state.label.empty())
			{
				const SdTextStyle textStyle = BasicWidgetDetail::BuildTextStyle({}, style.fontSize, style.lineHeight);
				labelSize = BasicWidgetDetail::MeasureText(context, state.label, textStyle);
			}

			context.SetDesiredSize({
				style.padding.left + labelSize.x + (state.label.empty() ? 0.0f : style.labelGap) + style.width + style.padding.right,
				std::max(style.height, labelSize.y + style.padding.top + style.padding.bottom)
			});
		}

		void OnPaint(SdPaintContext& context)
		{
			const State& state = context.State<State>();
			const Style& style = context.ComputedStyle<SdSliderFloat>();
			const SdTextStyle textStyle = BasicWidgetDetail::BuildTextStyle({}, style.fontSize, style.lineHeight);
			const float lineHeight = BasicWidgetDetail::ResolveLineHeight(textStyle);
			const SdColor textColor = BasicWidgetDetail::ApplyOpacity(context.style.color, context.opacity * style.opacity);
			const SdColor trackColor = BasicWidgetDetail::ApplyOpacity(context.style.background, context.opacity * style.opacity);
			const SdColor border = BasicWidgetDetail::ApplyOpacity(context.style.border, context.opacity * style.opacity);
			const SdColor accent = BasicWidgetDetail::ApplyOpacity(
				context.instance.GetStyleSystem().GetTheme().GetColor(SdStyleToken::ColorAccent),
				context.opacity * style.opacity);

			float trackStartX = context.animatedRect.min.x + style.padding.left;
			if (!state.label.empty())
			{
				const SdVec2 labelSize = BasicWidgetDetail::MeasureText(context.instance, state.label, textStyle);
				const SdVec2 labelPosition = {
					trackStartX,
					context.animatedRect.min.y + std::max(style.padding.top, (context.animatedRect.Height() - lineHeight) * 0.5f)
				};
				context.renderList.AddText(state.label, textStyle, labelPosition, textColor, context.clipRect);
				trackStartX += labelSize.x + style.labelGap;
			}

			const float trackCenterY = context.animatedRect.min.y + (context.animatedRect.Height() * 0.5f);
			const SdRect trackRect = {
				trackStartX,
				trackCenterY - (style.trackHeight * 0.5f),
				trackStartX + style.width,
				trackCenterY + (style.trackHeight * 0.5f)
			};
			const float t = BasicWidgetDetail::Normalize(state.value, state.minValue, state.maxValue);
			const float thumbX = BasicWidgetDetail::Lerp(trackRect.min.x, trackRect.max.x, t);
			const SdRect fillRect = { trackRect.min.x, trackRect.min.y, thumbX, trackRect.max.y };
			context.renderList.AddRectFilled(trackRect, trackColor, context.clipRect, style.radius);
			context.renderList.AddRectFilled(fillRect, accent, context.clipRect, style.radius);
			context.renderList.AddRect(trackRect, border, context.clipRect, 1.0f, style.radius);
			context.renderList.AddCircleFilled({ thumbX, trackCenterY }, style.thumbRadius, accent, context.clipRect);
			context.renderList.AddCircle({ thumbX, trackCenterY }, style.thumbRadius, border, context.clipRect, 1.0f);
		}

	private:
		static void Configure(SdUpdateContext& context)
		{
			context.widgetState.styleTokenTag = Style::TokenTag;
			context.widgetState.inputEnabled = true;
			context.widgetState.layoutWeight = 1.0f;
		}

		static float PositionToValue(SdUpdateContext& context, float x, float minValue, float maxValue)
		{
			const Style& style = context.instance.GetTargetStyle<SdSliderFloat>(context.id);
			const float trackMin = context.widgetState.targetRect.min.x + style.padding.left;
			const float trackMax = trackMin + std::max(1.0f, style.width);
			const float t = std::clamp((x - trackMin) / (trackMax - trackMin), 0.0f, 1.0f);
			return BasicWidgetDetail::Lerp(minValue, maxValue, t);
		}
	};

	struct SdTextInput final : SdWidgetTag
	{
		struct Style final
		{
			static constexpr SdStyleTokenTag TokenTag = SdStyleTargetTags::TextInput;
			SdSpacing padding = { 9.0f, 6.0f, 9.0f, 6.0f };
			float width = 220.0f;
			float minHeight = 32.0f;
			float fontSize = 16.0f;
			float lineHeight = 0.0f;
			float radius = 5.0f;
			float opacity = 1.0f;

			static Style Default(const SdStyleContext& context)
			{
				Style style = {};
				const float smallSpacing = context.theme.GetMetric(SdStyleToken::SpacingSmall);
				const float mediumSpacing = context.theme.GetMetric(SdStyleToken::SpacingMedium);
				style.padding = { mediumSpacing, smallSpacing, mediumSpacing, smallSpacing };
				style.width = 220.0f;
				style.minHeight = 32.0f;
				style.fontSize = BasicWidgetDetail::kDefaultFontSize;
				style.lineHeight = 0.0f;
				style.radius = context.theme.GetMetric(SdStyleToken::RadiusSmall);
				style.opacity = 1.0f;
				return style;
			}

			static void Describe(SdStyleContract<Style>& contract)
			{
				contract.Layout(&Style::padding);
				contract.Layout(&Style::width);
				contract.Layout(&Style::minHeight);
				contract.Layout(&Style::fontSize);
				contract.Layout(&Style::lineHeight);
				contract.Paint(&Style::radius).InterpolatesAsFloat();
				contract.Composite(&Style::opacity).InterpolatesAsFloat();
			}
		};

		struct State final
		{
			SdUtf8String text = {};
			SdUtf8String placeholder = {};
			SdUtf8String composition = {};
			bool changed = false;
			bool focused = false;
		};

		bool changedThisFrame = false;

		bool Changed() const noexcept
		{
			return changedThisFrame;
		}

		void OnUpdate(SdUpdateContext& context, SdUtf8String& value)
		{
			OnUpdate(context, value, {});
		}

		void OnUpdate(SdUpdateContext& context, SdUtf8String& value, SdUtf8StringView placeholder)
		{
			State& state = context.State<State>();
			state.placeholder.assign(placeholder.data(), placeholder.size());
			state.changed = false;
			Configure(context);

			if (context.IsFocused())
			{
				if (context.input.IsKeyDown(SdKeyCode::Backspace))
				{
					const SdSize oldSize = value.size();
					BasicWidgetDetail::PopBackUtf8(value);
					state.changed = state.changed || value.size() != oldSize;
				}
				const SdUtf8StringView committedText = context.input.GetCommittedText();
				if (!committedText.empty())
				{
					value.append(committedText.data(), committedText.size());
					state.changed = true;
				}

				const Style& style = context.instance.GetTargetStyle<SdTextInput>(context.id);
				const SdTextStyle textStyle = BasicWidgetDetail::BuildTextStyle({}, style.fontSize, style.lineHeight);
				const float lineHeight = BasicWidgetDetail::ResolveLineHeight(textStyle);
				const SdVec2 textSize = BasicWidgetDetail::MeasureText(context, value, textStyle);
				const SdRect caretRect = {
					context.widgetState.targetRect.min.x + style.padding.left + textSize.x,
					context.widgetState.targetRect.min.y + style.padding.top,
					context.widgetState.targetRect.min.x + style.padding.left + textSize.x + 1.0f,
					context.widgetState.targetRect.min.y + style.padding.top + lineHeight
				};
				const SdTextInputTargetId targetId = BasicWidgetDetail::MakeTextInputTargetId(context.id);
				context.instance.GetInputSystem().SetTextInputTarget(targetId, caretRect, lineHeight);
				context.instance.GetInputSystem().ActivateTextInput(targetId);
			}
			else
			{
				context.instance.GetInputSystem().DeactivateTextInput(BasicWidgetDetail::MakeTextInputTargetId(context.id));
			}

			state.focused = context.IsFocused();
			state.text = value;
			state.composition = context.input.IsComposing() && state.focused
				? SdUtf8String(context.input.GetCompositionText())
				: SdUtf8String();
			changedThisFrame = state.changed;
		}

		void OnUpdate(SdUpdateContext& context, SdUtf8String& value, SdUtf8StringView placeholder, bool& changed)
		{
			OnUpdate(context, value, placeholder);
			changed = changedThisFrame;
		}

		void OnLayout(SdLayoutContext& context)
		{
			const Style& style = context.TargetStyle<SdTextInput>();
			const SdTextStyle textStyle = BasicWidgetDetail::BuildTextStyle({}, style.fontSize, style.lineHeight);
			const float lineHeight = BasicWidgetDetail::ResolveLineHeight(textStyle);
			context.SetDesiredSize({
				std::max(style.width, style.padding.left + style.padding.right + 24.0f),
				std::max(style.minHeight, lineHeight + style.padding.top + style.padding.bottom)
			});
		}

		void OnPaint(SdPaintContext& context)
		{
			const State& state = context.State<State>();
			const Style& style = context.ComputedStyle<SdTextInput>();
			const SdTextStyle textStyle = BasicWidgetDetail::BuildTextStyle({}, style.fontSize, style.lineHeight);
			const float lineHeight = BasicWidgetDetail::ResolveLineHeight(textStyle);
			const SdColor background = BasicWidgetDetail::ApplyOpacity(context.style.background, context.opacity * style.opacity);
			const SdColor border = BasicWidgetDetail::ApplyOpacity(context.style.border, context.opacity * style.opacity);
			const SdColor textColor = BasicWidgetDetail::ApplyOpacity(context.style.color, context.opacity * style.opacity);
			SdColor placeholderColor = textColor;
			placeholderColor.a = static_cast<SdUInt8>(static_cast<float>(placeholderColor.a) * 0.52f);

			context.renderList.AddRectFilled(context.animatedRect, background, context.clipRect, style.radius);
			context.renderList.AddRect(context.animatedRect, border, context.clipRect, 1.0f, style.radius);

			const bool showPlaceholder = state.text.empty() && state.composition.empty() && !state.placeholder.empty();
			SdUtf8String paintText = showPlaceholder ? state.placeholder : state.text;
			if (!state.composition.empty())
				paintText += state.composition;

			const SdVec2 textPosition = {
				context.animatedRect.min.x + style.padding.left,
				context.animatedRect.min.y + std::max(style.padding.top, (context.animatedRect.Height() - lineHeight) * 0.5f)
			};
			context.renderList.AddText(
				paintText,
				textStyle,
				textPosition,
				showPlaceholder ? placeholderColor : textColor,
				context.clipRect);

			if (state.focused)
			{
				const SdVec2 textSize = BasicWidgetDetail::MeasureText(context.instance, state.text, textStyle);
				const SdRect caretRect = {
					textPosition.x + textSize.x + 1.0f,
					textPosition.y,
					textPosition.x + textSize.x + 2.0f,
					textPosition.y + lineHeight
				};
				context.renderList.AddRectFilled(caretRect, textColor, context.clipRect);
			}
		}

	private:
		static void Configure(SdUpdateContext& context)
		{
			context.widgetState.styleTokenTag = Style::TokenTag;
			context.widgetState.inputEnabled = true;
			context.widgetState.layoutWeight = 1.0f;
		}
	};

	struct SdWindowOptions final
	{
		SdVec2 position = { 64.0f, 54.0f };
		SdVec2 size = { 420.0f, 260.0f };
		bool closable = true;
		bool draggable = true;
	};

	struct SdWindow final : SdWidgetTag
	{
		struct Style final
		{
			static constexpr SdStyleTokenTag TokenTag = SdStyleTargetTags::Window;
			SdSpacing padding = { 12.0f, 40.0f, 12.0f, 12.0f };
			float width = 420.0f;
			float height = 260.0f;
			float titleHeight = 30.0f;
			float childSpacing = 6.0f;
			float fontSize = 16.0f;
			float lineHeight = 0.0f;
			float radius = 5.0f;
			float opacity = 1.0f;

			static Style Default(const SdStyleContext& context)
			{
				Style style = {};
				const float mediumSpacing = context.theme.GetMetric(SdStyleToken::SpacingMedium);
				style.padding = { mediumSpacing, 40.0f, mediumSpacing, mediumSpacing };
				style.width = 420.0f;
				style.height = 260.0f;
				style.titleHeight = 30.0f;
				style.childSpacing = context.theme.GetMetric(SdStyleToken::SpacingSmall);
				style.fontSize = BasicWidgetDetail::kDefaultFontSize;
				style.lineHeight = 0.0f;
				style.radius = context.theme.GetMetric(SdStyleToken::RadiusSmall);
				style.opacity = 1.0f;
				return style;
			}

			static void Describe(SdStyleContract<Style>& contract)
			{
				contract.Layout(&Style::padding);
				contract.Layout(&Style::width);
				contract.Layout(&Style::height);
				contract.Layout(&Style::titleHeight);
				contract.Layout(&Style::childSpacing);
				contract.Layout(&Style::fontSize);
				contract.Layout(&Style::lineHeight);
				contract.Paint(&Style::radius).InterpolatesAsFloat();
				contract.Composite(&Style::opacity).InterpolatesAsFloat();
			}
		};

		struct State final
		{
			SdUtf8String title = {};
			SdRect rect = {};
			SdWindowOptions options = {};
			bool open = true;
			bool initialized = false;
			bool dragging = false;
		};

		void OnUpdate(SdUpdateContext& context, SdUtf8StringView title)
		{
			State& state = context.State<State>();
			bool open = state.open;
			UpdateWindow(context, title, open, {});
			state.open = open;
		}

		void OnUpdate(SdUpdateContext& context, SdUtf8StringView title, bool& open)
		{
			UpdateWindow(context, title, open, {});
		}

		void OnUpdate(SdUpdateContext& context, SdUtf8StringView title, bool& open, const SdWindowOptions& options)
		{
			UpdateWindow(context, title, open, options);
		}

		template<class TContent>
			requires std::is_invocable_v<TContent&, SdUi&>
		void OnUpdate(SdUpdateContext& context, SdUtf8StringView title, TContent&& content)
		{
			State& state = context.State<State>();
			bool open = state.open;
			UpdateWindow(context, title, open, {});
			state.open = open;
			if (open)
				std::forward<TContent>(content)(context.ui);
		}

		template<class TContent>
			requires std::is_invocable_v<TContent&, SdUi&>
		void OnUpdate(SdUpdateContext& context, SdUtf8StringView title, bool& open, TContent&& content)
		{
			UpdateWindow(context, title, open, {});
			if (open)
				std::forward<TContent>(content)(context.ui);
		}

		template<class TContent>
			requires std::is_invocable_v<TContent&, SdUi&>
		void OnUpdate(SdUpdateContext& context, SdUtf8StringView title, bool& open, const SdWindowOptions& options, TContent&& content)
		{
			UpdateWindow(context, title, open, options);
			if (open)
				std::forward<TContent>(content)(context.ui);
		}

		void OnLayout(SdLayoutContext& context)
		{
			State& state = context.State<State>();
			const Style& style = context.TargetStyle<SdWindow>();
			if (!state.initialized)
			{
				const SdVec2 size = {
					state.options.size.x > 0.0f ? state.options.size.x : style.width,
					state.options.size.y > 0.0f ? state.options.size.y : style.height
				};
				state.rect = BasicWidgetDetail::MakeRect(state.options.position, size);
				state.initialized = true;
			}

			context.widgetState.manualLayout = true;
			context.widgetState.manualRect = state.open ? state.rect : SdRect{};
			context.widgetState.arrangeChildren = state.open;
			context.widgetState.clipChildren = true;
			context.widgetState.childPadding = style.padding;
			context.widgetState.childSpacing = std::max(0.0f, style.childSpacing);
			context.SetDesiredSize(state.open ? state.rect.Size() : SdVec2{});
		}

		void OnPaint(SdPaintContext& context)
		{
			const State& state = context.State<State>();
			if (!state.open)
				return;

			const Style& style = context.ComputedStyle<SdWindow>();
			const SdTextStyle textStyle = BasicWidgetDetail::BuildTextStyle({}, style.fontSize, style.lineHeight);
			const float lineHeight = BasicWidgetDetail::ResolveLineHeight(textStyle);
			const SdColor background = BasicWidgetDetail::ApplyOpacity(context.style.background, context.opacity * style.opacity);
			const SdColor border = BasicWidgetDetail::ApplyOpacity(context.style.border, context.opacity * style.opacity);
			const SdColor textColor = BasicWidgetDetail::ApplyOpacity(context.style.color, context.opacity * style.opacity);
			const SdColor titleColor = BasicWidgetDetail::ApplyOpacity(
				context.instance.GetStyleSystem().GetTheme().GetColor(SdStyleToken::ColorButton),
				context.opacity * style.opacity);

			context.renderList.AddRectFilled(context.animatedRect, background, context.clipRect, style.radius);
			const SdRect titleRect = {
				context.animatedRect.min.x,
				context.animatedRect.min.y,
				context.animatedRect.max.x,
				std::min(context.animatedRect.max.y, context.animatedRect.min.y + style.titleHeight)
			};
			context.renderList.AddRectFilled(titleRect, titleColor, context.clipRect, style.radius);
			context.renderList.AddRect(context.animatedRect, border, context.clipRect, 1.0f, style.radius);

			const SdVec2 titlePosition = {
				titleRect.min.x + style.padding.left,
				titleRect.min.y + std::max(0.0f, (titleRect.Height() - lineHeight) * 0.5f)
			};
			context.renderList.AddText(state.title, textStyle, titlePosition, textColor, context.clipRect);

			if (state.options.closable)
			{
				const SdRect closeRect = CloseRect(titleRect);
				context.renderList.AddLine(
					{ closeRect.min.x + 5.0f, closeRect.min.y + 5.0f },
					{ closeRect.max.x - 5.0f, closeRect.max.y - 5.0f },
					textColor,
					context.clipRect,
					1.5f);
				context.renderList.AddLine(
					{ closeRect.max.x - 5.0f, closeRect.min.y + 5.0f },
					{ closeRect.min.x + 5.0f, closeRect.max.y - 5.0f },
					textColor,
					context.clipRect,
					1.5f);
			}
		}

	private:
		static SdRect CloseRect(const SdRect& titleRect) noexcept
		{
			const float size = std::min(24.0f, std::max(0.0f, titleRect.Height() - 6.0f));
			return {
				titleRect.max.x - size - 6.0f,
				titleRect.min.y + ((titleRect.Height() - size) * 0.5f),
				titleRect.max.x - 6.0f,
				titleRect.min.y + ((titleRect.Height() + size) * 0.5f)
			};
		}

		static SdRect TitleRect(const State& state, const Style& style) noexcept
		{
			return {
				state.rect.min.x,
				state.rect.min.y,
				state.rect.max.x,
				std::min(state.rect.max.y, state.rect.min.y + style.titleHeight)
			};
		}

		static void UpdateWindow(SdUpdateContext& context, SdUtf8StringView title, bool& open, const SdWindowOptions& options)
		{
			State& state = context.State<State>();
			state.title.assign(title.data(), title.size());
			state.options = options;
			state.open = open;
			context.widgetState.styleTokenTag = Style::TokenTag;
			context.widgetState.layerPriority = SdLayerPriority::Floating;
			context.widgetState.inputEnabled = open;
			context.widgetState.layoutWeight = open ? 1.0f : 0.0f;
			if (!open)
			{
				state.dragging = false;
				return;
			}

			const Style& style = context.instance.GetTargetStyle<SdWindow>(context.id);
			if (!state.initialized)
			{
				const SdVec2 size = {
					state.options.size.x > 0.0f ? state.options.size.x : style.width,
					state.options.size.y > 0.0f ? state.options.size.y : style.height
				};
				state.rect = BasicWidgetDetail::MakeRect(state.options.position, size);
				state.initialized = true;
			}

			const SdVec2 mouse = context.input.GetMousePosition();
			const SdRect titleRect = TitleRect(state, style);
			const bool inCloseButton = state.options.closable && CloseRect(titleRect).Contains(mouse);
			if (state.options.draggable && context.WasPressed() && titleRect.Contains(mouse) && !inCloseButton)
				state.dragging = true;

			if (state.dragging && context.IsCaptured() && context.input.IsMouseButtonHeld(SdMouseButton::Left))
				state.rect = BasicWidgetDetail::OffsetRect(state.rect, context.input.GetMouseDelta());

			if (context.WasReleased())
				state.dragging = false;

			if (state.options.closable && context.WasClicked() && CloseRect(titleRect).Contains(mouse))
			{
				open = false;
				state.open = false;
				state.dragging = false;
			}
		}
	};

	struct SdImageViewer final : SdWidgetTag
	{
		struct Style final
		{
			static constexpr SdStyleTokenTag TokenTag = SdStyleTargetTags::ImageViewer;
			SdSpacing padding = {};
			float width = 160.0f;
			float height = 120.0f;
			float radius = 5.0f;
			float opacity = 1.0f;

			static Style Default(const SdStyleContext& context)
			{
				Style style = {};
				style.width = 160.0f;
				style.height = 120.0f;
				style.radius = context.theme.GetMetric(SdStyleToken::RadiusSmall);
				style.opacity = 1.0f;
				return style;
			}

			static void Describe(SdStyleContract<Style>& contract)
			{
				contract.Layout(&Style::padding);
				contract.Layout(&Style::width);
				contract.Layout(&Style::height);
				contract.Paint(&Style::radius).InterpolatesAsFloat();
				contract.Composite(&Style::opacity).InterpolatesAsFloat();
			}
		};

		struct State final
		{
			SdTextureHandle texture = {};
			SdRect uvRect = { 0.0f, 0.0f, 1.0f, 1.0f };
			SdColor tint = SdColorWhite;
			SdVec2 size = {};
		};

		void OnUpdate(SdUpdateContext& context, SdTextureHandle texture)
		{
			OnUpdate(context, texture, {});
		}

		void OnUpdate(SdUpdateContext& context, SdTextureHandle texture, SdVec2 size)
		{
			OnUpdate(context, texture, size, { 0.0f, 0.0f, 1.0f, 1.0f }, SdColorWhite);
		}

		void OnUpdate(SdUpdateContext& context, SdTextureHandle texture, SdVec2 size, const SdRect& uvRect, SdColor tint)
		{
			State& state = context.State<State>();
			state.texture = texture;
			state.size = size;
			state.uvRect = uvRect;
			state.tint = tint;
			context.widgetState.styleTokenTag = Style::TokenTag;
			context.widgetState.inputEnabled = false;
			context.widgetState.layoutWeight = 1.0f;
		}

		void OnLayout(SdLayoutContext& context)
		{
			const State& state = context.State<State>();
			const Style& style = context.TargetStyle<SdImageViewer>();
			const SdVec2 imageSize = {
				state.size.x > 0.0f ? state.size.x : style.width,
				state.size.y > 0.0f ? state.size.y : style.height
			};
			context.SetDesiredSize({
				imageSize.x + style.padding.left + style.padding.right,
				imageSize.y + style.padding.top + style.padding.bottom
			});
		}

		void OnPaint(SdPaintContext& context)
		{
			const State& state = context.State<State>();
			const Style& style = context.ComputedStyle<SdImageViewer>();
			const SdColor background = BasicWidgetDetail::ApplyOpacity(context.style.background, context.opacity * style.opacity);
			const SdColor tint = BasicWidgetDetail::ApplyOpacity(state.tint, context.opacity * style.opacity);
			context.renderList.AddRectFilled(context.animatedRect, background, context.clipRect, style.radius);
			const SdRect imageRect = BasicWidgetDetail::InsetRect(context.animatedRect, style.padding);
			if (state.texture.IsValid())
				context.renderList.AddImage(state.texture, imageRect, state.uvRect, tint, context.clipRect);
			context.renderList.AddRect(context.animatedRect, context.style.border, context.clipRect, 1.0f, style.radius);
		}
	};

	struct SdScrollView final : SdWidgetTag
	{
		struct Style final
		{
			static constexpr SdStyleTokenTag TokenTag = SdStyleTargetTags::ScrollView;
			SdSpacing padding = { 8.0f, 8.0f, 8.0f, 8.0f };
			float width = 240.0f;
			float height = 160.0f;
			float childSpacing = 6.0f;
			float scrollbarWidth = 5.0f;
			float radius = 5.0f;
			float opacity = 1.0f;

			static Style Default(const SdStyleContext& context)
			{
				Style style = {};
				const float smallSpacing = context.theme.GetMetric(SdStyleToken::SpacingSmall);
				style.padding = { smallSpacing, smallSpacing, smallSpacing, smallSpacing };
				style.width = 240.0f;
				style.height = 160.0f;
				style.childSpacing = smallSpacing;
				style.scrollbarWidth = 5.0f;
				style.radius = context.theme.GetMetric(SdStyleToken::RadiusSmall);
				style.opacity = 1.0f;
				return style;
			}

			static void Describe(SdStyleContract<Style>& contract)
			{
				contract.Layout(&Style::padding);
				contract.Layout(&Style::width);
				contract.Layout(&Style::height);
				contract.Layout(&Style::childSpacing);
				contract.Layout(&Style::scrollbarWidth);
				contract.Paint(&Style::radius).InterpolatesAsFloat();
				contract.Composite(&Style::opacity).InterpolatesAsFloat();
			}
		};

		struct State final
		{
			float scrollOffset = 0.0f;
		};

		template<class TContent>
			requires std::is_invocable_v<TContent&, SdUi&>
		void OnUpdate(SdUpdateContext& context, TContent&& content)
		{
			State& state = context.State<State>();
			context.widgetState.styleTokenTag = Style::TokenTag;
			context.widgetState.inputEnabled = true;
			context.widgetState.layoutWeight = 1.0f;
			if (context.IsHovered())
			{
				state.scrollOffset = std::max(0.0f, state.scrollOffset - (context.input.GetMouseWheelDelta().y * 24.0f));
			}
			std::forward<TContent>(content)(context.ui);
		}

		void OnLayout(SdLayoutContext& context)
		{
			const Style& style = context.TargetStyle<SdScrollView>();
			context.SetDesiredSize({ std::max(0.0f, style.width), std::max(0.0f, style.height) });
			context.widgetState.arrangeChildren = true;
			context.widgetState.clipChildren = true;
			context.widgetState.childPadding = style.padding;
			context.widgetState.childSpacing = std::max(0.0f, style.childSpacing);
		}

		void OnPaint(SdPaintContext& context)
		{
			const State& state = context.State<State>();
			const Style& style = context.ComputedStyle<SdScrollView>();
			const SdColor background = BasicWidgetDetail::ApplyOpacity(context.style.background, context.opacity * style.opacity);
			const SdColor border = BasicWidgetDetail::ApplyOpacity(context.style.border, context.opacity * style.opacity);
			const SdColor accent = BasicWidgetDetail::ApplyOpacity(
				context.instance.GetStyleSystem().GetTheme().GetColor(SdStyleToken::ColorAccent),
				context.opacity * style.opacity);
			context.renderList.AddRectFilled(context.animatedRect, background, context.clipRect, style.radius);
			context.renderList.AddRect(context.animatedRect, border, context.clipRect, 1.0f, style.radius);

			if (style.scrollbarWidth > 0.0f && state.scrollOffset > 0.0f)
			{
				const float thumbHeight = std::max(18.0f, context.animatedRect.Height() * 0.35f);
				const float travel = std::max(0.0f, context.animatedRect.Height() - thumbHeight - 8.0f);
				const float thumbY = context.animatedRect.min.y + 4.0f + std::fmod(state.scrollOffset, std::max(1.0f, travel));
				const SdRect thumbRect = {
					context.animatedRect.max.x - style.scrollbarWidth - 4.0f,
					thumbY,
					context.animatedRect.max.x - 4.0f,
					thumbY + thumbHeight
				};
				context.renderList.AddRectFilled(thumbRect, accent, context.clipRect, style.scrollbarWidth * 0.5f);
			}
		}
	};

	struct SdPopup final : SdWidgetTag
	{
		struct Style final
		{
			static constexpr SdStyleTokenTag TokenTag = SdStyleTargetTags::Popup;
			SdSpacing padding = { 8.0f, 8.0f, 8.0f, 8.0f };
			float width = 220.0f;
			float height = 140.0f;
			float childSpacing = 6.0f;
			float radius = 5.0f;
			float opacity = 1.0f;

			static Style Default(const SdStyleContext& context)
			{
				Style style = {};
				const float smallSpacing = context.theme.GetMetric(SdStyleToken::SpacingSmall);
				style.padding = { smallSpacing, smallSpacing, smallSpacing, smallSpacing };
				style.width = 220.0f;
				style.height = 140.0f;
				style.childSpacing = smallSpacing;
				style.radius = context.theme.GetMetric(SdStyleToken::RadiusSmall);
				style.opacity = 1.0f;
				return style;
			}

			static void Describe(SdStyleContract<Style>& contract)
			{
				contract.Layout(&Style::padding);
				contract.Layout(&Style::width);
				contract.Layout(&Style::height);
				contract.Layout(&Style::childSpacing);
				contract.Paint(&Style::radius).InterpolatesAsFloat();
				contract.Composite(&Style::opacity).InterpolatesAsFloat();
			}
		};

		struct State final
		{
			SdVec2 position = { 96.0f, 96.0f };
			bool open = false;
		};

		template<class TContent>
			requires std::is_invocable_v<TContent&, SdUi&>
		void OnUpdate(SdUpdateContext& context, bool open, TContent&& content)
		{
			OnUpdate(context, open, { 96.0f, 96.0f }, std::forward<TContent>(content));
		}

		template<class TContent>
			requires std::is_invocable_v<TContent&, SdUi&>
		void OnUpdate(SdUpdateContext& context, bool open, SdVec2 position, TContent&& content)
		{
			State& state = context.State<State>();
			state.open = open;
			state.position = position;
			Configure(context, open);
			if (open)
				std::forward<TContent>(content)(context.ui);
		}

		void OnLayout(SdLayoutContext& context)
		{
			const State& state = context.State<State>();
			const Style& style = context.TargetStyle<SdPopup>();
			context.widgetState.manualLayout = true;
			context.widgetState.manualRect = state.open
				? BasicWidgetDetail::MakeRect(state.position, { style.width, style.height })
				: SdRect{};
			context.widgetState.arrangeChildren = state.open;
			context.widgetState.clipChildren = true;
			context.widgetState.childPadding = style.padding;
			context.widgetState.childSpacing = style.childSpacing;
			context.SetDesiredSize(state.open ? SdVec2{ style.width, style.height } : SdVec2{});
		}

		void OnPaint(SdPaintContext& context)
		{
			const State& state = context.State<State>();
			if (!state.open)
				return;
			const Style& style = context.ComputedStyle<SdPopup>();
			context.renderList.AddRectFilled(
				context.animatedRect,
				BasicWidgetDetail::ApplyOpacity(context.style.background, context.opacity * style.opacity),
				context.clipRect,
				style.radius);
			context.renderList.AddRect(context.animatedRect, context.style.border, context.clipRect, 1.0f, style.radius);
		}

	protected:
		static void Configure(SdUpdateContext& context, bool open)
		{
			context.widgetState.styleTokenTag = Style::TokenTag;
			context.widgetState.layerPriority = SdLayerPriority::Popup;
			context.widgetState.inputEnabled = open;
			context.widgetState.layoutWeight = open ? 1.0f : 0.0f;
		}
	};

	struct SdContextMenu final : SdWidgetTag
	{
		struct Style final
		{
			static constexpr SdStyleTokenTag TokenTag = SdStyleTargetTags::ContextMenu;
			SdSpacing padding = { 8.0f, 8.0f, 8.0f, 8.0f };
			float width = 220.0f;
			float height = 140.0f;
			float childSpacing = 6.0f;
			float radius = 5.0f;
			float opacity = 1.0f;

			static Style Default(const SdStyleContext& context)
			{
				Style style = {};
				const float smallSpacing = context.theme.GetMetric(SdStyleToken::SpacingSmall);
				style.padding = { smallSpacing, smallSpacing, smallSpacing, smallSpacing };
				style.width = 220.0f;
				style.height = 140.0f;
				style.childSpacing = smallSpacing;
				style.radius = context.theme.GetMetric(SdStyleToken::RadiusSmall);
				style.opacity = 1.0f;
				return style;
			}

			static void Describe(SdStyleContract<Style>& contract)
			{
				contract.Layout(&Style::padding);
				contract.Layout(&Style::width);
				contract.Layout(&Style::height);
				contract.Layout(&Style::childSpacing);
				contract.Paint(&Style::radius).InterpolatesAsFloat();
				contract.Composite(&Style::opacity).InterpolatesAsFloat();
			}
		};

		struct State final
		{
			SdVec2 position = {};
			bool open = false;
		};

		template<class TContent>
			requires std::is_invocable_v<TContent&, SdUi&>
		void OnUpdate(SdUpdateContext& context, bool open, SdVec2 position, TContent&& content)
		{
			State& state = context.State<State>();
			state.open = open;
			state.position = position;
			context.widgetState.styleTokenTag = Style::TokenTag;
			context.widgetState.layerPriority = SdLayerPriority::Popup;
			context.widgetState.inputEnabled = open;
			context.widgetState.layoutWeight = open ? 1.0f : 0.0f;
			if (open)
				std::forward<TContent>(content)(context.ui);
		}

		void OnLayout(SdLayoutContext& context)
		{
			const State& state = context.State<State>();
			const Style& style = context.TargetStyle<SdContextMenu>();
			context.widgetState.manualLayout = true;
			context.widgetState.manualRect = state.open
				? BasicWidgetDetail::MakeRect(state.position, { style.width, style.height })
				: SdRect{};
			context.widgetState.arrangeChildren = state.open;
			context.widgetState.clipChildren = true;
			context.widgetState.childPadding = style.padding;
			context.widgetState.childSpacing = style.childSpacing;
			context.SetDesiredSize(state.open ? SdVec2{ style.width, style.height } : SdVec2{});
		}

		void OnPaint(SdPaintContext& context)
		{
			const State& state = context.State<State>();
			if (!state.open)
				return;
			const Style& style = context.ComputedStyle<SdContextMenu>();
			context.renderList.AddRectFilled(
				context.animatedRect,
				BasicWidgetDetail::ApplyOpacity(context.style.background, context.opacity * style.opacity),
				context.clipRect,
				style.radius);
			context.renderList.AddRect(context.animatedRect, context.style.border, context.clipRect, 1.0f, style.radius);
		}
	};

	struct SdTooltip final : SdWidgetTag
	{
		struct Style final
		{
			static constexpr SdStyleTokenTag TokenTag = SdStyleTargetTags::Tooltip;
			SdSpacing padding = { 7.0f, 5.0f, 7.0f, 5.0f };
			float fontSize = 16.0f;
			float lineHeight = 0.0f;
			float radius = 5.0f;
			float opacity = 1.0f;

			static Style Default(const SdStyleContext& context)
			{
				Style style = {};
				const float smallSpacing = context.theme.GetMetric(SdStyleToken::SpacingSmall);
				style.padding = { smallSpacing, smallSpacing, smallSpacing, smallSpacing };
				style.fontSize = BasicWidgetDetail::kDefaultFontSize;
				style.lineHeight = 0.0f;
				style.radius = context.theme.GetMetric(SdStyleToken::RadiusSmall);
				style.opacity = 1.0f;
				return style;
			}

			static void Describe(SdStyleContract<Style>& contract)
			{
				contract.Layout(&Style::padding);
				contract.Layout(&Style::fontSize);
				contract.Layout(&Style::lineHeight);
				contract.Paint(&Style::radius).InterpolatesAsFloat();
				contract.Composite(&Style::opacity).InterpolatesAsFloat();
			}
		};

		struct State final
		{
			SdUtf8String text = {};
			SdVec2 position = {};
			bool visible = false;
		};

		void OnUpdate(SdUpdateContext& context, bool visible, SdVec2 position, SdUtf8StringView text)
		{
			State& state = context.State<State>();
			state.visible = visible;
			state.position = position;
			state.text.assign(text.data(), text.size());
			context.widgetState.styleTokenTag = Style::TokenTag;
			context.widgetState.layerPriority = SdLayerPriority::Overlay;
			context.widgetState.inputEnabled = false;
			context.widgetState.layoutWeight = visible ? 1.0f : 0.0f;
		}

		void OnLayout(SdLayoutContext& context)
		{
			const State& state = context.State<State>();
			const Style& style = context.TargetStyle<SdTooltip>();
			const SdTextStyle textStyle = BasicWidgetDetail::BuildTextStyle({}, style.fontSize, style.lineHeight);
			const SdVec2 textSize = BasicWidgetDetail::MeasureText(context, state.text, textStyle);
			const SdVec2 size = state.visible
				? SdVec2{ textSize.x + style.padding.left + style.padding.right, textSize.y + style.padding.top + style.padding.bottom }
				: SdVec2{};
			context.widgetState.manualLayout = true;
			context.widgetState.manualRect = BasicWidgetDetail::MakeRect(state.position, size);
			context.SetDesiredSize(size);
		}

		void OnPaint(SdPaintContext& context)
		{
			const State& state = context.State<State>();
			if (!state.visible)
				return;
			const Style& style = context.ComputedStyle<SdTooltip>();
			const SdTextStyle textStyle = BasicWidgetDetail::BuildTextStyle({}, style.fontSize, style.lineHeight);
			const SdColor background = BasicWidgetDetail::ApplyOpacity(context.style.background, context.opacity * style.opacity);
			const SdColor border = BasicWidgetDetail::ApplyOpacity(context.style.border, context.opacity * style.opacity);
			const SdColor color = BasicWidgetDetail::ApplyOpacity(context.style.color, context.opacity * style.opacity);
			context.renderList.AddRectFilled(context.animatedRect, background, context.clipRect, style.radius);
			context.renderList.AddRect(context.animatedRect, border, context.clipRect, 1.0f, style.radius);
			context.renderList.AddText(
				state.text,
				textStyle,
				{ context.animatedRect.min.x + style.padding.left, context.animatedRect.min.y + style.padding.top },
				color,
				context.clipRect);
		}
	};
}
