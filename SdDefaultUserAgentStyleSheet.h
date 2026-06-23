#pragma once

#include <algorithm>

namespace Sodium
{
	namespace Detail
	{
		inline void SdAddDefaultUserAgentRootBackgroundRule(
			SdStyleSheet& sheet,
			SdStyleId targetTag,
			SdStyleInteractionState interactionState,
			SdRootLayer rootLayer,
			const char* backgroundVariable,
			bool matchLayer = false)
		{
			auto rule = sheet.RuleForTarget<SdWidgetRootStyle>(targetTag);
			rule.Cascade(SdCascadeLayer::UserAgent)
				.Pseudo(interactionState)
				.Set(&SdBoxStyle::backgroundColor, ThemeColor(backgroundVariable));
			if (matchLayer)
				rule.Layer(rootLayer);
		}
	}

	inline SdStyleSheet SdDefaultUserAgentStyleSheet(const SdTheme& theme)
	{
		SdStyleSheet sheet = {};
		auto global = sheet.RuleForTarget<SdWidgetRootStyle>(SdWidgetTargetIds::Global);
		global.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::color, ThemeColor("text"))
			.Set(&SdBoxStyle::backgroundColor, ThemeColor("background"))
			.Set(&SdBoxStyle::border, ThemeColor("border"))
			.Set(&SdBoxStyle::radius, ThemeMetric("radius.small"))
			.Set(&SdBoxStyle::opacity, 1.0f)
			.Set(&SdBoxStyle::fontSize, ThemeMetric("font.button"))
			.Set(&SdBoxStyle::lineHeight, 0.0f);

