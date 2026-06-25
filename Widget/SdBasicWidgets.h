#pragma once

#include "Core/SdRuntime.h"
#include "Core/SdText.h"
#include "Core/SdUtf8.h"

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

		inline SdRect PaintRect(const SdPaintContext& context) noexcept
		{
			const SdRect& borderBox = context.rootLayoutBox.borderBox;
			return borderBox.Width() > 0.0f || borderBox.Height() > 0.0f
				? borderBox
				: context.animatedRect;
		}

		inline SdRect StyleNodeRect(const SdStyleNode& node, const SdRect& fallback) noexcept
		{
			if (node.layoutBox.borderBox.Width() > 0.0f || node.layoutBox.borderBox.Height() > 0.0f)
				return node.layoutBox.borderBox;
			if (node.usedBox.borderBox.Width() > 0.0f || node.usedBox.borderBox.Height() > 0.0f)
				return node.usedBox.borderBox;
			return fallback;
		}

		inline SdRect StyleNodeUsedRect(const SdStyleNode& node, const SdRect& fallback = {}) noexcept
		{
			if (node.usedBox.borderBox.Width() > 0.0f || node.usedBox.borderBox.Height() > 0.0f)
				return node.usedBox.borderBox;
			if (node.layoutBox.borderBox.Width() > 0.0f || node.layoutBox.borderBox.Height() > 0.0f)
				return node.layoutBox.borderBox;
			return fallback;
		}

		inline SdRect StyleNodeArrangeRect(const SdStyleNode& node, const SdRect& fallback = {}) noexcept
		{
			SdRect result = StyleNodeRect(node, fallback);
			const SdRect usedRect = StyleNodeUsedRect(node, result);
			if (usedRect.Width() > result.Width())
				result.max.x = result.min.x + usedRect.Width();
			if (usedRect.Height() > result.Height())
				result.max.y = result.min.y + usedRect.Height();
			return result;
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

		struct SdMeasuredTextLayout final
		{
			SdVec2 layoutSize = {};
			SdRect visibleBounds = {};
		};

		inline SdRect SizeRect(SdVec2 size) noexcept
		{
			return { 0.0f, 0.0f, std::max(0.0f, size.x), std::max(0.0f, size.y) };
		}

		inline SdMeasuredTextLayout MeasureTextLayout(SdInstance& instance, SdUtf8StringView text, const SdTextStyle& textStyle)
		{
			SdMeasuredTextLayout metrics = {};
			metrics.layoutSize = EstimateTextSize(text, textStyle);
			metrics.visibleBounds = SizeRect(metrics.layoutSize);
			if (text.empty())
				return metrics;

			ISdFontBackend* fontBackend = instance.GetFontBackend();
			if (!fontBackend)
				return metrics;

			metrics.layoutSize = fontBackend->MeasureText(text, textStyle);
			metrics.visibleBounds = SizeRect(metrics.layoutSize);
			const SdParagraphLayout layout = fontBackend->BuildParagraphLayout(text, textStyle, 0.0f, SdColorWhite);
			if (layout.size.x > 0.0f || layout.size.y > 0.0f)
			{
				metrics.layoutSize = layout.size;
				metrics.visibleBounds = SizeRect(metrics.layoutSize);
			}
			bool hasVisibleBounds = false;
			SdRect visibleBounds = {};
			for (const SdTextGlyph& glyph : layout.glyphs)
			{
				if (glyph.rect.Width() <= 0.0f || glyph.rect.Height() <= 0.0f)
					continue;
				if (!hasVisibleBounds)
				{
					visibleBounds = glyph.rect;
					hasVisibleBounds = true;
					continue;
				}
				visibleBounds.min.x = std::min(visibleBounds.min.x, glyph.rect.min.x);
				visibleBounds.min.y = std::min(visibleBounds.min.y, glyph.rect.min.y);
				visibleBounds.max.x = std::max(visibleBounds.max.x, glyph.rect.max.x);
				visibleBounds.max.y = std::max(visibleBounds.max.y, glyph.rect.max.y);
			}
			if (hasVisibleBounds)
				metrics.visibleBounds = visibleBounds;
			return metrics;
		}

		inline SdVec2 AlignTextLayoutOrigin(
			const SdRect& rect,
			const SdMeasuredTextLayout& metrics,
			bool centerX,
			bool centerY) noexcept
		{
			const float x = centerX
				? rect.min.x + (rect.Width() - metrics.visibleBounds.Width()) * 0.5f
				: rect.min.x;
			const float y = centerY
				? rect.min.y + (rect.Height() - metrics.visibleBounds.Height()) * 0.5f
				: rect.min.y;
			return {
				x - metrics.visibleBounds.min.x,
				y - metrics.visibleBounds.min.y
			};
		}

		inline SdVec2 CenterTextLayoutOrigin(const SdRect& rect, const SdMeasuredTextLayout& metrics) noexcept
		{
			return AlignTextLayoutOrigin(rect, metrics, true, true);
		}

		inline SdVec2 LeftCenterTextLayoutOrigin(const SdRect& rect, const SdMeasuredTextLayout& metrics) noexcept
		{
			return AlignTextLayoutOrigin(rect, metrics, false, true);
		}

		inline SdVec2 LeftTopTextLayoutOrigin(const SdRect& rect, const SdMeasuredTextLayout& metrics) noexcept
		{
			return AlignTextLayoutOrigin(rect, metrics, false, false);
		}

		inline SdRect TextLayoutRect(const SdVec2& origin, const SdMeasuredTextLayout& metrics) noexcept
		{
			return { origin, origin + metrics.layoutSize };
		}

		inline SdVec2 TextVisualSize(const SdMeasuredTextLayout& metrics) noexcept
		{
			return {
				std::max(metrics.layoutSize.x, metrics.visibleBounds.Width()),
				metrics.visibleBounds.Height()
			};
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
		static constexpr SdTypeId TypeId = "Sodium.Widget.Text"_SdId;
		static constexpr SdTypeId TargetTypeId = SdWidgetTargetIds::Text;
		using Style = SdWidgetRootStyle;

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
			const SdBoxStyle& rootStyle = context.RootStyleNode().resolvedStyle;
			const SdResolvedBoxStyle usedStyle = SdResolveBoxStyle(rootStyle, context.constraints.maxSize, {});
			const SdTextStyle textStyle = BasicWidgetDetail::BuildTextStyle(state.textStyle, rootStyle.fontSize, rootStyle.lineHeight);
			SdVec2 desiredSize = BasicWidgetDetail::TextVisualSize(
				BasicWidgetDetail::MeasureTextLayout(context.instance, state.text, textStyle));

			desiredSize.x += usedStyle.padding.left + usedStyle.padding.right;
			desiredSize.y += usedStyle.padding.top + usedStyle.padding.bottom;
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

			const SdBoxStyle& presentation = context.RootStyleNode().presentationStyle;
			const SdRect paintRect = BasicWidgetDetail::PaintRect(context);
			const SdResolvedBoxStyle usedStyle = SdResolveBoxStyle(presentation, paintRect.Size(), {});
			const SdTextStyle textStyle = BasicWidgetDetail::BuildTextStyle(state.textStyle, presentation.fontSize, presentation.lineHeight);
			const SdColor color = BasicWidgetDetail::ApplyOpacity(
				presentation.color,
				context.opacity * presentation.opacity);
			const SdRect contentRect = SdInsetRect(paintRect, usedStyle.padding);
			const BasicWidgetDetail::SdMeasuredTextLayout textLayout = BasicWidgetDetail::MeasureTextLayout(context.instance, state.text, textStyle);
			const SdVec2 position = BasicWidgetDetail::LeftTopTextLayoutOrigin(contentRect, textLayout);
			context.renderList.PushClipRect(context.clipRect);
			context.renderList.AddText(state.text, textStyle, position, color);
			context.renderList.PopClip();
		}
	};

	struct SdPanel final : SdWidgetTag
	{
		static constexpr SdTypeId TypeId = "Sodium.Widget.Panel"_SdId;
		static constexpr SdTypeId TargetTypeId = SdWidgetTargetIds::Panel;
		using Style = SdWidgetRootStyle;

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
			const SdBoxStyle& rootStyle = context.RootStyleNode().resolvedStyle;
			const SdResolvedBoxStyle usedStyle = SdResolveBoxStyle(rootStyle, context.constraints.maxSize, { 240.0f, 120.0f });
			context.SetDesiredSize({
				std::max(0.0f, usedStyle.width),
				std::max(0.0f, usedStyle.height)
				});
			context.widgetState.arrangeChildren = true;
			context.widgetState.clipChildren = true;
		}

		void OnPaint(SdPaintContext& context)
		{
			const SdBoxStyle& presentation = context.RootStyleNode().presentationStyle;
			const SdRect paintRect = BasicWidgetDetail::PaintRect(context);
			const float radius = SdResolveLength(presentation.radius, paintRect.Width());
			context.renderList.PushClipRect(context.clipRect);
			context.renderList.AddRectFilled(
				paintRect,
				BasicWidgetDetail::ApplyOpacity(presentation.backgroundColor, context.opacity * presentation.opacity),
				radius);
			context.renderList.AddRect(
				paintRect,
				BasicWidgetDetail::ApplyOpacity(presentation.border.left.color, context.opacity * presentation.opacity),
				1.0f,
				radius);
			context.renderList.PopClip();
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
		static constexpr SdTypeId TypeId = "Sodium.Widget.Button"_SdId;
		static constexpr SdTypeId TargetTypeId = SdWidgetTargetIds::Button;

		struct Parts final
		{
			static constexpr SdStylePart Content = SdWidgetPartIds::Button::Content;
			static constexpr SdStylePart Icon = SdWidgetPartIds::Button::Icon;
			static constexpr SdStylePart Label = SdWidgetPartIds::Button::Label;
		};
		using Style = SdWidgetRootStyle;

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
			const SdVec2 textSize = BasicWidgetDetail::TextVisualSize(
				BasicWidgetDetail::MeasureTextLayout(context.instance, state.label, textStyle));

			context.SetDesiredSize({
				std::max(usedStyle.minWidth, textSize.x + usedStyle.padding.left + usedStyle.padding.right),
				std::max(usedStyle.minHeight, textSize.y + usedStyle.padding.top + usedStyle.padding.bottom)
				});
		}

		void OnArrange(SdArrangeContext& context)
		{
			const State& state = context.State<State>();
			const SdStyleNode& rootNode = context.RootStyleNode();
			const SdRect rootRect = BasicWidgetDetail::StyleNodeArrangeRect(rootNode);
			const SdBoxStyle& rootStyle = rootNode.resolvedStyle;
			const SdBoxStyle& labelStyle = context.Part(Parts::Label).resolvedStyle;
			const SdResolvedBoxStyle usedStyle = SdResolveBoxStyle(rootStyle, rootRect.Size(), {});
			const SdTextStyle textStyle = BasicWidgetDetail::BuildTextStyle({}, labelStyle.fontSize, labelStyle.lineHeight);
			const SdRect contentRect = SdInsetRect(rootRect, usedStyle.padding);
			const BasicWidgetDetail::SdMeasuredTextLayout textLayout = BasicWidgetDetail::MeasureTextLayout(context.instance, state.label, textStyle);
			const SdVec2 textOrigin = BasicWidgetDetail::CenterTextLayoutOrigin(contentRect, textLayout);
			const SdRect labelRect = BasicWidgetDetail::TextLayoutRect(textOrigin, textLayout);
			context.SetPartBorderBox(Parts::Content, rootRect);
			context.SetPartBorderBox(Parts::Label, labelRect);
		}

		void OnPaint(SdPaintContext& context)
		{
			const State& state = context.State<State>();
			const SdBoxStyle& presentation = context.RootStyleNode().presentationStyle;
			const SdStyleNode& contentNode = context.Part(Parts::Content);
			const SdStyleNode& labelNode = context.Part(Parts::Label);
			const SdBoxStyle& contentPresentation = contentNode.presentationStyle;
			const SdBoxStyle& labelPresentation = labelNode.presentationStyle;
			const SdRect paintRect = BasicWidgetDetail::StyleNodeRect(contentNode, BasicWidgetDetail::PaintRect(context));
			const SdRect labelRect = BasicWidgetDetail::StyleNodeRect(labelNode, paintRect);
			const SdTextStyle textStyle = BasicWidgetDetail::BuildTextStyle({}, labelPresentation.fontSize, labelPresentation.lineHeight);
			const SdColor background = BasicWidgetDetail::ApplyOpacity(contentPresentation.backgroundColor, context.opacity * contentPresentation.opacity);
			const SdColor border = BasicWidgetDetail::ApplyOpacity(contentPresentation.border.left.color, context.opacity * contentPresentation.opacity);
			const SdColor color = BasicWidgetDetail::ApplyOpacity(labelPresentation.color, context.opacity * labelPresentation.opacity);
			const SdColor innerShadowColor = BasicWidgetDetail::ApplyOpacity(SdColor(0, 0, 0, 72), context.opacity * contentPresentation.opacity);
			const float radius = SdResolveLength(contentPresentation.radius, paintRect.Width(), SdResolveLength(presentation.radius, paintRect.Width()));

			context.renderList.PushClipRect(context.clipRect);
			context.renderList.AddRectFilled(paintRect, background, radius);
			context.renderList.AddInnerShadow(paintRect, { 0.0f, 0.0f }, innerShadowColor, 10.0f, -5.0f, radius);
			context.renderList.AddRect(paintRect, border, 1.0f, radius);
			context.renderList.AddText(state.label, textStyle, labelRect.min, color);
			context.renderList.PopClip();
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
		static constexpr SdTypeId TypeId = "Sodium.Widget.CheckBox"_SdId;
		static constexpr SdTypeId TargetTypeId = SdWidgetTargetIds::CheckBox;

		struct Parts final
		{
			static constexpr SdStylePart Box = SdWidgetPartIds::CheckBox::Box;
			static constexpr SdStylePart Indicator = SdWidgetPartIds::CheckBox::Indicator;
			static constexpr SdStylePart Label = SdWidgetPartIds::CheckBox::Label;
		};

		struct Style final
		{
			float boxSize = 18.0f;

			static Style Default(const SdStyleContext&)
			{
				Style style = {};
				style.boxSize = 18.0f;
				return style;
			}

			static void Describe(SdStyleContract<Style>& contract)
			{
				contract.Layout(&Style::boxSize);
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
			const SdVec2 textSize = BasicWidgetDetail::TextVisualSize(
				BasicWidgetDetail::MeasureTextLayout(context.instance, state.label, textStyle));
			const float labelGap = std::max(0.0f, usedStyle.gap);

			context.SetDesiredSize({
				usedStyle.padding.left + style.boxSize + labelGap + textSize.x + usedStyle.padding.right,
				std::max(usedStyle.minHeight, std::max(style.boxSize, textSize.y) + usedStyle.padding.top + usedStyle.padding.bottom)
				});
		}

		void OnArrange(SdArrangeContext& context)
		{
			const State& state = context.State<State>();
			const Style& style = context.RootResolvedStyle<SdCheckBox>();
			const SdStyleNode& rootNode = context.RootStyleNode();
			const SdRect rootRect = BasicWidgetDetail::StyleNodeArrangeRect(rootNode);
			const SdBoxStyle& rootStyle = rootNode.resolvedStyle;
			const SdBoxStyle& labelStyle = context.Part(Parts::Label).resolvedStyle;
			const SdResolvedBoxStyle usedStyle = SdResolveBoxStyle(rootStyle, rootRect.Size(), {});
			const SdRect contentRect = SdInsetRect(rootRect, usedStyle.padding);
			const float labelGap = std::max(0.0f, usedStyle.gap);
			const float boxY = contentRect.min.y + (contentRect.Height() - style.boxSize) * 0.5f;
			const SdRect boxRect = {
				contentRect.min.x,
				boxY,
				contentRect.min.x + style.boxSize,
				boxY + style.boxSize
			};
			const SdRect indicatorRect = BasicWidgetDetail::InsetRect(boxRect, { 4.0f, 4.0f, 4.0f, 4.0f });

			if (!state.label.empty())
			{
				const SdTextStyle textStyle = BasicWidgetDetail::BuildTextStyle({}, labelStyle.fontSize, labelStyle.lineHeight);
				const BasicWidgetDetail::SdMeasuredTextLayout textLayout = BasicWidgetDetail::MeasureTextLayout(context.instance, state.label, textStyle);
				const SdRect labelArea = {
					boxRect.max.x + labelGap,
					contentRect.min.y,
					contentRect.max.x,
					contentRect.max.y
				};
				const SdVec2 textOrigin = BasicWidgetDetail::LeftCenterTextLayoutOrigin(labelArea, textLayout);
				context.SetPartBorderBox(Parts::Label, BasicWidgetDetail::TextLayoutRect(textOrigin, textLayout));
			}
			else
			{
				context.SetPartBorderBox(Parts::Label, {});
			}

			context.SetPartBorderBox(Parts::Box, boxRect);
			context.SetPartBorderBox(Parts::Indicator, indicatorRect);
		}

		void OnPaint(SdPaintContext& context)
		{
			const State& state = context.State<State>();
			const Style& style = context.RootPresentationStyle<SdCheckBox>();
			const SdBoxStyle& presentation = context.RootStyleNode().presentationStyle;
			const SdStyleNode& boxNode = context.Part(Parts::Box);
			const SdStyleNode& indicatorNode = context.Part(Parts::Indicator);
			const SdStyleNode& labelNode = context.Part(Parts::Label);
			const SdBoxStyle& boxPresentation = boxNode.presentationStyle;
			const SdBoxStyle& indicatorPresentation = indicatorNode.presentationStyle;
			const SdBoxStyle& labelPresentation = labelNode.presentationStyle;
			const SdRect paintRect = BasicWidgetDetail::PaintRect(context);
			const SdRect boxRect = BasicWidgetDetail::StyleNodeRect(boxNode, paintRect);
			const SdRect indicatorRect = BasicWidgetDetail::StyleNodeRect(indicatorNode, BasicWidgetDetail::InsetRect(boxRect, { 4.0f, 4.0f, 4.0f, 4.0f }));
			const SdRect labelRect = BasicWidgetDetail::StyleNodeRect(labelNode, paintRect);
			const SdTextStyle textStyle = BasicWidgetDetail::BuildTextStyle({}, labelPresentation.fontSize, labelPresentation.lineHeight);
			const SdColor background = BasicWidgetDetail::ApplyOpacity(boxPresentation.backgroundColor, context.opacity * boxPresentation.opacity);
			const SdColor border = BasicWidgetDetail::ApplyOpacity(boxPresentation.border.left.color, context.opacity * boxPresentation.opacity);
			const SdColor textColor = BasicWidgetDetail::ApplyOpacity(labelPresentation.color, context.opacity * labelPresentation.opacity);
			const SdColor indicatorColor = BasicWidgetDetail::ApplyOpacity(indicatorPresentation.backgroundColor, context.opacity * indicatorPresentation.opacity);
			const float radius = SdResolveLength(boxPresentation.radius, style.boxSize, SdResolveLength(presentation.radius, style.boxSize));

			context.renderList.PushClipRect(context.clipRect);
			context.renderList.AddRectFilled(boxRect, background, radius);
			context.renderList.AddRect(boxRect, border, 1.0f, radius);
			if (state.checked)
			{
				context.renderList.AddRectFilled(
					indicatorRect,
					indicatorColor,
					std::max(0.0f, radius - 2.0f));
			}

			if (!state.label.empty())
				context.renderList.AddText(state.label, textStyle, labelRect.min, textColor);
			context.renderList.PopClip();
		}

	private:
		static void Configure(SdUpdateContext& context)
		{
			context.widgetState.targetTypeId = TargetTypeId;
			context.widgetState.inputEnabled = true;
			context.widgetState.layoutWeight = 1.0f;
			context.EnsurePart(Parts::Box);
			context.EnsurePart(Parts::Indicator);
			context.EnsurePart(Parts::Label);
		}
	};

	using SdCheckbox = SdCheckBox;

	struct SdSliderFloat final : SdWidgetTag
	{
		static constexpr SdTypeId TypeId = "Sodium.Widget.Slider"_SdId;
		static constexpr SdTypeId TargetTypeId = SdWidgetTargetIds::Slider;

		struct Parts final
		{
			static constexpr SdStylePart Label = SdWidgetPartIds::Slider::Label;
			static constexpr SdStylePart Track = SdWidgetPartIds::Slider::Track;
			static constexpr SdStylePart Fill = SdWidgetPartIds::Slider::Fill;
			static constexpr SdStylePart Thumb = SdWidgetPartIds::Slider::Thumb;
		};

		struct Style final
		{
			float trackHeight = 6.0f;
			float thumbRadius = 8.0f;

			static Style Default(const SdStyleContext&)
			{
				Style style = {};
				style.trackHeight = 6.0f;
				style.thumbRadius = 8.0f;
				return style;
			}

			static void Describe(SdStyleContract<Style>& contract)
			{
				contract.Layout(&Style::trackHeight);
				contract.Layout(&Style::thumbRadius);
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
			(void)style;
			const SdBoxStyle& rootStyle = context.RootStyleNode().resolvedStyle;
			const SdResolvedBoxStyle usedStyle = SdResolveBoxStyle(rootStyle, context.constraints.maxSize, { 180.0f, 30.0f });
			SdVec2 labelSize = {};
			if (!state.label.empty())
			{
				const SdTextStyle textStyle = BasicWidgetDetail::BuildTextStyle({}, rootStyle.fontSize, rootStyle.lineHeight);
				labelSize = BasicWidgetDetail::TextVisualSize(
					BasicWidgetDetail::MeasureTextLayout(context.instance, state.label, textStyle));
			}
			const float labelGap = state.label.empty() ? 0.0f : std::max(0.0f, usedStyle.gap);

			context.SetDesiredSize({
				usedStyle.padding.left + labelSize.x + labelGap + usedStyle.width + usedStyle.padding.right,
				std::max(usedStyle.height, labelSize.y + usedStyle.padding.top + usedStyle.padding.bottom)
				});
		}

		void OnArrange(SdArrangeContext& context)
		{
			const State& state = context.State<State>();
			const Style& style = context.RootResolvedStyle<SdSliderFloat>();
			const SdStyleNode& rootNode = context.RootStyleNode();
			const SdRect rootRect = BasicWidgetDetail::StyleNodeArrangeRect(rootNode);
			const SdBoxStyle& rootStyle = rootNode.resolvedStyle;
			const SdBoxStyle& labelStyle = context.Part(Parts::Label).resolvedStyle;
			const SdResolvedBoxStyle usedStyle = SdResolveBoxStyle(rootStyle, rootRect.Size(), { 180.0f, 30.0f });
			const SdRect contentRect = SdInsetRect(rootRect, usedStyle.padding);
			const float labelGap = std::max(0.0f, usedStyle.gap);
			const SdTextStyle textStyle = BasicWidgetDetail::BuildTextStyle({}, labelStyle.fontSize, labelStyle.lineHeight);
			float trackStartX = contentRect.min.x;
			if (!state.label.empty())
			{
				const BasicWidgetDetail::SdMeasuredTextLayout textLayout = BasicWidgetDetail::MeasureTextLayout(context.instance, state.label, textStyle);
				const SdRect labelArea = {
					trackStartX,
					contentRect.min.y,
					contentRect.max.x,
					contentRect.max.y
				};
				const SdVec2 textOrigin = BasicWidgetDetail::LeftCenterTextLayoutOrigin(labelArea, textLayout);
				const SdRect labelRect = BasicWidgetDetail::TextLayoutRect(textOrigin, textLayout);
				context.SetPartBorderBox(Parts::Label, labelRect);
				trackStartX = textOrigin.x + textLayout.visibleBounds.max.x + labelGap;
			}
			else
			{
				context.SetPartBorderBox(Parts::Label, {});
			}

			const float trackCenterY = contentRect.min.y + (contentRect.Height() * 0.5f);
			const float trackWidth = std::max(1.0f, std::min(std::max(1.0f, usedStyle.width), contentRect.max.x - trackStartX));
			const SdRect trackRect = {
				trackStartX,
				trackCenterY - (style.trackHeight * 0.5f),
				trackStartX + trackWidth,
				trackCenterY + (style.trackHeight * 0.5f)
			};
			const float t = BasicWidgetDetail::Normalize(state.value, state.minValue, state.maxValue);
			const float thumbX = BasicWidgetDetail::Lerp(trackRect.min.x, trackRect.max.x, t);
			const SdRect fillRect = { trackRect.min.x, trackRect.min.y, thumbX, trackRect.max.y };
			const SdRect thumbRect = {
				thumbX - style.thumbRadius,
				trackCenterY - style.thumbRadius,
				thumbX + style.thumbRadius,
				trackCenterY + style.thumbRadius
			};
			context.SetPartBorderBox(Parts::Track, trackRect);
			context.SetPartBorderBox(Parts::Fill, fillRect);
			context.SetPartBorderBox(Parts::Thumb, thumbRect);
		}

		void OnPaint(SdPaintContext& context)
		{
			const State& state = context.State<State>();
			const Style& style = context.RootPresentationStyle<SdSliderFloat>();
			const SdBoxStyle& presentation = context.RootStyleNode().presentationStyle;
			const SdStyleNode& labelNode = context.Part(Parts::Label);
			const SdStyleNode& trackNode = context.Part(Parts::Track);
			const SdStyleNode& fillNode = context.Part(Parts::Fill);
			const SdStyleNode& thumbNode = context.Part(Parts::Thumb);
			const SdBoxStyle& labelPresentation = labelNode.presentationStyle;
			const SdBoxStyle& trackPresentation = trackNode.presentationStyle;
			const SdBoxStyle& fillPresentation = fillNode.presentationStyle;
			const SdBoxStyle& thumbPresentation = thumbNode.presentationStyle;
			const SdRect paintRect = BasicWidgetDetail::PaintRect(context);
			const SdRect trackRect = BasicWidgetDetail::StyleNodeRect(trackNode, paintRect);
			const SdRect fillRect = BasicWidgetDetail::StyleNodeRect(fillNode, trackRect);
			const SdRect thumbRect = BasicWidgetDetail::StyleNodeRect(thumbNode, trackRect);
			const SdRect labelRect = BasicWidgetDetail::StyleNodeRect(labelNode, paintRect);
			const SdTextStyle textStyle = BasicWidgetDetail::BuildTextStyle({}, labelPresentation.fontSize, labelPresentation.lineHeight);
			const SdColor textColor = BasicWidgetDetail::ApplyOpacity(labelPresentation.color, context.opacity * labelPresentation.opacity);
			const SdColor trackColor = BasicWidgetDetail::ApplyOpacity(trackPresentation.backgroundColor, context.opacity * trackPresentation.opacity);
			const SdColor trackBorder = BasicWidgetDetail::ApplyOpacity(trackPresentation.border.left.color, context.opacity * trackPresentation.opacity);
			const SdColor fillColor = BasicWidgetDetail::ApplyOpacity(fillPresentation.backgroundColor, context.opacity * fillPresentation.opacity);
			const SdColor thumbColor = BasicWidgetDetail::ApplyOpacity(thumbPresentation.backgroundColor, context.opacity * thumbPresentation.opacity);
			const SdColor thumbBorder = BasicWidgetDetail::ApplyOpacity(thumbPresentation.border.left.color, context.opacity * thumbPresentation.opacity);
			const float radius = SdResolveLength(trackPresentation.radius, trackRect.Width(), SdResolveLength(presentation.radius, trackRect.Width()));

			context.renderList.PushClipRect(context.clipRect);
			if (!state.label.empty())
				context.renderList.AddText(state.label, textStyle, labelRect.min, textColor);
			context.renderList.AddRectFilled(trackRect, trackColor, radius);
			context.renderList.AddRectFilled(fillRect, fillColor, radius);
			context.renderList.AddRect(trackRect, trackBorder, 1.0f, radius);
			const SdVec2 thumbCenter = {
				(thumbRect.min.x + thumbRect.max.x) * 0.5f,
				(thumbRect.min.y + thumbRect.max.y) * 0.5f
			};
			const float thumbRadius = thumbRect.Width() > 0.0f ? thumbRect.Width() * 0.5f : style.thumbRadius;
			context.renderList.AddCircleFilled(thumbCenter, thumbRadius, thumbColor);
			context.renderList.AddCircle(thumbCenter, thumbRadius, thumbBorder, 1.0f);
			context.renderList.PopClip();
		}

	private:
		static void Configure(SdUpdateContext& context)
		{
			context.widgetState.targetTypeId = TargetTypeId;
			context.widgetState.inputEnabled = true;
			context.widgetState.layoutWeight = 1.0f;
			context.EnsurePart(Parts::Label);
			context.EnsurePart(Parts::Track);
			context.EnsurePart(Parts::Fill);
			context.EnsurePart(Parts::Thumb);
		}

		static float PositionToValue(SdUpdateContext& context, float x, float minValue, float maxValue)
		{
			const State& state = context.State<State>();
			const SdStyleNode& rootNode = context.instance.GetRootStyleNode(context.id);
			const SdRect rootRect = BasicWidgetDetail::StyleNodeArrangeRect(rootNode, context.widgetState.targetRect);
			const SdBoxStyle& rootStyle = rootNode.resolvedStyle;
			const SdResolvedBoxStyle usedStyle = SdResolveBoxStyle(rootStyle, rootRect.Size(), { 180.0f, 30.0f });
			float trackMin = rootRect.min.x + usedStyle.padding.left;
			if (!state.label.empty())
			{
				const SdBoxStyle& labelStyle = context.instance.GetStylePart(context.id, Parts::Label).resolvedStyle;
				const SdTextStyle textStyle = BasicWidgetDetail::BuildTextStyle({}, labelStyle.fontSize, labelStyle.lineHeight);
				const BasicWidgetDetail::SdMeasuredTextLayout textLayout = BasicWidgetDetail::MeasureTextLayout(context.instance, state.label, textStyle);
				trackMin += textLayout.visibleBounds.Width() + std::max(0.0f, usedStyle.gap);
			}
			const float contentMaxX = rootRect.max.x - usedStyle.padding.right;
			const float trackWidth = std::max(1.0f, std::min(std::max(1.0f, usedStyle.width), contentMaxX - trackMin));
			const float trackMax = trackMin + trackWidth;
			const float t = std::clamp((x - trackMin) / (trackMax - trackMin), 0.0f, 1.0f);
			return BasicWidgetDetail::Lerp(minValue, maxValue, t);
		}
	};

	struct SdTextInput final : SdWidgetTag
	{
		static constexpr SdTypeId TypeId = "Sodium.Widget.TextInput"_SdId;
		static constexpr SdTypeId TargetTypeId = SdWidgetTargetIds::TextInput;

		struct Parts final
		{
			static constexpr SdStylePart Field = SdWidgetPartIds::TextInput::Field;
			static constexpr SdStylePart Value = SdWidgetPartIds::TextInput::Value;
			static constexpr SdStylePart Placeholder = SdWidgetPartIds::TextInput::Placeholder;
			static constexpr SdStylePart Selection = SdWidgetPartIds::TextInput::Selection;
			static constexpr SdStylePart Caret = SdWidgetPartIds::TextInput::Caret;
		};
		using Style = SdWidgetRootStyle;

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

				const SdStyleNode& rootNode = context.instance.GetRootStyleNode(context.id);
				const SdRect rootRect = BasicWidgetDetail::StyleNodeArrangeRect(rootNode, context.widgetState.targetRect);
				const SdBoxStyle& rootStyle = rootNode.resolvedStyle;
				const SdResolvedBoxStyle usedStyle = SdResolveBoxStyle(rootStyle, rootRect.Size(), {});
				const SdTextStyle textStyle = BasicWidgetDetail::BuildTextStyle({}, rootStyle.fontSize, rootStyle.lineHeight);
				const float lineHeight = BasicWidgetDetail::ResolveLineHeight(textStyle);
				const BasicWidgetDetail::SdMeasuredTextLayout textLayout = BasicWidgetDetail::MeasureTextLayout(context.instance, value, textStyle);
				const SdRect contentRect = SdInsetRect(rootRect, usedStyle.padding);
				const SdVec2 textPosition = BasicWidgetDetail::LeftCenterTextLayoutOrigin(contentRect, textLayout);
				const SdRect caretRect = {
					textPosition.x + textLayout.layoutSize.x,
					textPosition.y,
					textPosition.x + textLayout.layoutSize.x + 1.0f,
					textPosition.y + lineHeight
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

		void OnArrange(SdArrangeContext& context)
		{
			const State& state = context.State<State>();
			const SdStyleNode& rootNode = context.RootStyleNode();
			const SdRect rootRect = BasicWidgetDetail::StyleNodeArrangeRect(rootNode);
			const SdBoxStyle& rootStyle = rootNode.resolvedStyle;
			const bool showPlaceholder = state.text.empty() && state.composition.empty() && !state.placeholder.empty();
			const SdBoxStyle& textStyleNode = context.Part(showPlaceholder ? Parts::Placeholder : Parts::Value).resolvedStyle;
			const SdResolvedBoxStyle usedStyle = SdResolveBoxStyle(rootStyle, rootRect.Size(), {});
			const SdTextStyle textStyle = BasicWidgetDetail::BuildTextStyle({}, textStyleNode.fontSize, textStyleNode.lineHeight);
			const float lineHeight = BasicWidgetDetail::ResolveLineHeight(textStyle);
			SdUtf8String paintText = showPlaceholder ? state.placeholder : state.text;
			if (!state.composition.empty())
				paintText += state.composition;
			const BasicWidgetDetail::SdMeasuredTextLayout textLayout = BasicWidgetDetail::MeasureTextLayout(context.instance, paintText, textStyle);
			const BasicWidgetDetail::SdMeasuredTextLayout caretTextLayout = BasicWidgetDetail::MeasureTextLayout(context.instance, state.text, textStyle);
			const SdRect contentRect = SdInsetRect(rootRect, usedStyle.padding);
			const SdVec2 textPosition = BasicWidgetDetail::LeftCenterTextLayoutOrigin(contentRect, textLayout);
			const SdRect textRect = BasicWidgetDetail::TextLayoutRect(textPosition, textLayout);
			const SdRect caretRect = {
				textPosition.x + caretTextLayout.layoutSize.x + 1.0f,
				textPosition.y,
				textPosition.x + caretTextLayout.layoutSize.x + 2.0f,
				textPosition.y + lineHeight
			};
			context.SetPartBorderBox(Parts::Field, rootRect);
			context.SetPartBorderBox(Parts::Value, textRect);
			context.SetPartBorderBox(Parts::Placeholder, textRect);
			context.SetPartBorderBox(Parts::Caret, caretRect);
			context.SetPartBorderBox(Parts::Selection, {});
		}

		void OnPaint(SdPaintContext& context)
		{
			const State& state = context.State<State>();
			const SdBoxStyle& presentation = context.RootStyleNode().presentationStyle;
			const SdStyleNode& fieldNode = context.Part(Parts::Field);
			const SdStyleNode& valueNode = context.Part(Parts::Value);
			const SdStyleNode& placeholderNode = context.Part(Parts::Placeholder);
			const SdStyleNode& caretNode = context.Part(Parts::Caret);
			const SdBoxStyle& fieldPresentation = fieldNode.presentationStyle;
			const SdBoxStyle& valuePresentation = valueNode.presentationStyle;
			const SdBoxStyle& placeholderPresentation = placeholderNode.presentationStyle;
			const SdBoxStyle& caretPresentation = caretNode.presentationStyle;
			const SdRect paintRect = BasicWidgetDetail::StyleNodeRect(fieldNode, BasicWidgetDetail::PaintRect(context));
			const bool showPlaceholder = state.text.empty() && state.composition.empty() && !state.placeholder.empty();
			const SdBoxStyle& textPresentation = showPlaceholder ? placeholderPresentation : valuePresentation;
			const SdStyleNode& textNode = showPlaceholder ? placeholderNode : valueNode;
			const SdRect textRect = BasicWidgetDetail::StyleNodeRect(textNode, paintRect);
			const SdRect caretRect = BasicWidgetDetail::StyleNodeRect(caretNode, textRect);
			const SdTextStyle textStyle = BasicWidgetDetail::BuildTextStyle({}, textPresentation.fontSize, textPresentation.lineHeight);
			const SdColor background = BasicWidgetDetail::ApplyOpacity(fieldPresentation.backgroundColor, context.opacity * fieldPresentation.opacity);
			const SdColor border = BasicWidgetDetail::ApplyOpacity(fieldPresentation.border.left.color, context.opacity * fieldPresentation.opacity);
			const SdColor textColor = BasicWidgetDetail::ApplyOpacity(textPresentation.color, context.opacity * textPresentation.opacity);
			const SdColor caretColor = BasicWidgetDetail::ApplyOpacity(caretPresentation.color, context.opacity * caretPresentation.opacity);
			const float radius = SdResolveLength(fieldPresentation.radius, paintRect.Width(), SdResolveLength(presentation.radius, paintRect.Width()));

			context.renderList.PushClipRect(context.clipRect);
			context.renderList.AddRectFilled(paintRect, background, radius);
			context.renderList.AddRect(paintRect, border, 1.0f, radius);

			SdUtf8String paintText = showPlaceholder ? state.placeholder : state.text;
			if (!state.composition.empty())
				paintText += state.composition;

			context.renderList.AddText(
				paintText,
				textStyle,
				textRect.min,
				textColor);

			if (state.focused)
				context.renderList.AddRectFilled(caretRect, caretColor, radius);
			context.renderList.PopClip();
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
		float backgroundBlurRadius = 16.0f;
		SdVec2 shadowOffset = { 0.0f, 0.0f };
		SdColor shadowColor = { 0, 0, 0, 144 };
		float shadowRadius = 20.0f;
		float shadowSpread = 0.0f;
		bool closable = true;
		bool draggable = true;
	};

	struct SdWindow final : SdWidgetTag
	{
		static constexpr SdTypeId TypeId = "Sodium.Widget.Window"_SdId;
		static constexpr SdTypeId TargetTypeId = SdWidgetTargetIds::Window;

		struct Parts final
		{
			static constexpr SdStylePart Titlebar = SdWidgetPartIds::Window::Titlebar;
			static constexpr SdStylePart Title = SdWidgetPartIds::Window::Title;
			static constexpr SdStylePart CloseButton = SdWidgetPartIds::Window::CloseButton;
			static constexpr SdStylePart Content = SdWidgetPartIds::Window::Content;
			static constexpr SdStylePart ResizeHandle = SdWidgetPartIds::Window::ResizeHandle;
		};

		struct Style final
		{
			float titleHeight = 30.0f;

			static Style Default(const SdStyleContext&)
			{
				Style style = {};
				style.titleHeight = 30.0f;
				return style;
			}

			static void Describe(SdStyleContract<Style>& contract)
			{
				contract.Layout(&Style::titleHeight);
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
			const SdBoxStyle& rootStyle = context.RootStyleNode().resolvedStyle;
			const SdResolvedBoxStyle usedStyle = SdResolveBoxStyle(rootStyle, context.constraints.maxSize, { 420.0f, 260.0f });
			if (!state.initialized)
			{
				const SdVec2 size = {
					state.options.size.x > 0.0f ? state.options.size.x : usedStyle.width,
					state.options.size.y > 0.0f ? state.options.size.y : usedStyle.height
				};
				state.rect = BasicWidgetDetail::MakeRect(state.options.position, size);
				state.initialized = true;
			}

			context.widgetState.manualLayout = true;
			context.widgetState.manualRect = state.open ? state.rect : SdRect{};
			context.widgetState.arrangeChildren = state.open;
			context.widgetState.clipChildren = true;
			context.SetDesiredSize(state.open ? state.rect.Size() : SdVec2{});
		}

		void OnArrange(SdArrangeContext& context)
		{
			const State& state = context.State<State>();
			if (!state.open)
			{
				context.SetPartBorderBox(Parts::Titlebar, {});
				context.SetPartBorderBox(Parts::Title, {});
				context.SetPartBorderBox(Parts::CloseButton, {});
				context.SetPartBorderBox(Parts::Content, {});
				context.SetPartBorderBox(Parts::ResizeHandle, {});
				return;
			}

			const Style& style = context.RootResolvedStyle<SdWindow>();
			const SdStyleNode& rootNode = context.RootStyleNode();
			const SdRect rootRect = BasicWidgetDetail::StyleNodeArrangeRect(rootNode);
			const SdResolvedBoxStyle usedStyle = SdResolveBoxStyle(rootNode.resolvedStyle, rootRect.Size(), { 420.0f, 260.0f });
			const SdBoxStyle& titleStyle = context.Part(Parts::Title).resolvedStyle;
			const SdTextStyle textStyle = BasicWidgetDetail::BuildTextStyle({}, titleStyle.fontSize, titleStyle.lineHeight);
			const SdRect titlebarRect = {
				rootRect.min.x,
				rootRect.min.y,
				rootRect.max.x,
				std::min(rootRect.max.y, rootRect.min.y + style.titleHeight)
			};
			const SdRect closeRect = state.options.closable ? CloseRect(titlebarRect) : SdRect{};
			const BasicWidgetDetail::SdMeasuredTextLayout titleLayout = BasicWidgetDetail::MeasureTextLayout(context.instance, state.title, textStyle);
			const SdRect titleArea = {
				titlebarRect.min.x + usedStyle.padding.left,
				titlebarRect.min.y,
				state.options.closable ? std::max(titlebarRect.min.x + usedStyle.padding.left, closeRect.min.x - 6.0f) : titlebarRect.max.x - usedStyle.padding.right,
				titlebarRect.max.y
			};
			const SdVec2 titlePosition = BasicWidgetDetail::LeftCenterTextLayoutOrigin(titleArea, titleLayout);
			const SdRect titleRect = BasicWidgetDetail::TextLayoutRect(titlePosition, titleLayout);
			const SdRect resizeHandleRect = {
				rootRect.max.x - 12.0f,
				rootRect.max.y - 12.0f,
				rootRect.max.x,
				rootRect.max.y
			};
			context.SetPartBorderBox(Parts::Content, rootRect);
			context.SetPartBorderBox(Parts::Titlebar, titlebarRect);
			context.SetPartBorderBox(Parts::Title, titleRect);
			context.SetPartBorderBox(Parts::CloseButton, closeRect);
			context.SetPartBorderBox(Parts::ResizeHandle, resizeHandleRect);
		}

		void OnPaint(SdPaintContext& context)
		{
			const State& state = context.State<State>();
			if (!state.open)
				return;

			const Style& style = context.RootPresentationStyle<SdWindow>();
			const SdBoxStyle& presentation = context.RootStyleNode().presentationStyle;
			(void)style;
			const SdStyleNode& titlebarNode = context.Part(Parts::Titlebar);
			const SdStyleNode& titleNode = context.Part(Parts::Title);
			const SdStyleNode& closeButtonNode = context.Part(Parts::CloseButton);
			const SdStyleNode& contentNode = context.Part(Parts::Content);
			const SdBoxStyle& titlebarPresentation = titlebarNode.presentationStyle;
			const SdBoxStyle& titlePresentation = titleNode.presentationStyle;
			const SdBoxStyle& closeButtonPresentation = closeButtonNode.presentationStyle;
			const SdBoxStyle& contentPresentation = contentNode.presentationStyle;
			const SdRect paintRect = BasicWidgetDetail::StyleNodeRect(contentNode, BasicWidgetDetail::PaintRect(context));
			const SdRect titleRect = BasicWidgetDetail::StyleNodeRect(titlebarNode, paintRect);
			const SdRect titleTextRect = BasicWidgetDetail::StyleNodeRect(titleNode, titleRect);
			const SdRect closeRect = BasicWidgetDetail::StyleNodeRect(closeButtonNode, titleRect);
			const SdTextStyle textStyle = BasicWidgetDetail::BuildTextStyle({}, titlePresentation.fontSize, titlePresentation.lineHeight);
			const SdColor background = BasicWidgetDetail::ApplyOpacity(contentPresentation.backgroundColor, context.opacity * contentPresentation.opacity);
			const SdColor border = BasicWidgetDetail::ApplyOpacity(contentPresentation.border.left.color, context.opacity * contentPresentation.opacity);
			const SdColor textColor = BasicWidgetDetail::ApplyOpacity(titlePresentation.color, context.opacity * titlePresentation.opacity);
			const SdColor closeColor = BasicWidgetDetail::ApplyOpacity(closeButtonPresentation.color, context.opacity * closeButtonPresentation.opacity);
			const SdColor titleColor = BasicWidgetDetail::ApplyOpacity(titlebarPresentation.backgroundColor, context.opacity * titlebarPresentation.opacity);
			const float radius = SdResolveLength(contentPresentation.radius, paintRect.Width(), SdResolveLength(presentation.radius, paintRect.Width()));
			const float titlebarRadius = SdResolveLength(titlebarPresentation.radius, paintRect.Width(), radius);

			context.renderList.PushClipRect(context.clipRect);
			if (state.options.backgroundBlurRadius > 0.0f)
				context.renderList.AddBackdropBlur(paintRect, state.options.backgroundBlurRadius, radius);
			if (state.options.shadowRadius > 0.0f && state.options.shadowColor.a != 0)
			{
				const float shadowPadding = std::ceil(std::max(0.0f, state.options.shadowRadius)
					+ std::max(0.0f, state.options.shadowSpread)
					+ std::max(std::abs(state.options.shadowOffset.x), std::abs(state.options.shadowOffset.y)));
				const SdRect shadowClipRect = {
					context.clipRect.min.x - shadowPadding,
					context.clipRect.min.y - shadowPadding,
					context.clipRect.max.x + shadowPadding,
					context.clipRect.max.y + shadowPadding
				};
				context.renderList.PushClipRect(shadowClipRect);
				context.renderList.AddDropShadow(paintRect, state.options.shadowOffset, state.options.shadowColor, state.options.shadowRadius, state.options.shadowSpread, radius);
				context.renderList.PopClip();
			}
			context.renderList.AddRectFilled(paintRect, background, radius);
			context.renderList.AddRectFilled(titleRect, titleColor, titlebarRadius);
			context.renderList.AddRect(paintRect, border, 1.0f, radius);
			context.renderList.AddText(state.title, textStyle, titleTextRect.min, textColor);

			if (state.options.closable)
			{
				context.renderList.AddLine(
					{ closeRect.min.x + 5.0f, closeRect.min.y + 5.0f },
					{ closeRect.max.x - 5.0f, closeRect.max.y - 5.0f },
					closeColor,
					1.5f);
				context.renderList.AddLine(
					{ closeRect.max.x - 5.0f, closeRect.min.y + 5.0f },
					{ closeRect.min.x + 5.0f, closeRect.max.y - 5.0f },
					closeColor,
					1.5f);
			}
			context.renderList.PopClip();
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
			context.widgetState.rootLayer = SdRootLayer::Floating;
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
			const SdWidgetRootStyle rootStyle = context.instance.ResolveRootStyleForWidget(context.id, SdStyleInteractionState::Normal, SdRootLayer::Floating);
			const SdResolvedBoxStyle usedStyle = SdResolveBoxStyle(rootStyle, context.widgetState.targetRect.Size(), { 420.0f, 260.0f });
			if (!state.initialized)
			{
				const SdVec2 size = {
					state.options.size.x > 0.0f ? state.options.size.x : usedStyle.width,
					state.options.size.y > 0.0f ? state.options.size.y : usedStyle.height
				};
				state.rect = BasicWidgetDetail::MakeRect(state.options.position, size);
				state.initialized = true;
			}

			const SdVec2 mouse = context.input.GetMousePosition();
			const SdRect titleRect = TitleRect(state, style);
			const bool inCloseButton = state.options.closable && CloseRect(titleRect).Contains(mouse);
			const SdWidgetId pressedWidget = context.instance.GetInteractionSystem().GetState().pressedWidget;
			if (pressedWidget != 0 && context.instance.IsWidgetDescendantOf(pressedWidget, context.id))
			{
				const SdUInt32 activationOrder = context.instance.AllocateActivationOrder();
				context.widgetState.stackingOrder = activationOrder;
				context.widgetState.computedStackingOrder = activationOrder;
			}
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
		static constexpr SdTypeId TypeId = "Sodium.Widget.ImageViewer"_SdId;
		static constexpr SdTypeId TargetTypeId = SdWidgetTargetIds::ImageViewer;
		using Style = SdWidgetRootStyle;

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
			const SdRect paintRect = BasicWidgetDetail::PaintRect(context);
			const SdResolvedBoxStyle usedStyle = SdResolveBoxStyle(presentation, paintRect.Size(), { 160.0f, 120.0f });
			const SdColor background = BasicWidgetDetail::ApplyOpacity(presentation.backgroundColor, context.opacity * presentation.opacity);
			const SdColor tint = BasicWidgetDetail::ApplyOpacity(state.tint, context.opacity * presentation.opacity);
			const float radius = SdResolveLength(presentation.radius, paintRect.Width());

			context.renderList.PushClipRect(context.clipRect);
			context.renderList.AddRectFilled(paintRect, background, radius);
			const SdRect imageRect = BasicWidgetDetail::InsetRect(paintRect, {
				usedStyle.padding.left,
				usedStyle.padding.top,
				usedStyle.padding.right,
				usedStyle.padding.bottom
				});
			if (state.texture.IsValid())
				context.renderList.AddImage(state.texture, imageRect, state.uvRect, tint);
			context.renderList.AddRect(paintRect, presentation.border.left.color, 1.0f, radius);
			context.renderList.PopClip();
		}
	};

	struct SdScrollView final : SdWidgetTag
	{
		static constexpr SdTypeId TypeId = "Sodium.Widget.ScrollView"_SdId;
		static constexpr SdTypeId TargetTypeId = SdWidgetTargetIds::ScrollView;

		struct Parts final
		{
			static constexpr SdStylePart Scrollbar = SdWidgetPartIds::ScrollView::Scrollbar;
			static constexpr SdStylePart Thumb = SdWidgetPartIds::ScrollView::Thumb;
		};

		struct Style final
		{
			float scrollbarWidth = 5.0f;

			static Style Default(const SdStyleContext& context)
			{
				(void)context;
				Style style = {};
				style.scrollbarWidth = 5.0f;
				return style;
			}

			static void Describe(SdStyleContract<Style>& contract)
			{
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
			context.EnsurePart(Parts::Scrollbar);
			context.EnsurePart(Parts::Thumb);
			if (context.IsHovered())
			{
				state.scrollOffset = std::max(0.0f, state.scrollOffset - (context.input.GetMouseWheelDelta().y * 24.0f));
			}
			std::forward<TContent>(content)(context.ui);
		}

		void OnLayout(SdLayoutContext& context)
		{
			const Style& style = context.RootResolvedStyle<SdScrollView>();
			(void)style;
			const SdBoxStyle& rootStyle = context.RootStyleNode().resolvedStyle;
			const SdResolvedBoxStyle usedStyle = SdResolveBoxStyle(rootStyle, context.constraints.maxSize, { 240.0f, 160.0f });
			context.SetDesiredSize({ std::max(0.0f, usedStyle.width), std::max(0.0f, usedStyle.height) });
			context.widgetState.arrangeChildren = true;
			context.widgetState.clipChildren = true;
		}

		void OnArrange(SdArrangeContext& context)
		{
			const State& state = context.State<State>();
			const Style& style = context.RootResolvedStyle<SdScrollView>();
			const SdRect rootRect = BasicWidgetDetail::StyleNodeArrangeRect(context.RootStyleNode());
			if (style.scrollbarWidth <= 0.0f)
			{
				context.SetPartBorderBox(Parts::Scrollbar, {});
				context.SetPartBorderBox(Parts::Thumb, {});
				return;
			}

			const SdRect scrollbarRect = {
				rootRect.max.x - style.scrollbarWidth - 4.0f,
				rootRect.min.y + 4.0f,
				rootRect.max.x - 4.0f,
				rootRect.max.y - 4.0f
			};
			const float thumbHeight = std::max(18.0f, scrollbarRect.Height() * 0.35f);
			const float travel = std::max(0.0f, scrollbarRect.Height() - thumbHeight);
			const float thumbY = scrollbarRect.min.y + std::fmod(state.scrollOffset, std::max(1.0f, travel));
			const SdRect thumbRect = {
				scrollbarRect.min.x,
				thumbY,
				scrollbarRect.max.x,
				std::min(scrollbarRect.max.y, thumbY + thumbHeight)
			};
			context.SetPartBorderBox(Parts::Scrollbar, scrollbarRect);
			context.SetPartBorderBox(Parts::Thumb, thumbRect);
		}

		void OnPaint(SdPaintContext& context)
		{
			const Style& style = context.RootPresentationStyle<SdScrollView>();
			const SdBoxStyle& presentation = context.RootStyleNode().presentationStyle;
			const SdStyleNode& scrollbarNode = context.Part(Parts::Scrollbar);
			const SdStyleNode& thumbNode = context.Part(Parts::Thumb);
			const SdBoxStyle& scrollbarPresentation = scrollbarNode.presentationStyle;
			const SdBoxStyle& thumbPresentation = thumbNode.presentationStyle;
			const SdRect paintRect = BasicWidgetDetail::PaintRect(context);
			const SdRect scrollbarRect = BasicWidgetDetail::StyleNodeRect(scrollbarNode, paintRect);
			const SdRect thumbRect = BasicWidgetDetail::StyleNodeRect(thumbNode, scrollbarRect);
			const SdColor rootBackground = BasicWidgetDetail::ApplyOpacity(presentation.backgroundColor, context.opacity * presentation.opacity);
			const SdColor background = BasicWidgetDetail::ApplyOpacity(scrollbarPresentation.backgroundColor, context.opacity * scrollbarPresentation.opacity);
			const SdColor border = BasicWidgetDetail::ApplyOpacity(scrollbarPresentation.border.left.color, context.opacity * scrollbarPresentation.opacity);
			const SdColor thumbColor = BasicWidgetDetail::ApplyOpacity(thumbPresentation.backgroundColor, context.opacity * thumbPresentation.opacity);
			const float rootRadius = SdResolveLength(presentation.radius, paintRect.Width());
			const float radius = SdResolveLength(scrollbarPresentation.radius, scrollbarRect.Width(), rootRadius);

			context.renderList.PushClipRect(context.clipRect);
			context.renderList.AddRectFilled(paintRect, rootBackground, rootRadius);
			context.renderList.AddRectFilled(scrollbarRect, background, radius);
			context.renderList.AddRect(scrollbarRect, border, 1.0f, radius);

			if (style.scrollbarWidth > 0.0f)
			{
				context.renderList.AddRectFilled(thumbRect, thumbColor, style.scrollbarWidth * 0.5f);
			}
			context.renderList.PopClip();
		}
	};

	struct SdPopup final : SdWidgetTag
	{
		static constexpr SdTypeId TypeId = "Sodium.Widget.Popup"_SdId;
		static constexpr SdTypeId TargetTypeId = SdWidgetTargetIds::Popup;
		using Style = SdWidgetRootStyle;

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
			{
				context.ui.BeginPortal(SdPortalRoot::Popup, context.id, context.id);
				std::forward<TContent>(content)(context.ui);
				context.ui.EndPortal();
			}
		}

		void OnLayout(SdLayoutContext& context)
		{
			const State& state = context.State<State>();
			const SdBoxStyle& rootStyle = context.RootStyleNode().resolvedStyle;
			const SdResolvedBoxStyle usedStyle = SdResolveBoxStyle(rootStyle, context.constraints.maxSize, { 220.0f, 140.0f });
			context.widgetState.manualLayout = true;
			context.widgetState.manualRect = state.open
				? BasicWidgetDetail::MakeRect(state.position, { usedStyle.width, usedStyle.height })
				: SdRect{};
			context.widgetState.arrangeChildren = state.open;
			context.widgetState.clipChildren = true;
			context.SetDesiredSize(state.open ? SdVec2{ usedStyle.width, usedStyle.height } : SdVec2{});
		}

		void OnPaint(SdPaintContext& context)
		{
			const State& state = context.State<State>();
			if (!state.open)
				return;
			const SdBoxStyle& presentation = context.RootStyleNode().presentationStyle;
			const SdRect paintRect = BasicWidgetDetail::PaintRect(context);
			const float radius = SdResolveLength(presentation.radius, paintRect.Width());
			context.renderList.PushClipRect(context.clipRect);
			context.renderList.AddRectFilled(
				paintRect,
				BasicWidgetDetail::ApplyOpacity(presentation.backgroundColor, context.opacity * presentation.opacity),
				radius);
			context.renderList.AddRect(paintRect, presentation.border.left.color, 1.0f, radius);
			context.renderList.PopClip();
		}

	protected:
		static void Configure(SdUpdateContext& context, bool open)
		{
			context.widgetState.targetTypeId = TargetTypeId;
			context.widgetState.rootLayer = SdRootLayer::Popup;
			context.widgetState.rootLayer = SdRootLayer::Popup;
			context.widgetState.portalRoot = SdPortalRoot::Popup;
			context.widgetState.portalOwnerWidgetId = context.id;
			context.widgetState.portalAnchorWidgetId = context.id;
			context.widgetState.escapesParentClip = true;
			context.widgetState.createsStackingContext = open;
			context.widgetState.inputEnabled = open;
			context.widgetState.layoutWeight = open ? 1.0f : 0.0f;
		}
	};

	struct SdContextMenu final : SdWidgetTag
	{
		static constexpr SdTypeId TypeId = "Sodium.Widget.ContextMenu"_SdId;
		static constexpr SdTypeId TargetTypeId = SdWidgetTargetIds::ContextMenu;
		using Style = SdWidgetRootStyle;

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
			context.widgetState.rootLayer = SdRootLayer::Popup;
			context.widgetState.rootLayer = SdRootLayer::Popup;
			context.widgetState.portalRoot = SdPortalRoot::Popup;
			context.widgetState.portalOwnerWidgetId = context.id;
			context.widgetState.portalAnchorWidgetId = context.id;
			context.widgetState.escapesParentClip = true;
			context.widgetState.createsStackingContext = open;
			context.widgetState.inputEnabled = open;
			context.widgetState.layoutWeight = open ? 1.0f : 0.0f;
			if (open)
			{
				context.ui.BeginPortal(SdPortalRoot::Popup, context.id, context.id);
				std::forward<TContent>(content)(context.ui);
				context.ui.EndPortal();
			}
		}

		void OnLayout(SdLayoutContext& context)
		{
			const State& state = context.State<State>();
			const SdBoxStyle& rootStyle = context.RootStyleNode().resolvedStyle;
			const SdResolvedBoxStyle usedStyle = SdResolveBoxStyle(rootStyle, context.constraints.maxSize, { 220.0f, 140.0f });
			context.widgetState.manualLayout = true;
			context.widgetState.manualRect = state.open
				? BasicWidgetDetail::MakeRect(state.position, { usedStyle.width, usedStyle.height })
				: SdRect{};
			context.widgetState.arrangeChildren = state.open;
			context.widgetState.clipChildren = true;
			context.SetDesiredSize(state.open ? SdVec2{ usedStyle.width, usedStyle.height } : SdVec2{});
		}

		void OnPaint(SdPaintContext& context)
		{
			const State& state = context.State<State>();
			if (!state.open)
				return;
			const SdBoxStyle& presentation = context.RootStyleNode().presentationStyle;
			const SdRect paintRect = BasicWidgetDetail::PaintRect(context);
			const float radius = SdResolveLength(presentation.radius, paintRect.Width());

			context.renderList.PushClipRect(context.clipRect);
			context.renderList.AddRectFilled(
				paintRect,
				BasicWidgetDetail::ApplyOpacity(presentation.backgroundColor, context.opacity * presentation.opacity),
				radius);
			context.renderList.AddRect(paintRect, presentation.border.left.color, 1.0f, radius);
			context.renderList.PopClip();
		}
	};

	struct SdTooltip final : SdWidgetTag
	{
		static constexpr SdTypeId TypeId = "Sodium.Widget.Tooltip"_SdId;
		static constexpr SdTypeId TargetTypeId = SdWidgetTargetIds::Tooltip;
		using Style = SdWidgetRootStyle;

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
			context.widgetState.rootLayer = SdRootLayer::Tooltip;
			context.widgetState.rootLayer = SdRootLayer::Tooltip;
			context.widgetState.portalRoot = SdPortalRoot::Tooltip;
			context.widgetState.portalOwnerWidgetId = context.id;
			context.widgetState.portalAnchorWidgetId = context.id;
			context.widgetState.escapesParentClip = true;
			context.widgetState.createsStackingContext = visible;
			context.widgetState.inputEnabled = false;
			context.widgetState.layoutWeight = visible ? 1.0f : 0.0f;
		}

		void OnLayout(SdLayoutContext& context)
		{
			const State& state = context.State<State>();
			const SdBoxStyle& rootStyle = context.RootStyleNode().resolvedStyle;
			const SdResolvedBoxStyle usedStyle = SdResolveBoxStyle(rootStyle, context.constraints.maxSize, {});
			const SdTextStyle textStyle = BasicWidgetDetail::BuildTextStyle({}, rootStyle.fontSize, rootStyle.lineHeight);
			const SdVec2 textSize = BasicWidgetDetail::TextVisualSize(
				BasicWidgetDetail::MeasureTextLayout(context.instance, state.text, textStyle));
			const SdVec2 size = state.visible
				? SdVec2{ textSize.x + usedStyle.padding.left + usedStyle.padding.right, textSize.y + usedStyle.padding.top + usedStyle.padding.bottom }
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
			const SdBoxStyle& presentation = context.RootStyleNode().presentationStyle;
			const SdRect paintRect = BasicWidgetDetail::PaintRect(context);
			const SdResolvedBoxStyle usedStyle = SdResolveBoxStyle(presentation, paintRect.Size(), {});
			const SdTextStyle textStyle = BasicWidgetDetail::BuildTextStyle({}, presentation.fontSize, presentation.lineHeight);
			const BasicWidgetDetail::SdMeasuredTextLayout textLayout = BasicWidgetDetail::MeasureTextLayout(context.instance, state.text, textStyle);
			const SdRect contentRect = SdInsetRect(paintRect, usedStyle.padding);
			const SdVec2 textPosition = BasicWidgetDetail::LeftTopTextLayoutOrigin(contentRect, textLayout);
			const SdColor background = BasicWidgetDetail::ApplyOpacity(presentation.backgroundColor, context.opacity * presentation.opacity);
			const SdColor border = BasicWidgetDetail::ApplyOpacity(presentation.border.left.color, context.opacity * presentation.opacity);
			const SdColor color = BasicWidgetDetail::ApplyOpacity(presentation.color, context.opacity * presentation.opacity);
			const float radius = SdResolveLength(presentation.radius, paintRect.Width());

			context.renderList.PushClipRect(context.clipRect);
			context.renderList.AddRectFilled(paintRect, background, radius);
			context.renderList.AddRect(paintRect, border, 1.0f, radius);
			context.renderList.AddText(
				state.text,
				textStyle,
				textPosition,
				color);
			context.renderList.PopClip();
		}
	};
}
