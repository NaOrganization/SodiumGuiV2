#pragma once

#include <algorithm>

namespace Sodium
{
	namespace Detail
	{
		inline void SdAddDefaultUserAgentRootBackgroundRule(
			SdStyleSheet& sheet,
			SdTypeId targetTag,
			SdStyleInteractionState interactionState,
			SdRootLayer rootLayer,
			SdDesignTokenId backgroundVariable,
			bool matchLayer = false)
		{
			auto rule = sheet.RuleForTarget<SdWidgetRootStyle>(targetTag);
			rule.Cascade(SdCascadeLayer::UserAgent)
				.Pseudo(interactionState)
				.Set(&SdBoxStyle::backgroundColor, DesignColor(backgroundVariable));
			if (matchLayer)
				rule.Layer(rootLayer);
		}
	}

	inline SdStyleSheet SdDefaultUserAgentStyleSheet(const SdDesignTokenSet& tokenSet)
	{
		SdStyleSheet sheet = {};
		auto global = sheet.RuleForTarget<SdWidgetRootStyle>(SdWidgetTargetIds::Global);
		global.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::color, DesignColor(SdDesignTokenIds::Text))
			.Set(&SdBoxStyle::backgroundColor, DesignColor(SdDesignTokenIds::Background))
			.Set(&SdBoxStyle::border, DesignColor(SdDesignTokenIds::Border))
			.Set(&SdBoxStyle::radius, DesignMetric(SdDesignTokenIds::RadiusSmall))
			.Set(&SdBoxStyle::opacity, 1.0f)
			.Set(&SdBoxStyle::fontSize, DesignMetric(SdDesignTokenIds::FontButton))
			.Set(&SdBoxStyle::lineHeight, 0.0f);