		const float mediumSpacing = theme.GetMetricVariable(SdThemeVariableLiteral("spacing.medium"));
		sheet.RuleForTarget<SdWidgetRootStyle>(SdWidgetTargetIds::Panel)
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::width, SdLength::Pixels(240.0f))
			.Set(&SdBoxStyle::height, SdLength::Pixels(120.0f))
			.Set(&SdBoxStyle::padding, SdStyleValue::FromSpacing({ mediumSpacing, mediumSpacing, mediumSpacing, mediumSpacing }))
			.Set(&SdBoxStyle::gap, SdLength::Pixels(theme.GetMetricVariable(SdThemeVariableLiteral("spacing.small"))));
		sheet.RuleForTarget<SdWidgetRootStyle>(SdWidgetTargetIds::Window)
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::width, SdLength::Pixels(420.0f))
			.Set(&SdBoxStyle::height, SdLength::Pixels(260.0f))
			.Set(&SdBoxStyle::padding, SdStyleValue::FromSpacing({ mediumSpacing, 40.0f, mediumSpacing, mediumSpacing }))
			.Set(&SdBoxStyle::fontSize, ThemeMetric("font.button"))
			.Set(&SdBoxStyle::lineHeight, 0.0f)
			.Set(&SdBoxStyle::radius, ThemeMetric("radius.small"))
			.Set(&SdBoxStyle::gap, SdLength::Pixels(theme.GetMetricVariable(SdThemeVariableLiteral("spacing.small"))))
			.Set(&SdBoxStyle::opacity, 1.0f);
		sheet.PartForTarget(SdWidgetTargetIds::Window, SdStylePart::Make("Sodium.Window.Part.Content"))
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::backgroundColor, ThemeColor("window.bg"))
			.Set(&SdBoxStyle::border, ThemeColor("border"))
			.Set(&SdBoxStyle::radius, ThemeMetric("radius.small"));
		sheet.PartForTarget(SdWidgetTargetIds::Window, SdStylePart::Make("Sodium.Window.Part.Titlebar"))
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::backgroundColor, ThemeColor("button.bg"))
			.Set(&SdBoxStyle::radius, ThemeMetric("radius.small"));

		const float smallSpacing = theme.GetMetricVariable(SdThemeVariableLiteral("spacing.small"));
		sheet.RuleForTarget<SdWidgetRootStyle>(SdWidgetTargetIds::Button)
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::minWidth, SdLength::Pixels(82.0f))
			.Set(&SdBoxStyle::minHeight, SdLength::Pixels(30.0f))
			.Set(&SdBoxStyle::padding, SdStyleValue::FromSpacing({ mediumSpacing, smallSpacing, mediumSpacing, smallSpacing }))
			.Set(&SdBoxStyle::fontSize, ThemeMetric("font.button"))
			.Set(&SdBoxStyle::lineHeight, 0.0f);
		sheet.PartForTarget(SdWidgetTargetIds::Button, SdStylePart::Make("Sodium.Button.Part.Content"))
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::backgroundColor, ThemeColor("button.bg"))
			.Set(&SdBoxStyle::border, ThemeColor("border"))
			.Set(&SdBoxStyle::radius, ThemeMetric("radius.small"));
		sheet.PartForTarget(SdWidgetTargetIds::Button, SdStylePart::Make("Sodium.Button.Part.Content"))
			.Cascade(SdCascadeLayer::UserAgent)
			.Pseudo(SdStyleInteractionState::Hovered)
			.Set(&SdBoxStyle::backgroundColor, ThemeColor("button.bg.hover"));
		sheet.PartForTarget(SdWidgetTargetIds::Button, SdStylePart::Make("Sodium.Button.Part.Content"))
			.Cascade(SdCascadeLayer::UserAgent)
			.Pseudo(SdStyleInteractionState::Pressed)
			.Set(&SdBoxStyle::backgroundColor, ThemeColor("button.bg.pressed"));

		sheet.RuleForTarget<SdWidgetRootStyle>(SdWidgetTargetIds::CheckBox)
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::minHeight, SdLength::Pixels(28.0f))
			.Set(&SdBoxStyle::padding, SdStyleValue::FromSpacing({ smallSpacing, smallSpacing, smallSpacing, smallSpacing }))
			.Set(&SdBoxStyle::fontSize, ThemeMetric("font.button"))
			.Set(&SdBoxStyle::lineHeight, 0.0f)
			.Set(&SdBoxStyle::gap, SdLength::Pixels(smallSpacing))
			.Set(&SdBoxStyle::radius, SdLength::Pixels(std::max(2.0f, theme.GetMetricVariable(SdThemeVariableLiteral("radius.small")) - 1.0f)));
		sheet.PartForTarget(SdWidgetTargetIds::CheckBox, SdStylePart::Make("Sodium.CheckBox.Part.Box"))
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::backgroundColor, ThemeColor("panel.bg"))
			.Set(&SdBoxStyle::border, ThemeColor("border"))
			.Set(&SdBoxStyle::radius, SdLength::Pixels(std::max(2.0f, theme.GetMetricVariable(SdThemeVariableLiteral("radius.small")) - 1.0f)));
		sheet.PartForTarget(SdWidgetTargetIds::CheckBox, SdStylePart::Make("Sodium.CheckBox.Part.Box"))
			.Cascade(SdCascadeLayer::UserAgent)
			.Pseudo(SdStyleInteractionState::Hovered)
			.Set(&SdBoxStyle::backgroundColor, ThemeColor("button.bg.hover"));
		sheet.PartForTarget(SdWidgetTargetIds::CheckBox, SdStylePart::Make("Sodium.CheckBox.Part.Indicator"))
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::backgroundColor, ThemeColor("accent"));

		sheet.RuleForTarget<SdWidgetRootStyle>(SdWidgetTargetIds::Slider)
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::width, SdLength::Pixels(180.0f))
			.Set(&SdBoxStyle::height, SdLength::Pixels(30.0f))
			.Set(&SdBoxStyle::padding, SdStyleValue::FromSpacing({ smallSpacing, smallSpacing, smallSpacing, smallSpacing }))
			.Set(&SdBoxStyle::fontSize, ThemeMetric("font.button"))
			.Set(&SdBoxStyle::lineHeight, 0.0f)
			.Set(&SdBoxStyle::gap, SdLength::Pixels(smallSpacing));
		sheet.PartForTarget(SdWidgetTargetIds::Slider, SdStylePart::Make("Sodium.Slider.Part.Track"))
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::backgroundColor, ThemeColor("panel.bg"))
			.Set(&SdBoxStyle::border, ThemeColor("border"))
			.Set(&SdBoxStyle::radius, ThemeMetric("radius.small"));
		sheet.PartForTarget(SdWidgetTargetIds::Slider, SdStylePart::Make("Sodium.Slider.Part.Track"))
			.Cascade(SdCascadeLayer::UserAgent)
			.Pseudo(SdStyleInteractionState::Hovered)
			.Set(&SdBoxStyle::backgroundColor, ThemeColor("button.bg.hover"));
		sheet.PartForTarget(SdWidgetTargetIds::Slider, SdStylePart::Make("Sodium.Slider.Part.Track"))
			.Cascade(SdCascadeLayer::UserAgent)
			.Pseudo(SdStyleInteractionState::Pressed)
			.Set(&SdBoxStyle::backgroundColor, ThemeColor("button.bg.pressed"));
		sheet.PartForTarget(SdWidgetTargetIds::Slider, SdStylePart::Make("Sodium.Slider.Part.Fill"))
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::backgroundColor, ThemeColor("accent"));
		sheet.PartForTarget(SdWidgetTargetIds::Slider, SdStylePart::Make("Sodium.Slider.Part.Thumb"))
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::backgroundColor, ThemeColor("accent"))
			.Set(&SdBoxStyle::border, ThemeColor("border"));

		sheet.RuleForTarget<SdWidgetRootStyle>(SdWidgetTargetIds::TextInput)
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::width, SdLength::Pixels(220.0f))
			.Set(&SdBoxStyle::minHeight, SdLength::Pixels(32.0f))
			.Set(&SdBoxStyle::padding, SdStyleValue::FromSpacing({ mediumSpacing, smallSpacing, mediumSpacing, smallSpacing }))
			.Set(&SdBoxStyle::fontSize, ThemeMetric("font.button"))
			.Set(&SdBoxStyle::lineHeight, 0.0f);
		sheet.PartForTarget(SdWidgetTargetIds::TextInput, SdStylePart::Make("Sodium.TextInput.Part.Field"))
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::backgroundColor, ThemeColor("panel.bg"))
			.Set(&SdBoxStyle::border, ThemeColor("border"))
			.Set(&SdBoxStyle::radius, ThemeMetric("radius.small"));
		sheet.PartForTarget(SdWidgetTargetIds::TextInput, SdStylePart::Make("Sodium.TextInput.Part.Field"))
			.Cascade(SdCascadeLayer::UserAgent)
			.Pseudo(SdStyleInteractionState::Focused)
			.Set(&SdBoxStyle::backgroundColor, ThemeColor("button.bg"));
		sheet.PartForTarget(SdWidgetTargetIds::TextInput, SdStylePart::Make("Sodium.TextInput.Part.Placeholder"))
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::opacity, 0.52f);

		sheet.RuleForTarget<SdWidgetRootStyle>(SdWidgetTargetIds::ImageViewer)
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::width, SdLength::Pixels(160.0f))
			.Set(&SdBoxStyle::height, SdLength::Pixels(120.0f));
		sheet.RuleForTarget<SdWidgetRootStyle>(SdWidgetTargetIds::ScrollView)
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::width, SdLength::Pixels(240.0f))
			.Set(&SdBoxStyle::height, SdLength::Pixels(160.0f))
			.Set(&SdBoxStyle::padding, SdStyleValue::FromSpacing({ smallSpacing, smallSpacing, smallSpacing, smallSpacing }))
			.Set(&SdBoxStyle::gap, SdLength::Pixels(smallSpacing));
		sheet.PartForTarget(SdWidgetTargetIds::ScrollView, SdStylePart::Make("Sodium.ScrollView.Part.Scrollbar"))
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::backgroundColor, ThemeColor("panel.bg"))
			.Set(&SdBoxStyle::border, ThemeColor("border"))
			.Set(&SdBoxStyle::radius, ThemeMetric("radius.small"));
		sheet.PartForTarget(SdWidgetTargetIds::ScrollView, SdStylePart::Make("Sodium.ScrollView.Part.Thumb"))
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::backgroundColor, ThemeColor("accent"));

		sheet.RuleForTarget<SdWidgetRootStyle>(SdWidgetTargetIds::Popup)
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::width, SdLength::Pixels(220.0f))
			.Set(&SdBoxStyle::height, SdLength::Pixels(140.0f))
			.Set(&SdBoxStyle::padding, SdStyleValue::FromSpacing({ smallSpacing, smallSpacing, smallSpacing, smallSpacing }))
			.Set(&SdBoxStyle::gap, SdLength::Pixels(smallSpacing));
		sheet.RuleForTarget<SdWidgetRootStyle>(SdWidgetTargetIds::ContextMenu)
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::width, SdLength::Pixels(220.0f))
			.Set(&SdBoxStyle::height, SdLength::Pixels(140.0f))
			.Set(&SdBoxStyle::padding, SdStyleValue::FromSpacing({ smallSpacing, smallSpacing, smallSpacing, smallSpacing }))
			.Set(&SdBoxStyle::gap, SdLength::Pixels(smallSpacing));
		sheet.RuleForTarget<SdWidgetRootStyle>(SdWidgetTargetIds::Tooltip)
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::padding, SdStyleValue::FromSpacing({ smallSpacing, smallSpacing, smallSpacing, smallSpacing }))
			.Set(&SdBoxStyle::fontSize, ThemeMetric("font.button"))
			.Set(&SdBoxStyle::lineHeight, 0.0f);

		Detail::SdAddDefaultUserAgentRootBackgroundRule(sheet, SdWidgetTargetIds::Default, SdStyleInteractionState::Normal, SdRootLayer::Content, "background");
		Detail::SdAddDefaultUserAgentRootBackgroundRule(sheet, SdWidgetTargetIds::Panel, SdStyleInteractionState::Normal, SdRootLayer::Content, "panel.bg");
		Detail::SdAddDefaultUserAgentRootBackgroundRule(sheet, SdWidgetTargetIds::ImageViewer, SdStyleInteractionState::Normal, SdRootLayer::Content, "panel.bg");
		Detail::SdAddDefaultUserAgentRootBackgroundRule(sheet, SdWidgetTargetIds::Popup, SdStyleInteractionState::Normal, SdRootLayer::Popup, "window.bg", true);
		Detail::SdAddDefaultUserAgentRootBackgroundRule(sheet, SdWidgetTargetIds::ContextMenu, SdStyleInteractionState::Normal, SdRootLayer::Popup, "window.bg", true);
		Detail::SdAddDefaultUserAgentRootBackgroundRule(sheet, SdWidgetTargetIds::Tooltip, SdStyleInteractionState::Normal, SdRootLayer::Tooltip, "panel.bg", true);

		return sheet;
	}
}
