#pragma once

#include "SdUiCore.h"

#include <array>
#include <vector>

namespace Sodium
{
	struct SdTheme final
	{
		std::array<SdColor, 16> colors = {};
		std::array<float, 16> metrics = {};

		SdTheme()
		{
			colors[static_cast<SdSize>(SdStyleToken::ColorText)] = SdColorWhite;
			colors[static_cast<SdSize>(SdStyleToken::ColorWindowBg)] = { 24, 30, 39, 242 };
			colors[static_cast<SdSize>(SdStyleToken::ColorPanelBg)] = { 35, 42, 52, 235 };
			colors[static_cast<SdSize>(SdStyleToken::ColorButton)] = { 48, 72, 96, 255 };
			colors[static_cast<SdSize>(SdStyleToken::ColorButtonHovered)] = { 62, 100, 138, 255 };
			colors[static_cast<SdSize>(SdStyleToken::ColorButtonPressed)] = { 68, 118, 160, 255 };
			colors[static_cast<SdSize>(SdStyleToken::ColorAccent)] = { 82, 170, 128, 255 };
			colors[static_cast<SdSize>(SdStyleToken::ColorBackground)] = { 24, 30, 39, 242 };
			metrics[static_cast<SdSize>(SdStyleToken::SpacingSmall)] = 6.0f;
			metrics[static_cast<SdSize>(SdStyleToken::SpacingMedium)] = 10.0f;
			metrics[static_cast<SdSize>(SdStyleToken::RadiusSmall)] = 5.0f;
			metrics[static_cast<SdSize>(SdStyleToken::DurationFast)] = 0.16f;
		}

		SdColor GetColor(SdStyleToken token) const noexcept
		{
			return colors[static_cast<SdSize>(token)];
		}

		float GetMetric(SdStyleToken token) const noexcept
		{
			return metrics[static_cast<SdSize>(token)];
		}
	};

	struct SdStyleRule final
	{
		SdStyleWidgetClass widgetClass = SdStyleWidgetClass::Default;
		SdStyleInteractionState interactionState = SdStyleInteractionState::Normal;
		SdStyleToken backgroundToken = SdStyleToken::ColorBackground;
		SdStyleToken colorToken = SdStyleToken::ColorText;
		SdStyleToken radiusToken = SdStyleToken::RadiusSmall;
		bool hasBackground = false;
		bool hasColor = false;
		bool hasRadius = false;
	};

	class SdStyleSystem final
	{
	private:
		SdTheme theme = {};
		std::vector<SdStyleRule> rules = {};