		const float mediumSpacing = tokenSet.GetMetric(SdDesignTokenIds::SpacingMedium);
		sheet.RuleForTarget<SdWidgetRootStyle>(SdWidgetTargetIds::Panel)
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::width, SdLength::Pixels(240.0f))
			.Set(&SdBoxStyle::height, SdLength::Pixels(120.0f))
			.Set(&SdBoxStyle::padding, SdStyleValue::FromSpacing({ mediumSpacing, mediumSpacing, mediumSpacing, mediumSpacing }))
			.Set(&SdBoxStyle::gap, SdLength::Pixels(tokenSet.GetMetric(SdDesignTokenIds::SpacingSmall)));
		sheet.RuleForTarget<SdWidgetRootStyle>(SdWidgetTargetIds::Window)
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::width, SdLength::Pixels(420.0f))
			.Set(&SdBoxStyle::height, SdLength::Pixels(260.0f))
			.Set(&SdBoxStyle::padding, SdStyleValue::FromSpacing({ mediumSpacing, 40.0f, mediumSpacing, mediumSpacing }))
			.Set(&SdBoxStyle::fontSize, DesignMetric(SdDesignTokenIds::FontButton))
			.Set(&SdBoxStyle::lineHeight, 0.0f)
			.Set(&SdBoxStyle::radius, DesignMetric(SdDesignTokenIds::RadiusSmall))
			.Set(&SdBoxStyle::gap, SdLength::Pixels(tokenSet.GetMetric(SdDesignTokenIds::SpacingSmall)))
			.Set(&SdBoxStyle::opacity, 1.0f);
		sheet.PartForTarget(SdWidgetTargetIds::Window, SdWidgetPartIds::Window::Content)
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::backgroundColor, DesignColor(SdDesignTokenIds::WindowBg))
			.Set(&SdBoxStyle::border, DesignColor(SdDesignTokenIds::Border))
			.Set(&SdBoxStyle::radius, DesignMetric(SdDesignTokenIds::RadiusSmall));
		sheet.PartForTarget(SdWidgetTargetIds::Window, SdWidgetPartIds::Window::Titlebar)
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::backgroundColor, DesignColor(SdDesignTokenIds::ButtonBg))
			.Set(&SdBoxStyle::radius, DesignMetric(SdDesignTokenIds::RadiusSmall));

		const float smallSpacing = tokenSet.GetMetric(SdDesignTokenIds::SpacingSmall);
		sheet.RuleForTarget<SdWidgetRootStyle>(SdWidgetTargetIds::Button)
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::minWidth, SdLength::Pixels(82.0f))
			.Set(&SdBoxStyle::minHeight, SdLength::Pixels(30.0f))
			.Set(&SdBoxStyle::padding, SdStyleValue::FromSpacing({ mediumSpacing, smallSpacing, mediumSpacing, smallSpacing }))
			.Set(&SdBoxStyle::fontSize, DesignMetric(SdDesignTokenIds::FontButton))
			.Set(&SdBoxStyle::lineHeight, 0.0f);
		sheet.PartForTarget(SdWidgetTargetIds::Button, SdWidgetPartIds::Button::Content)
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::backgroundColor, DesignColor(SdDesignTokenIds::ButtonBg))
			.Set(&SdBoxStyle::border, DesignColor(SdDesignTokenIds::Border))
			.Set(&SdBoxStyle::radius, DesignMetric(SdDesignTokenIds::RadiusSmall));
		sheet.PartForTarget(SdWidgetTargetIds::Button, SdWidgetPartIds::Button::Content)
			.Cascade(SdCascadeLayer::UserAgent)
			.Pseudo(SdStyleInteractionState::Hovered)
			.Set(&SdBoxStyle::backgroundColor, DesignColor(SdDesignTokenIds::ButtonBgHover));
		sheet.PartForTarget(SdWidgetTargetIds::Button, SdWidgetPartIds::Button::Content)
			.Cascade(SdCascadeLayer::UserAgent)
			.Pseudo(SdStyleInteractionState::Pressed)
			.Set(&SdBoxStyle::backgroundColor, DesignColor(SdDesignTokenIds::ButtonBgPressed));

		sheet.RuleForTarget<SdWidgetRootStyle>(SdWidgetTargetIds::CheckBox)
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::minHeight, SdLength::Pixels(28.0f))
			.Set(&SdBoxStyle::padding, SdStyleValue::FromSpacing({ smallSpacing, smallSpacing, smallSpacing, smallSpacing }))
			.Set(&SdBoxStyle::fontSize, DesignMetric(SdDesignTokenIds::FontButton))
			.Set(&SdBoxStyle::lineHeight, 0.0f)
			.Set(&SdBoxStyle::gap, SdLength::Pixels(smallSpacing))
			.Set(&SdBoxStyle::radius, SdLength::Pixels(std::max(2.0f, tokenSet.GetMetric(SdDesignTokenIds::RadiusSmall) - 1.0f)));
		sheet.PartForTarget(SdWidgetTargetIds::CheckBox, SdWidgetPartIds::CheckBox::Box)
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::backgroundColor, DesignColor(SdDesignTokenIds::PanelBg))
			.Set(&SdBoxStyle::border, DesignColor(SdDesignTokenIds::Border))
			.Set(&SdBoxStyle::radius, SdLength::Pixels(std::max(2.0f, tokenSet.GetMetric(SdDesignTokenIds::RadiusSmall) - 1.0f)));
		sheet.PartForTarget(SdWidgetTargetIds::CheckBox, SdWidgetPartIds::CheckBox::Box)
			.Cascade(SdCascadeLayer::UserAgent)
			.Pseudo(SdStyleInteractionState::Hovered)
			.Set(&SdBoxStyle::backgroundColor, DesignColor(SdDesignTokenIds::ButtonBgHover));
		sheet.PartForTarget(SdWidgetTargetIds::CheckBox, SdWidgetPartIds::CheckBox::Indicator)
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::backgroundColor, DesignColor(SdDesignTokenIds::Accent));

		sheet.RuleForTarget<SdWidgetRootStyle>(SdWidgetTargetIds::Slider)
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::width, SdLength::Pixels(180.0f))
			.Set(&SdBoxStyle::height, SdLength::Pixels(30.0f))
			.Set(&SdBoxStyle::padding, SdStyleValue::FromSpacing({ smallSpacing, smallSpacing, smallSpacing, smallSpacing }))
			.Set(&SdBoxStyle::fontSize, DesignMetric(SdDesignTokenIds::FontButton))
			.Set(&SdBoxStyle::lineHeight, 0.0f)
			.Set(&SdBoxStyle::gap, SdLength::Pixels(smallSpacing));
		sheet.PartForTarget(SdWidgetTargetIds::Slider, SdWidgetPartIds::Slider::Track)
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::backgroundColor, DesignColor(SdDesignTokenIds::PanelBg))
			.Set(&SdBoxStyle::border, DesignColor(SdDesignTokenIds::Border))
			.Set(&SdBoxStyle::radius, DesignMetric(SdDesignTokenIds::RadiusSmall));
		sheet.PartForTarget(SdWidgetTargetIds::Slider, SdWidgetPartIds::Slider::Track)
			.Cascade(SdCascadeLayer::UserAgent)
			.Pseudo(SdStyleInteractionState::Hovered)
			.Set(&SdBoxStyle::backgroundColor, DesignColor(SdDesignTokenIds::ButtonBgHover));
		sheet.PartForTarget(SdWidgetTargetIds::Slider, SdWidgetPartIds::Slider::Track)
			.Cascade(SdCascadeLayer::UserAgent)
			.Pseudo(SdStyleInteractionState::Pressed)
			.Set(&SdBoxStyle::backgroundColor, DesignColor(SdDesignTokenIds::ButtonBgPressed));
		sheet.PartForTarget(SdWidgetTargetIds::Slider, SdWidgetPartIds::Slider::Fill)
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::backgroundColor, DesignColor(SdDesignTokenIds::Accent));
		sheet.PartForTarget(SdWidgetTargetIds::Slider, SdWidgetPartIds::Slider::Thumb)
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::backgroundColor, DesignColor(SdDesignTokenIds::Accent))
			.Set(&SdBoxStyle::border, DesignColor(SdDesignTokenIds::Border));

		sheet.RuleForTarget<SdWidgetRootStyle>(SdWidgetTargetIds::TextInput)
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::width, SdLength::Pixels(220.0f))
			.Set(&SdBoxStyle::minHeight, SdLength::Pixels(32.0f))
			.Set(&SdBoxStyle::padding, SdStyleValue::FromSpacing({ mediumSpacing, smallSpacing, mediumSpacing, smallSpacing }))
			.Set(&SdBoxStyle::fontSize, DesignMetric(SdDesignTokenIds::FontButton))
			.Set(&SdBoxStyle::lineHeight, 0.0f);
		sheet.PartForTarget(SdWidgetTargetIds::TextInput, SdWidgetPartIds::TextInput::Field)
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::backgroundColor, DesignColor(SdDesignTokenIds::PanelBg))
			.Set(&SdBoxStyle::border, DesignColor(SdDesignTokenIds::Border))
			.Set(&SdBoxStyle::radius, DesignMetric(SdDesignTokenIds::RadiusSmall));
		sheet.PartForTarget(SdWidgetTargetIds::TextInput, SdWidgetPartIds::TextInput::Field)
			.Cascade(SdCascadeLayer::UserAgent)
			.Pseudo(SdStyleInteractionState::Focused)
			.Set(&SdBoxStyle::backgroundColor, DesignColor(SdDesignTokenIds::ButtonBg));
		sheet.PartForTarget(SdWidgetTargetIds::TextInput, SdWidgetPartIds::TextInput::Placeholder)
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
		sheet.PartForTarget(SdWidgetTargetIds::ScrollView, SdWidgetPartIds::ScrollView::Scrollbar)
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::backgroundColor, DesignColor(SdDesignTokenIds::PanelBg))
			.Set(&SdBoxStyle::border, DesignColor(SdDesignTokenIds::Border))
			.Set(&SdBoxStyle::radius, DesignMetric(SdDesignTokenIds::RadiusSmall));
		sheet.PartForTarget(SdWidgetTargetIds::ScrollView, SdWidgetPartIds::ScrollView::Thumb)
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::backgroundColor, DesignColor(SdDesignTokenIds::Accent));

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
			.Set(&SdBoxStyle::fontSize, DesignMetric(SdDesignTokenIds::FontButton))
			.Set(&SdBoxStyle::lineHeight, 0.0f);

		Detail::SdAddDefaultUserAgentRootBackgroundRule(sheet, SdWidgetTargetIds::Default, SdStyleInteractionState::Normal, SdRootLayer::Content, SdDesignTokenIds::Background);
		Detail::SdAddDefaultUserAgentRootBackgroundRule(sheet, SdWidgetTargetIds::Panel, SdStyleInteractionState::Normal, SdRootLayer::Content, SdDesignTokenIds::PanelBg);
		Detail::SdAddDefaultUserAgentRootBackgroundRule(sheet, SdWidgetTargetIds::ImageViewer, SdStyleInteractionState::Normal, SdRootLayer::Content, SdDesignTokenIds::PanelBg);
		Detail::SdAddDefaultUserAgentRootBackgroundRule(sheet, SdWidgetTargetIds::Popup, SdStyleInteractionState::Normal, SdRootLayer::Popup, SdDesignTokenIds::WindowBg, true);
		Detail::SdAddDefaultUserAgentRootBackgroundRule(sheet, SdWidgetTargetIds::ContextMenu, SdStyleInteractionState::Normal, SdRootLayer::Popup, SdDesignTokenIds::WindowBg, true);
		Detail::SdAddDefaultUserAgentRootBackgroundRule(sheet, SdWidgetTargetIds::Tooltip, SdStyleInteractionState::Normal, SdRootLayer::Tooltip, SdDesignTokenIds::PanelBg, true);

		return sheet;
	}
}
