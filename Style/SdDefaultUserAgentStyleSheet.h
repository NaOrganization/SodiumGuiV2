#pragma once

#include <algorithm>

namespace Sodium
{
	namespace Detail
	{
		inline void SdAddDefaultUserAgentRootBackgroundRule(
			SdStyleSheet& sheet,
			SdStyleTargetId targetTag,
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
		auto global = sheet.RuleForTarget<SdWidgetRootStyle>(SdStyleTargetIds::Global);
		global.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::color, DesignColor(SdDesignTokenIds::Text))
			.Set(&SdBoxStyle::backgroundColor, DesignColor(SdDesignTokenIds::Background))
			.Set(&SdBoxStyle::border, DesignColor(SdDesignTokenIds::Border))
			.Set(&SdBoxStyle::radius, DesignMetric(SdDesignTokenIds::RadiusSmall))
			.Set(&SdBoxStyle::opacity, 1.0f)
			.Set(&SdBoxStyle::fontSize, DesignMetric(SdDesignTokenIds::FontButton))
			.Set(&SdBoxStyle::lineHeight, 0.0f);

		const float mediumSpacing = tokenSet.GetMetric(SdDesignTokenIds::SpacingMedium);
		sheet.RuleForTarget<SdWidgetRootStyle>(SdStyleTargetIds::Panel)
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::width, SdLength::Pixels(240.0f))
			.Set(&SdBoxStyle::height, SdLength::Pixels(120.0f))
			.Set(&SdBoxStyle::padding, SdStyleValue::FromSpacing({ mediumSpacing, mediumSpacing, mediumSpacing, mediumSpacing }))
			.Set(&SdBoxStyle::gap, SdLength::Pixels(tokenSet.GetMetric(SdDesignTokenIds::SpacingSmall)));
		sheet.RuleForTarget<SdWidgetRootStyle>(SdStyleTargetIds::Window)
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::width, SdLength::Pixels(420.0f))
			.Set(&SdBoxStyle::height, SdLength::Pixels(260.0f))
			.Set(&SdBoxStyle::padding, SdStyleValue::FromSpacing({ mediumSpacing, 40.0f, mediumSpacing, mediumSpacing }))
			.Set(&SdBoxStyle::fontSize, DesignMetric(SdDesignTokenIds::FontButton))
			.Set(&SdBoxStyle::lineHeight, 0.0f)
			.Set(&SdBoxStyle::radius, DesignMetric(SdDesignTokenIds::RadiusSmall))
			.Set(&SdBoxStyle::gap, SdLength::Pixels(tokenSet.GetMetric(SdDesignTokenIds::SpacingSmall)))
			.Set(&SdBoxStyle::opacity, 1.0f);
		sheet.PartForTarget(SdStyleTargetIds::Window, SdWidgetPartIds::Window::Content)
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::backgroundColor, DesignColor(SdDesignTokenIds::WindowBg))
			.Set(&SdBoxStyle::border, DesignColor(SdDesignTokenIds::Border))
			.Set(&SdBoxStyle::radius, DesignMetric(SdDesignTokenIds::RadiusSmall));
		sheet.PartForTarget(SdStyleTargetIds::Window, SdWidgetPartIds::Window::Titlebar)
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::backgroundColor, DesignColor(SdDesignTokenIds::ButtonBg))
			.Set(&SdBoxStyle::radius, DesignMetric(SdDesignTokenIds::RadiusSmall));

		const float smallSpacing = tokenSet.GetMetric(SdDesignTokenIds::SpacingSmall);
		sheet.RuleForTarget<SdWidgetRootStyle>(SdStyleTargetIds::Button)
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::minWidth, SdLength::Pixels(82.0f))
			.Set(&SdBoxStyle::minHeight, SdLength::Pixels(30.0f))
			.Set(&SdBoxStyle::padding, SdStyleValue::FromSpacing({ mediumSpacing, smallSpacing, mediumSpacing, smallSpacing }))
			.Set(&SdBoxStyle::fontSize, DesignMetric(SdDesignTokenIds::FontButton))
			.Set(&SdBoxStyle::lineHeight, 0.0f);
		sheet.PartForTarget(SdStyleTargetIds::Button, SdWidgetPartIds::Button::Content)
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::backgroundColor, DesignColor(SdDesignTokenIds::ButtonBg))
			.Set(&SdBoxStyle::border, DesignColor(SdDesignTokenIds::Border))
			.Set(&SdBoxStyle::radius, DesignMetric(SdDesignTokenIds::RadiusSmall));
		sheet.PartForTarget(SdStyleTargetIds::Button, SdWidgetPartIds::Button::Content)
			.Cascade(SdCascadeLayer::UserAgent)
			.Pseudo(SdStyleInteractionState::Hovered)
			.Set(&SdBoxStyle::backgroundColor, DesignColor(SdDesignTokenIds::ButtonBgHover));
		sheet.PartForTarget(SdStyleTargetIds::Button, SdWidgetPartIds::Button::Content)
			.Cascade(SdCascadeLayer::UserAgent)
			.Pseudo(SdStyleInteractionState::Pressed)
			.Set(&SdBoxStyle::backgroundColor, DesignColor(SdDesignTokenIds::ButtonBgPressed));

		sheet.RuleForTarget<SdWidgetRootStyle>(SdStyleTargetIds::CheckBox)
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::minHeight, SdLength::Pixels(28.0f))
			.Set(&SdBoxStyle::padding, SdStyleValue::FromSpacing({ smallSpacing, smallSpacing, smallSpacing, smallSpacing }))
			.Set(&SdBoxStyle::fontSize, DesignMetric(SdDesignTokenIds::FontButton))
			.Set(&SdBoxStyle::lineHeight, 0.0f)
			.Set(&SdBoxStyle::gap, SdLength::Pixels(smallSpacing))
			.Set(&SdBoxStyle::radius, SdLength::Pixels(std::max(2.0f, tokenSet.GetMetric(SdDesignTokenIds::RadiusSmall) - 1.0f)));
		sheet.PartForTarget(SdStyleTargetIds::CheckBox, SdWidgetPartIds::CheckBox::Box)
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::backgroundColor, DesignColor(SdDesignTokenIds::PanelBg))
			.Set(&SdBoxStyle::border, DesignColor(SdDesignTokenIds::Border))
			.Set(&SdBoxStyle::radius, SdLength::Pixels(std::max(2.0f, tokenSet.GetMetric(SdDesignTokenIds::RadiusSmall) - 1.0f)));
		sheet.PartForTarget(SdStyleTargetIds::CheckBox, SdWidgetPartIds::CheckBox::Box)
			.Cascade(SdCascadeLayer::UserAgent)
			.Pseudo(SdStyleInteractionState::Hovered)
			.Set(&SdBoxStyle::backgroundColor, DesignColor(SdDesignTokenIds::ButtonBgHover));
		sheet.PartForTarget(SdStyleTargetIds::CheckBox, SdWidgetPartIds::CheckBox::Indicator)
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::backgroundColor, DesignColor(SdDesignTokenIds::Accent));

		sheet.RuleForTarget<SdWidgetRootStyle>(SdStyleTargetIds::Slider)
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::width, SdLength::Pixels(180.0f))
			.Set(&SdBoxStyle::height, SdLength::Pixels(30.0f))
			.Set(&SdBoxStyle::padding, SdStyleValue::FromSpacing({ smallSpacing, smallSpacing, smallSpacing, smallSpacing }))
			.Set(&SdBoxStyle::fontSize, DesignMetric(SdDesignTokenIds::FontButton))
			.Set(&SdBoxStyle::lineHeight, 0.0f)
			.Set(&SdBoxStyle::gap, SdLength::Pixels(smallSpacing));
		sheet.PartForTarget(SdStyleTargetIds::Slider, SdWidgetPartIds::Slider::Track)
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::backgroundColor, DesignColor(SdDesignTokenIds::PanelBg))
			.Set(&SdBoxStyle::border, DesignColor(SdDesignTokenIds::Border))
			.Set(&SdBoxStyle::radius, DesignMetric(SdDesignTokenIds::RadiusSmall));
		sheet.PartForTarget(SdStyleTargetIds::Slider, SdWidgetPartIds::Slider::Track)
			.Cascade(SdCascadeLayer::UserAgent)
			.Pseudo(SdStyleInteractionState::Hovered)
			.Set(&SdBoxStyle::backgroundColor, DesignColor(SdDesignTokenIds::ButtonBgHover));
		sheet.PartForTarget(SdStyleTargetIds::Slider, SdWidgetPartIds::Slider::Track)
			.Cascade(SdCascadeLayer::UserAgent)
			.Pseudo(SdStyleInteractionState::Pressed)
			.Set(&SdBoxStyle::backgroundColor, DesignColor(SdDesignTokenIds::ButtonBgPressed));
		sheet.PartForTarget(SdStyleTargetIds::Slider, SdWidgetPartIds::Slider::Fill)
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::backgroundColor, DesignColor(SdDesignTokenIds::Accent));
		sheet.PartForTarget(SdStyleTargetIds::Slider, SdWidgetPartIds::Slider::Thumb)
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::backgroundColor, DesignColor(SdDesignTokenIds::Accent))
			.Set(&SdBoxStyle::border, DesignColor(SdDesignTokenIds::Border));

		sheet.RuleForTarget<SdWidgetRootStyle>(SdStyleTargetIds::TextInput)
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::width, SdLength::Pixels(220.0f))
			.Set(&SdBoxStyle::minHeight, SdLength::Pixels(32.0f))
			.Set(&SdBoxStyle::padding, SdStyleValue::FromSpacing({ mediumSpacing, smallSpacing, mediumSpacing, smallSpacing }))
			.Set(&SdBoxStyle::fontSize, DesignMetric(SdDesignTokenIds::FontButton))
			.Set(&SdBoxStyle::lineHeight, 0.0f);
		sheet.PartForTarget(SdStyleTargetIds::TextInput, SdWidgetPartIds::TextInput::Field)
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::backgroundColor, DesignColor(SdDesignTokenIds::PanelBg))
			.Set(&SdBoxStyle::border, DesignColor(SdDesignTokenIds::Border))
			.Set(&SdBoxStyle::radius, DesignMetric(SdDesignTokenIds::RadiusSmall));
		sheet.PartForTarget(SdStyleTargetIds::TextInput, SdWidgetPartIds::TextInput::Field)
			.Cascade(SdCascadeLayer::UserAgent)
			.Pseudo(SdStyleInteractionState::Focused)
			.Set(&SdBoxStyle::backgroundColor, DesignColor(SdDesignTokenIds::ButtonBg));
		sheet.PartForTarget(SdStyleTargetIds::TextInput, SdWidgetPartIds::TextInput::Placeholder)
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::opacity, 0.52f);

		sheet.RuleForTarget<SdWidgetRootStyle>(SdStyleTargetIds::ImageViewer)
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::width, SdLength::Pixels(160.0f))
			.Set(&SdBoxStyle::height, SdLength::Pixels(120.0f));
		sheet.RuleForTarget<SdWidgetRootStyle>(SdStyleTargetIds::ScrollView)
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::width, SdLength::Pixels(240.0f))
			.Set(&SdBoxStyle::height, SdLength::Pixels(160.0f))
			.Set(&SdBoxStyle::padding, SdStyleValue::FromSpacing({ smallSpacing, smallSpacing, smallSpacing, smallSpacing }))
			.Set(&SdBoxStyle::gap, SdLength::Pixels(smallSpacing));
		sheet.PartForTarget(SdStyleTargetIds::ScrollView, SdWidgetPartIds::ScrollView::Scrollbar)
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::backgroundColor, DesignColor(SdDesignTokenIds::PanelBg))
			.Set(&SdBoxStyle::border, DesignColor(SdDesignTokenIds::Border))
			.Set(&SdBoxStyle::radius, DesignMetric(SdDesignTokenIds::RadiusSmall));
		sheet.PartForTarget(SdStyleTargetIds::ScrollView, SdWidgetPartIds::ScrollView::Thumb)
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::backgroundColor, DesignColor(SdDesignTokenIds::Accent));

		sheet.RuleForTarget<SdWidgetRootStyle>(SdStyleTargetIds::Popup)
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::width, SdLength::Pixels(220.0f))
			.Set(&SdBoxStyle::height, SdLength::Pixels(140.0f))
			.Set(&SdBoxStyle::padding, SdStyleValue::FromSpacing({ smallSpacing, smallSpacing, smallSpacing, smallSpacing }))
			.Set(&SdBoxStyle::gap, SdLength::Pixels(smallSpacing));
		sheet.RuleForTarget<SdWidgetRootStyle>(SdStyleTargetIds::ContextMenu)
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::width, SdLength::Pixels(220.0f))
			.Set(&SdBoxStyle::height, SdLength::Pixels(140.0f))
			.Set(&SdBoxStyle::padding, SdStyleValue::FromSpacing({ smallSpacing, smallSpacing, smallSpacing, smallSpacing }))
			.Set(&SdBoxStyle::gap, SdLength::Pixels(smallSpacing));
		sheet.RuleForTarget<SdWidgetRootStyle>(SdStyleTargetIds::Tooltip)
			.Cascade(SdCascadeLayer::UserAgent)
			.Set(&SdBoxStyle::padding, SdStyleValue::FromSpacing({ smallSpacing, smallSpacing, smallSpacing, smallSpacing }))
			.Set(&SdBoxStyle::fontSize, DesignMetric(SdDesignTokenIds::FontButton))
			.Set(&SdBoxStyle::lineHeight, 0.0f);

		Detail::SdAddDefaultUserAgentRootBackgroundRule(sheet, SdStyleTargetIds::Default, SdStyleInteractionState::Normal, SdRootLayer::Content, SdDesignTokenIds::Background);
		Detail::SdAddDefaultUserAgentRootBackgroundRule(sheet, SdStyleTargetIds::Panel, SdStyleInteractionState::Normal, SdRootLayer::Content, SdDesignTokenIds::PanelBg);
		Detail::SdAddDefaultUserAgentRootBackgroundRule(sheet, SdStyleTargetIds::ImageViewer, SdStyleInteractionState::Normal, SdRootLayer::Content, SdDesignTokenIds::PanelBg);
		Detail::SdAddDefaultUserAgentRootBackgroundRule(sheet, SdStyleTargetIds::Popup, SdStyleInteractionState::Normal, SdRootLayer::Popup, SdDesignTokenIds::WindowBg, true);
		Detail::SdAddDefaultUserAgentRootBackgroundRule(sheet, SdStyleTargetIds::ContextMenu, SdStyleInteractionState::Normal, SdRootLayer::Popup, SdDesignTokenIds::WindowBg, true);
		Detail::SdAddDefaultUserAgentRootBackgroundRule(sheet, SdStyleTargetIds::Tooltip, SdStyleInteractionState::Normal, SdRootLayer::Tooltip, SdDesignTokenIds::PanelBg, true);

		return sheet;
	}
}