	public:
		SdStyleSystem()
		{
			AddRule({ SdStyleWidgetClass::Default, SdStyleInteractionState::Normal, SdStyleToken::ColorBackground, SdStyleToken::ColorText, SdStyleToken::RadiusSmall, true, true, true });
			AddRule({ SdStyleWidgetClass::Panel, SdStyleInteractionState::Normal, SdStyleToken::ColorPanelBg, SdStyleToken::ColorText, SdStyleToken::RadiusSmall, true, true, true });
			AddRule({ SdStyleWidgetClass::Button, SdStyleInteractionState::Normal, SdStyleToken::ColorButton, SdStyleToken::ColorText, SdStyleToken::RadiusSmall, true, true, true });
			AddRule({ SdStyleWidgetClass::Button, SdStyleInteractionState::Hovered, SdStyleToken::ColorButtonHovered, SdStyleToken::ColorText, SdStyleToken::RadiusSmall, true, true, true });
			AddRule({ SdStyleWidgetClass::Button, SdStyleInteractionState::Pressed, SdStyleToken::ColorButtonPressed, SdStyleToken::ColorText, SdStyleToken::RadiusSmall, true, true, true });
			AddRule({ SdStyleWidgetClass::CheckBox, SdStyleInteractionState::Normal, SdStyleToken::ColorPanelBg, SdStyleToken::ColorText, SdStyleToken::RadiusSmall, true, true, true });
			AddRule({ SdStyleWidgetClass::CheckBox, SdStyleInteractionState::Hovered, SdStyleToken::ColorButtonHovered, SdStyleToken::ColorText, SdStyleToken::RadiusSmall, true, true, true });
			AddRule({ SdStyleWidgetClass::Window, SdStyleInteractionState::Normal, SdStyleToken::ColorWindowBg, SdStyleToken::ColorText, SdStyleToken::RadiusSmall, true, true, true });
			AddRule({ SdStyleWidgetClass::ImageViewer, SdStyleInteractionState::Normal, SdStyleToken::ColorPanelBg, SdStyleToken::ColorText, SdStyleToken::RadiusSmall, true, true, true });
			AddRule({ SdStyleWidgetClass::Slider, SdStyleInteractionState::Normal, SdStyleToken::ColorPanelBg, SdStyleToken::ColorText, SdStyleToken::RadiusSmall, true, true, true });
			AddRule({ SdStyleWidgetClass::Slider, SdStyleInteractionState::Hovered, SdStyleToken::ColorButtonHovered, SdStyleToken::ColorText, SdStyleToken::RadiusSmall, true, true, true });
			AddRule({ SdStyleWidgetClass::Slider, SdStyleInteractionState::Pressed, SdStyleToken::ColorButtonPressed, SdStyleToken::ColorText, SdStyleToken::RadiusSmall, true, true, true });
			AddRule({ SdStyleWidgetClass::TextInput, SdStyleInteractionState::Normal, SdStyleToken::ColorPanelBg, SdStyleToken::ColorText, SdStyleToken::RadiusSmall, true, true, true });
			AddRule({ SdStyleWidgetClass::TextInput, SdStyleInteractionState::Focused, SdStyleToken::ColorButton, SdStyleToken::ColorText, SdStyleToken::RadiusSmall, true, true, true });
			AddRule({ SdStyleWidgetClass::ScrollView, SdStyleInteractionState::Normal, SdStyleToken::ColorPanelBg, SdStyleToken::ColorText, SdStyleToken::RadiusSmall, true, true, true });
			AddRule({ SdStyleWidgetClass::Popup, SdStyleInteractionState::Normal, SdStyleToken::ColorWindowBg, SdStyleToken::ColorText, SdStyleToken::RadiusSmall, true, true, true });
			AddRule({ SdStyleWidgetClass::ContextMenu, SdStyleInteractionState::Normal, SdStyleToken::ColorWindowBg, SdStyleToken::ColorText, SdStyleToken::RadiusSmall, true, true, true });
			AddRule({ SdStyleWidgetClass::Tooltip, SdStyleInteractionState::Normal, SdStyleToken::ColorPanelBg, SdStyleToken::ColorText, SdStyleToken::RadiusSmall, true, true, true });
		}

		const SdTheme& GetTheme() const noexcept
		{
			return theme;
		}

		void AddRule(const SdStyleRule& rule)
		{
			rules.push_back(rule);
		}

		SdComputedStyle Resolve(SdStyleWidgetClass widgetClass, SdStyleInteractionState interactionState) const
		{
			SdComputedStyle result = {};
			result.color = theme.GetColor(SdStyleToken::ColorText);
			result.background = theme.GetColor(SdStyleToken::ColorBackground);
			result.radius = theme.GetMetric(SdStyleToken::RadiusSmall);

			for (const SdStyleRule& rule : rules)
			{
				if (rule.widgetClass != SdStyleWidgetClass::Default && rule.widgetClass != widgetClass)
					continue;
				if (rule.interactionState != SdStyleInteractionState::Normal && rule.interactionState != interactionState)
					continue;
				if (rule.hasBackground)
					result.background = theme.GetColor(rule.backgroundToken);
				if (rule.hasColor)
					result.color = theme.GetColor(rule.colorToken);
				if (rule.hasRadius)
					result.radius = theme.GetMetric(rule.radiusToken);
			}
			return result;
		}
	};
}
