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
		static constexpr SdStyleId TargetTypeId = SdWidgetTargetIds::Text;

		struct Style final
		{
			static constexpr SdStyleId Color = SdStylePropertyIds::Color;
			static constexpr SdStyleId Opacity = SdStylePropertyIds::Opacity;
			SdSpacing padding = {};

			static Style Default(const SdStyleContext& context)
			{
				(void)context;
				Style style = {};
				return style;
			}

			static void Describe(SdStyleContract<Style>& contract)
			{
				contract.Layout(&Style::padding);
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
			context.widgetState.targetTypeId = TargetTypeId;
			context.widgetState.inputEnabled = false;
			context.widgetState.layoutWeight = 1.0f;
		}

		void OnLayout(SdLayoutContext& context)
		{
			const State& state = context.State<State>();
			const Style& style = context.RootResolvedStyle<SdText>();
			const SdBoxStyle& rootStyle = context.RootStyleNode().resolvedStyle;
			const SdTextStyle textStyle = BasicWidgetDetail::BuildTextStyle(state.textStyle, rootStyle.fontSize, rootStyle.lineHeight);
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

			const Style& style = context.RootResolvedStyle<SdText>();
			const SdBoxStyle& presentation = context.RootStyleNode().presentationStyle;
			const SdTextStyle textStyle = BasicWidgetDetail::BuildTextStyle(state.textStyle, presentation.fontSize, presentation.lineHeight);
			const SdColor color = BasicWidgetDetail::ApplyOpacity(
				presentation.color,
				context.opacity * presentation.opacity);
			const SdVec2 position = {
				context.animatedRect.min.x + style.padding.left,
				context.animatedRect.min.y + style.padding.top
			};
			context.renderList.AddText(state.text, textStyle, position, color, context.clipRect);
		}
	};

	struct SdPanel final : SdWidgetTag
	{
		static constexpr SdStyleId TargetTypeId = SdWidgetTargetIds::Panel;

		struct Style final
		{
			float childSpacing = 6.0f;

			static Style Default(const SdStyleContext& context)
			{
				Style style = {};
				style.childSpacing = context.theme.GetMetricVariable(SdThemeVariableLiteral("spacing.small"));
				return style;
			}

			static void Describe(SdStyleContract<Style>& contract)
			{
				contract.Layout(&Style::childSpacing);
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
			const Style& style = context.RootResolvedStyle<SdPanel>();
			const SdBoxStyle& rootStyle = context.RootStyleNode().resolvedStyle;
			const SdResolvedBoxStyle usedStyle = SdResolveBoxStyle(rootStyle, context.constraints.maxSize, { 240.0f, 120.0f });
			context.SetDesiredSize({
				std::max(0.0f, usedStyle.width),
				std::max(0.0f, usedStyle.height)
			});
			context.widgetState.arrangeChildren = true;
			context.widgetState.clipChildren = true;
			context.widgetState.childPadding = {
				usedStyle.padding.left,
				usedStyle.padding.top,
				usedStyle.padding.right,
				usedStyle.padding.bottom
			};
			context.widgetState.childSpacing = std::max(0.0f, style.childSpacing);
		}

		void OnPaint(SdPaintContext& context)
		{
			const SdBoxStyle& presentation = context.RootStyleNode().presentationStyle;
			const float radius = SdResolveLength(presentation.radius, context.animatedRect.Width());
			context.renderList.AddRectFilled(
				context.animatedRect,
				BasicWidgetDetail::ApplyOpacity(presentation.backgroundColor, context.opacity * presentation.opacity),
				context.clipRect,
				radius);
			context.renderList.AddRect(
				context.animatedRect,
				BasicWidgetDetail::ApplyOpacity(presentation.border.left.color, context.opacity * presentation.opacity),
				context.clipRect,
				1.0f,
				radius);
		}

	private:
		static void Configure(SdUpdateContext& context)
		{
			context.widgetState.targetTypeId = TargetTypeId;
			context.widgetState.inputEnabled = false;
			context.widgetState.layoutWeight = 1.0f;
		}
	};

	struct SdButton final : SdWidgetTag
	{
		static constexpr SdStyleId TargetTypeId = SdWidgetTargetIds::Button;

		struct Parts final
		{
			static constexpr SdStylePart Content = SdStylePart::Make("Sodium.Button.Part.Content");
			static constexpr SdStylePart Icon = SdStylePart::Make("Sodium.Button.Part.Icon");
			static constexpr SdStylePart Label = SdStylePart::Make("Sodium.Button.Part.Label");
		};

		struct Style final
		{
			static Style Default(const SdStyleContext& context)
			{
				(void)context;
				Style style = {};
				return style;
			}

			static void Describe(SdStyleContract<Style>& contract)
			{
				(void)contract;
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
			const SdBoxStyle& rootStyle = context.RootStyleNode().resolvedStyle;
			const SdResolvedBoxStyle usedStyle = SdResolveBoxStyle(rootStyle, context.constraints.maxSize, {});
			const SdTextStyle textStyle = BasicWidgetDetail::BuildTextStyle({}, rootStyle.fontSize, rootStyle.lineHeight);
			SdVec2 textSize = BasicWidgetDetail::MeasureText(context, state.label, textStyle);
			textSize.y = std::max(textSize.y, BasicWidgetDetail::ResolveLineHeight(textStyle));

			context.SetDesiredSize({
				std::max(usedStyle.minWidth, textSize.x + usedStyle.padding.left + usedStyle.padding.right),
				std::max(usedStyle.minHeight, textSize.y + usedStyle.padding.top + usedStyle.padding.bottom)
			});
		}

		void OnPaint(SdPaintContext& context)
		{
			const State& state = context.State<State>();
			const SdBoxStyle& presentation = context.RootStyleNode().presentationStyle;
			const SdResolvedBoxStyle usedStyle = SdResolveBoxStyle(presentation, context.animatedRect.Size(), {});
			const SdTextStyle textStyle = BasicWidgetDetail::BuildTextStyle({}, presentation.fontSize, presentation.lineHeight);
			const float lineHeight = BasicWidgetDetail::ResolveLineHeight(textStyle);
			const SdVec2 textSize = BasicWidgetDetail::MeasureText(context.instance, state.label, textStyle);
			const SdColor background = BasicWidgetDetail::ApplyOpacity(presentation.backgroundColor, context.opacity * presentation.opacity);
			const SdColor border = BasicWidgetDetail::ApplyOpacity(presentation.border.left.color, context.opacity * presentation.opacity);
			const SdColor color = BasicWidgetDetail::ApplyOpacity(presentation.color, context.opacity * presentation.opacity);
			const float radius = SdResolveLength(presentation.radius, context.animatedRect.Width());

			context.renderList.AddRectFilled(context.animatedRect, background, context.clipRect, radius);
			context.renderList.AddRect(context.animatedRect, border, context.clipRect, 1.0f, radius);
			const SdVec2 position = {
				context.animatedRect.min.x + std::max(usedStyle.padding.left, (context.animatedRect.Width() - textSize.x) * 0.5f),
				context.animatedRect.min.y + std::max(usedStyle.padding.top, (context.animatedRect.Height() - lineHeight) * 0.5f)
			};
			context.renderList.AddText(state.label, textStyle, position, color, context.clipRect);
		}

	private:
		static void Configure(SdUpdateContext& context)
		{
			context.widgetState.targetTypeId = TargetTypeId;
			context.widgetState.inputEnabled = true;
			context.widgetState.layoutWeight = 1.0f;
			context.EnsurePart(Parts::Content);
			context.EnsurePart(Parts::Icon);
			context.EnsurePart(Parts::Label);
		}
	};

	struct SdCheckBox final : SdWidgetTag
	{
		static constexpr SdStyleId TargetTypeId = SdWidgetTargetIds::CheckBox;

		struct Style final
		{
			float boxSize = 18.0f;
			float gap = 8.0f;

			static Style Default(const SdStyleContext& context)
			{
				Style style = {};
				const float smallSpacing = context.theme.GetMetricVariable(SdThemeVariableLiteral("spacing.small"));
				style.boxSize = 18.0f;
				style.gap = smallSpacing;
				return style;
			}

			static void Describe(SdStyleContract<Style>& contract)
			{
				contract.Layout(&Style::boxSize);
				contract.Layout(&Style::gap);
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
			const Style& style = context.RootResolvedStyle<SdCheckBox>();
			const SdBoxStyle& rootStyle = context.RootStyleNode().resolvedStyle;
			const SdResolvedBoxStyle usedStyle = SdResolveBoxStyle(rootStyle, context.constraints.maxSize, {});
			const SdTextStyle textStyle = BasicWidgetDetail::BuildTextStyle({}, rootStyle.fontSize, rootStyle.lineHeight);
			SdVec2 textSize = BasicWidgetDetail::MeasureText(context, state.label, textStyle);
			textSize.y = std::max(textSize.y, BasicWidgetDetail::ResolveLineHeight(textStyle));

			context.SetDesiredSize({
				usedStyle.padding.left + style.boxSize + style.gap + textSize.x + usedStyle.padding.right,
				std::max(usedStyle.minHeight, std::max(style.boxSize, textSize.y) + usedStyle.padding.top + usedStyle.padding.bottom)
			});
		}

		void OnPaint(SdPaintContext& context)
		{
			const State& state = context.State<State>();
			const Style& style = context.RootPresentationStyle<SdCheckBox>();
			const SdBoxStyle& presentation = context.RootStyleNode().presentationStyle;
			const SdResolvedBoxStyle usedStyle = SdResolveBoxStyle(presentation, context.animatedRect.Size(), {});
			const SdTextStyle textStyle = BasicWidgetDetail::BuildTextStyle({}, presentation.fontSize, presentation.lineHeight);
			const float lineHeight = BasicWidgetDetail::ResolveLineHeight(textStyle);
			const float boxY = context.animatedRect.min.y + (context.animatedRect.Height() - style.boxSize) * 0.5f;
			const SdRect boxRect = {
				context.animatedRect.min.x + usedStyle.padding.left,
				boxY,
				context.animatedRect.min.x + usedStyle.padding.left + style.boxSize,
				boxY + style.boxSize
			};
			const SdColor background = BasicWidgetDetail::ApplyOpacity(presentation.backgroundColor, context.opacity * presentation.opacity);
			const SdColor border = BasicWidgetDetail::ApplyOpacity(presentation.border.left.color, context.opacity * presentation.opacity);
			const SdColor textColor = BasicWidgetDetail::ApplyOpacity(presentation.color, context.opacity * presentation.opacity);
			const SdColor accent = BasicWidgetDetail::ApplyOpacity(
				context.instance.GetStyleSystem().GetTheme().GetColorVariable(SdThemeVariableLiteral("accent")),
				context.opacity * presentation.opacity);
			const float radius = SdResolveLength(presentation.radius, style.boxSize);

			context.renderList.AddRectFilled(boxRect, background, context.clipRect, radius);
			context.renderList.AddRect(boxRect, border, context.clipRect, 1.0f, radius);
			if (state.checked)
			{
				context.renderList.AddRectFilled(
					BasicWidgetDetail::InsetRect(boxRect, { 4.0f, 4.0f, 4.0f, 4.0f }),
					accent,
					context.clipRect,
					std::max(0.0f, radius - 2.0f));
			}

			const SdVec2 textPosition = {
				boxRect.max.x + style.gap,
				context.animatedRect.min.y + std::max(usedStyle.padding.top, (context.animatedRect.Height() - lineHeight) * 0.5f)
			};
			context.renderList.AddText(state.label, textStyle, textPosition, textColor, context.clipRect);
		}

	private:
		static void Configure(SdUpdateContext& context)
		{
			context.widgetState.targetTypeId = TargetTypeId;
			context.widgetState.inputEnabled = true;
			context.widgetState.layoutWeight = 1.0f;
		}
	};

	using SdCheckbox = SdCheckBox;

	struct SdSliderFloat final : SdWidgetTag
	{
		static constexpr SdStyleId TargetTypeId = SdWidgetTargetIds::Slider;

		struct Style final
		{
			float trackHeight = 6.0f;
			float thumbRadius = 8.0f;
			float labelGap = 8.0f;

			static Style Default(const SdStyleContext& context)
			{
				Style style = {};
				const float smallSpacing = context.theme.GetMetricVariable(SdThemeVariableLiteral("spacing.small"));
				style.trackHeight = 6.0f;
				style.thumbRadius = 8.0f;
				style.labelGap = smallSpacing;
				return style;
			}

			static void Describe(SdStyleContract<Style>& contract)
			{
				contract.Layout(&Style::trackHeight);
				contract.Layout(&Style::thumbRadius);
				contract.Layout(&Style::labelGap);
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
			const Style& style = context.RootResolvedStyle<SdSliderFloat>();
			const SdBoxStyle& rootStyle = context.RootStyleNode().resolvedStyle;
			const SdResolvedBoxStyle usedStyle = SdResolveBoxStyle(rootStyle, context.constraints.maxSize, { 180.0f, 30.0f });
			SdVec2 labelSize = {};
			if (!state.label.empty())
			{
				const SdTextStyle textStyle = BasicWidgetDetail::BuildTextStyle({}, rootStyle.fontSize, rootStyle.lineHeight);
				labelSize = BasicWidgetDetail::MeasureText(context, state.label, textStyle);
			}

			context.SetDesiredSize({
				usedStyle.padding.left + labelSize.x + (state.label.empty() ? 0.0f : style.labelGap) + usedStyle.width + usedStyle.padding.right,
				std::max(usedStyle.height, labelSize.y + usedStyle.padding.top + usedStyle.padding.bottom)
			});
		}

		void OnPaint(SdPaintContext& context)
		{
			const State& state = context.State<State>();
			const Style& style = context.RootPresentationStyle<SdSliderFloat>();
			const SdBoxStyle& presentation = context.RootStyleNode().presentationStyle;
			const SdResolvedBoxStyle usedStyle = SdResolveBoxStyle(presentation, context.animatedRect.Size(), { 180.0f, 30.0f });
			const SdTextStyle textStyle = BasicWidgetDetail::BuildTextStyle({}, presentation.fontSize, presentation.lineHeight);
			const float lineHeight = BasicWidgetDetail::ResolveLineHeight(textStyle);
			const SdColor textColor = BasicWidgetDetail::ApplyOpacity(presentation.color, context.opacity * presentation.opacity);
			const SdColor trackColor = BasicWidgetDetail::ApplyOpacity(presentation.backgroundColor, context.opacity * presentation.opacity);
			const SdColor border = BasicWidgetDetail::ApplyOpacity(presentation.border.left.color, context.opacity * presentation.opacity);
			const SdColor accent = BasicWidgetDetail::ApplyOpacity(
				context.instance.GetStyleSystem().GetTheme().GetColorVariable(SdThemeVariableLiteral("accent")),
				context.opacity * presentation.opacity);
			const float radius = SdResolveLength(presentation.radius, usedStyle.width);

			float trackStartX = context.animatedRect.min.x + usedStyle.padding.left;
			if (!state.label.empty())
			{
				const SdVec2 labelSize = BasicWidgetDetail::MeasureText(context.instance, state.label, textStyle);
				const SdVec2 labelPosition = {
					trackStartX,
					context.animatedRect.min.y + std::max(usedStyle.padding.top, (context.animatedRect.Height() - lineHeight) * 0.5f)
				};
				context.renderList.AddText(state.label, textStyle, labelPosition, textColor, context.clipRect);
				trackStartX += labelSize.x + style.labelGap;
			}

			const float trackCenterY = context.animatedRect.min.y + (context.animatedRect.Height() * 0.5f);
			const SdRect trackRect = {
				trackStartX,
				trackCenterY - (style.trackHeight * 0.5f),
				trackStartX + usedStyle.width,
				trackCenterY + (style.trackHeight * 0.5f)
			};
			const float t = BasicWidgetDetail::Normalize(state.value, state.minValue, state.maxValue);
			const float thumbX = BasicWidgetDetail::Lerp(trackRect.min.x, trackRect.max.x, t);
			const SdRect fillRect = { trackRect.min.x, trackRect.min.y, thumbX, trackRect.max.y };
			context.renderList.AddRectFilled(trackRect, trackColor, context.clipRect, radius);
			context.renderList.AddRectFilled(fillRect, accent, context.clipRect, radius);
			context.renderList.AddRect(trackRect, border, context.clipRect, 1.0f, radius);
			context.renderList.AddCircleFilled({ thumbX, trackCenterY }, style.thumbRadius, accent, context.clipRect);
			context.renderList.AddCircle({ thumbX, trackCenterY }, style.thumbRadius, border, context.clipRect, 1.0f);
		}

	private:
		static void Configure(SdUpdateContext& context)
		{
			context.widgetState.targetTypeId = TargetTypeId;
			context.widgetState.inputEnabled = true;
			context.widgetState.layoutWeight = 1.0f;
		}

		static float PositionToValue(SdUpdateContext& context, float x, float minValue, float maxValue)
		{
			const SdBoxStyle& rootStyle = context.instance.GetRootStyleNode(context.id).resolvedStyle;
			const SdResolvedBoxStyle usedStyle = SdResolveBoxStyle(rootStyle, context.widgetState.targetRect.Size(), { 180.0f, 30.0f });
			const float trackMin = context.widgetState.targetRect.min.x + usedStyle.padding.left;
			const float trackMax = trackMin + std::max(1.0f, usedStyle.width);
			const float t = std::clamp((x - trackMin) / (trackMax - trackMin), 0.0f, 1.0f);
			return BasicWidgetDetail::Lerp(minValue, maxValue, t);
		}
	};

	struct SdTextInput final : SdWidgetTag
	{
		static constexpr SdStyleId TargetTypeId = SdWidgetTargetIds::TextInput;

		struct Parts final
		{
			static constexpr SdStylePart Field = SdStylePart::Make("Sodium.TextInput.Part.Field");
			static constexpr SdStylePart Value = SdStylePart::Make("Sodium.TextInput.Part.Value");
			static constexpr SdStylePart Placeholder = SdStylePart::Make("Sodium.TextInput.Part.Placeholder");
			static constexpr SdStylePart Selection = SdStylePart::Make("Sodium.TextInput.Part.Selection");
			static constexpr SdStylePart Caret = SdStylePart::Make("Sodium.TextInput.Part.Caret");
		};

		struct Style final
		{
			static Style Default(const SdStyleContext& context)
			{
				(void)context;
				Style style = {};
				return style;
			}

			static void Describe(SdStyleContract<Style>& contract)
			{
				(void)contract;
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

				const SdBoxStyle& rootStyle = context.instance.GetRootStyleNode(context.id).resolvedStyle;
				const SdResolvedBoxStyle usedStyle = SdResolveBoxStyle(rootStyle, context.widgetState.targetRect.Size(), {});
				const SdTextStyle textStyle = BasicWidgetDetail::BuildTextStyle({}, rootStyle.fontSize, rootStyle.lineHeight);
				const float lineHeight = BasicWidgetDetail::ResolveLineHeight(textStyle);
				const SdVec2 textSize = BasicWidgetDetail::MeasureText(context, value, textStyle);
				const SdRect caretRect = {
					context.widgetState.targetRect.min.x + usedStyle.padding.left + textSize.x,
					context.widgetState.targetRect.min.y + usedStyle.padding.top,
					context.widgetState.targetRect.min.x + usedStyle.padding.left + textSize.x + 1.0f,
					context.widgetState.targetRect.min.y + usedStyle.padding.top + lineHeight
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
			const SdBoxStyle& rootStyle = context.RootStyleNode().resolvedStyle;
			const SdResolvedBoxStyle usedStyle = SdResolveBoxStyle(rootStyle, context.constraints.maxSize, {});
			const SdTextStyle textStyle = BasicWidgetDetail::BuildTextStyle({}, rootStyle.fontSize, rootStyle.lineHeight);
			const float lineHeight = BasicWidgetDetail::ResolveLineHeight(textStyle);
			context.SetDesiredSize({
				std::max(usedStyle.width, usedStyle.padding.left + usedStyle.padding.right + 24.0f),
				std::max(usedStyle.minHeight, lineHeight + usedStyle.padding.top + usedStyle.padding.bottom)
			});
		}

		void OnPaint(SdPaintContext& context)
		{
			const State& state = context.State<State>();
			const SdBoxStyle& presentation = context.RootStyleNode().presentationStyle;
			const SdResolvedBoxStyle usedStyle = SdResolveBoxStyle(presentation, context.animatedRect.Size(), {});
			const SdTextStyle textStyle = BasicWidgetDetail::BuildTextStyle({}, presentation.fontSize, presentation.lineHeight);
			const float lineHeight = BasicWidgetDetail::ResolveLineHeight(textStyle);
			const SdColor background = BasicWidgetDetail::ApplyOpacity(presentation.backgroundColor, context.opacity * presentation.opacity);
			const SdColor border = BasicWidgetDetail::ApplyOpacity(presentation.border.left.color, context.opacity * presentation.opacity);
			const SdColor textColor = BasicWidgetDetail::ApplyOpacity(presentation.color, context.opacity * presentation.opacity);
			const float radius = SdResolveLength(presentation.radius, context.animatedRect.Width());
			SdColor placeholderColor = textColor;
			placeholderColor.a = static_cast<SdUInt8>(static_cast<float>(placeholderColor.a) * 0.52f);

			context.renderList.AddRectFilled(context.animatedRect, background, context.clipRect, radius);
			context.renderList.AddRect(context.animatedRect, border, context.clipRect, 1.0f, radius);

			const bool showPlaceholder = state.text.empty() && state.composition.empty() && !state.placeholder.empty();
			SdUtf8String paintText = showPlaceholder ? state.placeholder : state.text;
			if (!state.composition.empty())
				paintText += state.composition;

			const SdVec2 textPosition = {
				context.animatedRect.min.x + usedStyle.padding.left,
				context.animatedRect.min.y + std::max(usedStyle.padding.top, (context.animatedRect.Height() - lineHeight) * 0.5f)
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
			context.widgetState.targetTypeId = TargetTypeId;
			context.widgetState.inputEnabled = true;
			context.widgetState.layoutWeight = 1.0f;
			context.EnsurePart(Parts::Field);
			context.EnsurePart(Parts::Value);
			context.EnsurePart(Parts::Placeholder);
			context.EnsurePart(Parts::Selection);
			context.EnsurePart(Parts::Caret);
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
		static constexpr SdStyleId TargetTypeId = SdWidgetTargetIds::Window;

		struct Parts final
		{
			static constexpr SdStylePart Titlebar = SdStylePart::Make("Sodium.Window.Part.Titlebar");
			static constexpr SdStylePart Title = SdStylePart::Make("Sodium.Window.Part.Title");
			static constexpr SdStylePart CloseButton = SdStylePart::Make("Sodium.Window.Part.CloseButton");
			static constexpr SdStylePart Content = SdStylePart::Make("Sodium.Window.Part.Content");
			static constexpr SdStylePart ResizeHandle = SdStylePart::Make("Sodium.Window.Part.ResizeHandle");
		};

		struct Style final
		{
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
				const float mediumSpacing = context.theme.GetMetricVariable(SdThemeVariableLiteral("spacing.medium"));
				style.padding = { mediumSpacing, 40.0f, mediumSpacing, mediumSpacing };
				style.width = 420.0f;
				style.height = 260.0f;
				style.titleHeight = 30.0f;
				style.childSpacing = context.theme.GetMetricVariable(SdThemeVariableLiteral("spacing.small"));
				style.fontSize = BasicWidgetDetail::kDefaultFontSize;
				style.lineHeight = 0.0f;
				style.radius = context.theme.GetMetricVariable(SdThemeVariableLiteral("radius.small"));
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
			const Style& style = context.RootResolvedStyle<SdWindow>();
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

			const Style& style = context.RootPresentationStyle<SdWindow>();
			const SdBoxStyle& presentation = context.RootStyleNode().presentationStyle;
			const SdTextStyle textStyle = BasicWidgetDetail::BuildTextStyle({}, style.fontSize, style.lineHeight);
			const float lineHeight = BasicWidgetDetail::ResolveLineHeight(textStyle);
			const SdColor background = BasicWidgetDetail::ApplyOpacity(presentation.backgroundColor, context.opacity * style.opacity);
			const SdColor border = BasicWidgetDetail::ApplyOpacity(presentation.border.left.color, context.opacity * style.opacity);
			const SdColor textColor = BasicWidgetDetail::ApplyOpacity(presentation.color, context.opacity * style.opacity);
			const SdColor titleColor = BasicWidgetDetail::ApplyOpacity(
				context.instance.GetStyleSystem().GetTheme().GetColorVariable(SdThemeVariableLiteral("button.bg")),
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
			context.widgetState.targetTypeId = TargetTypeId;
			context.widgetState.layerPriority = SdLayerPriority::Floating;
			context.widgetState.inputEnabled = open;
			context.widgetState.layoutWeight = open ? 1.0f : 0.0f;
			context.EnsurePart(Parts::Titlebar);
			context.EnsurePart(Parts::Title);
			context.EnsurePart(Parts::CloseButton);
			context.EnsurePart(Parts::Content);
			context.EnsurePart(Parts::ResizeHandle);
			if (!open)
			{
				state.dragging = false;
				return;
			}

			const Style& style = context.instance.GetResolvedStyle<SdWindow>(context.id);
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
		static constexpr SdStyleId TargetTypeId = SdWidgetTargetIds::ImageViewer;

		struct Style final
		{
			static Style Default(const SdStyleContext& context)
			{
				(void)context;
				Style style = {};
				return style;
			}

			static void Describe(SdStyleContract<Style>& contract)
			{
				(void)contract;
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
			context.widgetState.targetTypeId = TargetTypeId;
			context.widgetState.inputEnabled = false;
			context.widgetState.layoutWeight = 1.0f;
		}

		void OnLayout(SdLayoutContext& context)
		{
			const State& state = context.State<State>();
			const SdBoxStyle& rootStyle = context.RootStyleNode().resolvedStyle;
			const SdResolvedBoxStyle usedStyle = SdResolveBoxStyle(rootStyle, context.constraints.maxSize, { 160.0f, 120.0f });
			const SdVec2 imageSize = {
				state.size.x > 0.0f ? state.size.x : usedStyle.width,
				state.size.y > 0.0f ? state.size.y : usedStyle.height
			};
			context.SetDesiredSize({
				imageSize.x + usedStyle.padding.left + usedStyle.padding.right,
				imageSize.y + usedStyle.padding.top + usedStyle.padding.bottom
			});
		}

		void OnPaint(SdPaintContext& context)
		{
			const State& state = context.State<State>();
			const SdBoxStyle& presentation = context.RootStyleNode().presentationStyle;
			const SdResolvedBoxStyle usedStyle = SdResolveBoxStyle(presentation, context.animatedRect.Size(), { 160.0f, 120.0f });
			const SdColor background = BasicWidgetDetail::ApplyOpacity(presentation.backgroundColor, context.opacity * presentation.opacity);
			const SdColor tint = BasicWidgetDetail::ApplyOpacity(state.tint, context.opacity * presentation.opacity);
			const float radius = SdResolveLength(presentation.radius, context.animatedRect.Width());
			context.renderList.AddRectFilled(context.animatedRect, background, context.clipRect, radius);
			const SdRect imageRect = BasicWidgetDetail::InsetRect(context.animatedRect, {
				usedStyle.padding.left,
				usedStyle.padding.top,
				usedStyle.padding.right,
				usedStyle.padding.bottom
			});
			if (state.texture.IsValid())
				context.renderList.AddImage(state.texture, imageRect, state.uvRect, tint, context.clipRect);
			context.renderList.AddRect(context.animatedRect, presentation.border.left.color, context.clipRect, 1.0f, radius);
		}
	};

	struct SdScrollView final : SdWidgetTag
	{
		static constexpr SdStyleId TargetTypeId = SdWidgetTargetIds::ScrollView;

		struct Style final
		{
			float childSpacing = 6.0f;
			float scrollbarWidth = 5.0f;

			static Style Default(const SdStyleContext& context)
			{
				Style style = {};
				const float smallSpacing = context.theme.GetMetricVariable(SdThemeVariableLiteral("spacing.small"));
				style.childSpacing = smallSpacing;
				style.scrollbarWidth = 5.0f;
				return style;
			}

			static void Describe(SdStyleContract<Style>& contract)
			{
				contract.Layout(&Style::childSpacing);
				contract.Layout(&Style::scrollbarWidth);
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
			context.widgetState.targetTypeId = TargetTypeId;
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
			const Style& style = context.RootResolvedStyle<SdScrollView>();
			const SdBoxStyle& rootStyle = context.RootStyleNode().resolvedStyle;
			const SdResolvedBoxStyle usedStyle = SdResolveBoxStyle(rootStyle, context.constraints.maxSize, { 240.0f, 160.0f });
			context.SetDesiredSize({ std::max(0.0f, usedStyle.width), std::max(0.0f, usedStyle.height) });
			context.widgetState.arrangeChildren = true;
			context.widgetState.clipChildren = true;
			context.widgetState.childPadding = {
				usedStyle.padding.left,
				usedStyle.padding.top,
				usedStyle.padding.right,
				usedStyle.padding.bottom
			};
			context.widgetState.childSpacing = std::max(0.0f, style.childSpacing);
		}

		void OnPaint(SdPaintContext& context)
		{
			const State& state = context.State<State>();
			const Style& style = context.RootPresentationStyle<SdScrollView>();
			const SdBoxStyle& presentation = context.RootStyleNode().presentationStyle;
			const SdColor background = BasicWidgetDetail::ApplyOpacity(presentation.backgroundColor, context.opacity * presentation.opacity);
			const SdColor border = BasicWidgetDetail::ApplyOpacity(presentation.border.left.color, context.opacity * presentation.opacity);
			const SdColor accent = BasicWidgetDetail::ApplyOpacity(
				context.instance.GetStyleSystem().GetTheme().GetColorVariable(SdThemeVariableLiteral("accent")),
				context.opacity * presentation.opacity);
			const float radius = SdResolveLength(presentation.radius, context.animatedRect.Width());
			context.renderList.AddRectFilled(context.animatedRect, background, context.clipRect, radius);
			context.renderList.AddRect(context.animatedRect, border, context.clipRect, 1.0f, radius);

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
		static constexpr SdStyleId TargetTypeId = SdWidgetTargetIds::Popup;

		struct Style final
		{
			float childSpacing = 6.0f;

			static Style Default(const SdStyleContext& context)
			{
				Style style = {};
				const float smallSpacing = context.theme.GetMetricVariable(SdThemeVariableLiteral("spacing.small"));
				style.childSpacing = smallSpacing;
				return style;
			}

			static void Describe(SdStyleContract<Style>& contract)
			{
				contract.Layout(&Style::childSpacing);
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
			const Style& style = context.RootResolvedStyle<SdPopup>();
			const SdBoxStyle& rootStyle = context.RootStyleNode().resolvedStyle;
			const SdResolvedBoxStyle usedStyle = SdResolveBoxStyle(rootStyle, context.constraints.maxSize, { 220.0f, 140.0f });
			context.widgetState.manualLayout = true;
			context.widgetState.manualRect = state.open
				? BasicWidgetDetail::MakeRect(state.position, { usedStyle.width, usedStyle.height })
				: SdRect{};
			context.widgetState.arrangeChildren = state.open;
			context.widgetState.clipChildren = true;
			context.widgetState.childPadding = {
				usedStyle.padding.left,
				usedStyle.padding.top,
				usedStyle.padding.right,
				usedStyle.padding.bottom
			};
			context.widgetState.childSpacing = style.childSpacing;
			context.SetDesiredSize(state.open ? SdVec2{ usedStyle.width, usedStyle.height } : SdVec2{});
		}

		void OnPaint(SdPaintContext& context)
		{
			const State& state = context.State<State>();
			if (!state.open)
				return;
			const SdBoxStyle& presentation = context.RootStyleNode().presentationStyle;
			const float radius = SdResolveLength(presentation.radius, context.animatedRect.Width());
			context.renderList.AddRectFilled(
				context.animatedRect,
				BasicWidgetDetail::ApplyOpacity(presentation.backgroundColor, context.opacity * presentation.opacity),
				context.clipRect,
				radius);
			context.renderList.AddRect(context.animatedRect, presentation.border.left.color, context.clipRect, 1.0f, radius);
		}

	protected:
		static void Configure(SdUpdateContext& context, bool open)
		{
			context.widgetState.targetTypeId = TargetTypeId;
			context.widgetState.layerPriority = SdLayerPriority::Popup;
			context.widgetState.inputEnabled = open;
			context.widgetState.layoutWeight = open ? 1.0f : 0.0f;
		}
	};

	struct SdContextMenu final : SdWidgetTag
	{
		static constexpr SdStyleId TargetTypeId = SdWidgetTargetIds::ContextMenu;

		struct Style final
		{
			float childSpacing = 6.0f;

			static Style Default(const SdStyleContext& context)
			{
				Style style = {};
				const float smallSpacing = context.theme.GetMetricVariable(SdThemeVariableLiteral("spacing.small"));
				style.childSpacing = smallSpacing;
				return style;
			}

			static void Describe(SdStyleContract<Style>& contract)
			{
				contract.Layout(&Style::childSpacing);
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
			context.widgetState.targetTypeId = TargetTypeId;
			context.widgetState.layerPriority = SdLayerPriority::Popup;
			context.widgetState.inputEnabled = open;
			context.widgetState.layoutWeight = open ? 1.0f : 0.0f;
			if (open)
				std::forward<TContent>(content)(context.ui);
		}

		void OnLayout(SdLayoutContext& context)
		{
			const State& state = context.State<State>();
			const Style& style = context.RootResolvedStyle<SdContextMenu>();
			const SdBoxStyle& rootStyle = context.RootStyleNode().resolvedStyle;
			const SdResolvedBoxStyle usedStyle = SdResolveBoxStyle(rootStyle, context.constraints.maxSize, { 220.0f, 140.0f });
			context.widgetState.manualLayout = true;
			context.widgetState.manualRect = state.open
				? BasicWidgetDetail::MakeRect(state.position, { usedStyle.width, usedStyle.height })
				: SdRect{};
			context.widgetState.arrangeChildren = state.open;
			context.widgetState.clipChildren = true;
			context.widgetState.childPadding = {
				usedStyle.padding.left,
				usedStyle.padding.top,
				usedStyle.padding.right,
				usedStyle.padding.bottom
			};
			context.widgetState.childSpacing = style.childSpacing;
			context.SetDesiredSize(state.open ? SdVec2{ usedStyle.width, usedStyle.height } : SdVec2{});
		}

		void OnPaint(SdPaintContext& context)
		{
			const State& state = context.State<State>();
			if (!state.open)
				return;
			const SdBoxStyle& presentation = context.RootStyleNode().presentationStyle;
			const float radius = SdResolveLength(presentation.radius, context.animatedRect.Width());
			context.renderList.AddRectFilled(
				context.animatedRect,
				BasicWidgetDetail::ApplyOpacity(presentation.backgroundColor, context.opacity * presentation.opacity),
				context.clipRect,
				radius);
			context.renderList.AddRect(context.animatedRect, presentation.border.left.color, context.clipRect, 1.0f, radius);
		}
	};

	struct SdTooltip final : SdWidgetTag
	{
		static constexpr SdStyleId TargetTypeId = SdWidgetTargetIds::Tooltip;

		struct Style final
		{
			SdSpacing padding = { 7.0f, 5.0f, 7.0f, 5.0f };
			float fontSize = 16.0f;
			float lineHeight = 0.0f;
			float radius = 5.0f;
			float opacity = 1.0f;

			static Style Default(const SdStyleContext& context)
			{
				Style style = {};
				const float smallSpacing = context.theme.GetMetricVariable(SdThemeVariableLiteral("spacing.small"));
				style.padding = { smallSpacing, smallSpacing, smallSpacing, smallSpacing };
				style.fontSize = BasicWidgetDetail::kDefaultFontSize;
				style.lineHeight = 0.0f;
				style.radius = context.theme.GetMetricVariable(SdThemeVariableLiteral("radius.small"));
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
			context.widgetState.targetTypeId = TargetTypeId;
			context.widgetState.layerPriority = SdLayerPriority::Overlay;
			context.widgetState.inputEnabled = false;
			context.widgetState.layoutWeight = visible ? 1.0f : 0.0f;
		}

		void OnLayout(SdLayoutContext& context)
		{
			const State& state = context.State<State>();
			const Style& style = context.RootResolvedStyle<SdTooltip>();
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
			const Style& style = context.RootPresentationStyle<SdTooltip>();
			const SdBoxStyle& presentation = context.RootStyleNode().presentationStyle;
			const SdTextStyle textStyle = BasicWidgetDetail::BuildTextStyle({}, style.fontSize, style.lineHeight);
			const SdColor background = BasicWidgetDetail::ApplyOpacity(presentation.backgroundColor, context.opacity * style.opacity);
			const SdColor border = BasicWidgetDetail::ApplyOpacity(presentation.border.left.color, context.opacity * style.opacity);
			const SdColor color = BasicWidgetDetail::ApplyOpacity(presentation.color, context.opacity * style.opacity);
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
