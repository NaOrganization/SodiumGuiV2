#pragma once

#include "SdUiCore.h"

#include <array>
#include <vector>

namespace Sodium
{
	struct SdTheme final
	{
		std::array<SdColor, static_cast<SdSize>(SdStyleToken::Count)> colors = {};
		std::array<float, static_cast<SdSize>(SdStyleToken::Count)> metrics = {};

		SdTheme()
		{
			colors[static_cast<SdSize>(SdStyleToken::ColorText)] = SdColorWhite;
			colors[static_cast<SdSize>(SdStyleToken::ColorWindowBg)] = { 24, 30, 39, 242 };
			colors[static_cast<SdSize>(SdStyleToken::ColorPanelBg)] = { 35, 42, 52, 235 };
			colors[static_cast<SdSize>(SdStyleToken::ColorButton)] = { 48, 72, 96, 255 };
			colors[static_cast<SdSize>(SdStyleToken::ColorButtonHovered)] = { 62, 100, 138, 255 };
			colors[static_cast<SdSize>(SdStyleToken::ColorButtonPressed)] = { 68, 118, 160, 255 };
			colors[static_cast<SdSize>(SdStyleToken::ColorAccent)] = { 82, 170, 128, 255 };
			colors[static_cast<SdSize>(SdStyleToken::ColorBorder)] = { 91, 109, 128, 255 };
			colors[static_cast<SdSize>(SdStyleToken::ColorBorderStrong)] = { 128, 154, 180, 255 };
			colors[static_cast<SdSize>(SdStyleToken::ColorDanger)] = { 164, 66, 66, 255 };
			colors[static_cast<SdSize>(SdStyleToken::ColorSelection)] = { 82, 170, 128, 96 };
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
		SdLayerPriority layerPriority = SdLayerPriority::Content;
		SdStyleToken backgroundToken = SdStyleToken::ColorBackground;
		SdStyleToken colorToken = SdStyleToken::ColorText;
		SdStyleToken radiusToken = SdStyleToken::RadiusSmall;
		bool hasBackground = false;
		bool hasColor = false;
		bool hasRadius = false;
		bool matchLayer = false;
	};

	class SdStyleSystem final
	{
	private:
		SdTheme theme = {};
		std::vector<SdStyleRule> rules = {};

	public:
		SdStyleSystem()
		{
			AddRule({ SdStyleWidgetClass::Default, SdStyleInteractionState::Normal, SdLayerPriority::Content, SdStyleToken::ColorBackground, SdStyleToken::ColorText, SdStyleToken::RadiusSmall, true, true, true });
			AddRule({ SdStyleWidgetClass::Panel, SdStyleInteractionState::Normal, SdLayerPriority::Content, SdStyleToken::ColorPanelBg, SdStyleToken::ColorText, SdStyleToken::RadiusSmall, true, true, true });
			AddRule({ SdStyleWidgetClass::Button, SdStyleInteractionState::Normal, SdLayerPriority::Content, SdStyleToken::ColorButton, SdStyleToken::ColorText, SdStyleToken::RadiusSmall, true, true, true });
			AddRule({ SdStyleWidgetClass::Button, SdStyleInteractionState::Hovered, SdLayerPriority::Content, SdStyleToken::ColorButtonHovered, SdStyleToken::ColorText, SdStyleToken::RadiusSmall, true, true, true });
			AddRule({ SdStyleWidgetClass::Button, SdStyleInteractionState::Pressed, SdLayerPriority::Content, SdStyleToken::ColorButtonPressed, SdStyleToken::ColorText, SdStyleToken::RadiusSmall, true, true, true });
			AddRule({ SdStyleWidgetClass::CheckBox, SdStyleInteractionState::Normal, SdLayerPriority::Content, SdStyleToken::ColorPanelBg, SdStyleToken::ColorText, SdStyleToken::RadiusSmall, true, true, true });
			AddRule({ SdStyleWidgetClass::CheckBox, SdStyleInteractionState::Hovered, SdLayerPriority::Content, SdStyleToken::ColorButtonHovered, SdStyleToken::ColorText, SdStyleToken::RadiusSmall, true, true, true });
			AddRule({ SdStyleWidgetClass::Window, SdStyleInteractionState::Normal, SdLayerPriority::Floating, SdStyleToken::ColorWindowBg, SdStyleToken::ColorText, SdStyleToken::RadiusSmall, true, true, true, true });
			AddRule({ SdStyleWidgetClass::ImageViewer, SdStyleInteractionState::Normal, SdLayerPriority::Content, SdStyleToken::ColorPanelBg, SdStyleToken::ColorText, SdStyleToken::RadiusSmall, true, true, true });
			AddRule({ SdStyleWidgetClass::Slider, SdStyleInteractionState::Normal, SdLayerPriority::Content, SdStyleToken::ColorPanelBg, SdStyleToken::ColorText, SdStyleToken::RadiusSmall, true, true, true });
			AddRule({ SdStyleWidgetClass::Slider, SdStyleInteractionState::Hovered, SdLayerPriority::Content, SdStyleToken::ColorButtonHovered, SdStyleToken::ColorText, SdStyleToken::RadiusSmall, true, true, true });
			AddRule({ SdStyleWidgetClass::Slider, SdStyleInteractionState::Pressed, SdLayerPriority::Content, SdStyleToken::ColorButtonPressed, SdStyleToken::ColorText, SdStyleToken::RadiusSmall, true, true, true });
			AddRule({ SdStyleWidgetClass::TextInput, SdStyleInteractionState::Normal, SdLayerPriority::Content, SdStyleToken::ColorPanelBg, SdStyleToken::ColorText, SdStyleToken::RadiusSmall, true, true, true });
			AddRule({ SdStyleWidgetClass::TextInput, SdStyleInteractionState::Focused, SdLayerPriority::Content, SdStyleToken::ColorButton, SdStyleToken::ColorText, SdStyleToken::RadiusSmall, true, true, true });
			AddRule({ SdStyleWidgetClass::ScrollView, SdStyleInteractionState::Normal, SdLayerPriority::Content, SdStyleToken::ColorPanelBg, SdStyleToken::ColorText, SdStyleToken::RadiusSmall, true, true, true });
			AddRule({ SdStyleWidgetClass::Popup, SdStyleInteractionState::Normal, SdLayerPriority::Popup, SdStyleToken::ColorWindowBg, SdStyleToken::ColorText, SdStyleToken::RadiusSmall, true, true, true, true });
			AddRule({ SdStyleWidgetClass::ContextMenu, SdStyleInteractionState::Normal, SdLayerPriority::Popup, SdStyleToken::ColorWindowBg, SdStyleToken::ColorText, SdStyleToken::RadiusSmall, true, true, true, true });
			AddRule({ SdStyleWidgetClass::Tooltip, SdStyleInteractionState::Normal, SdLayerPriority::Overlay, SdStyleToken::ColorPanelBg, SdStyleToken::ColorText, SdStyleToken::RadiusSmall, true, true, true, true });
		}

		const SdTheme& GetTheme() const noexcept
		{
			return theme;
		}

		void AddRule(const SdStyleRule& rule)
		{
			rules.push_back(rule);
		}

		SdComputedStyle Resolve(
			SdStyleWidgetClass widgetClass,
			SdStyleInteractionState interactionState,
			SdLayerPriority layerPriority = SdLayerPriority::Content) const
		{
			SdComputedStyle result = {};
			result.color = theme.GetColor(SdStyleToken::ColorText);
			result.background = theme.GetColor(SdStyleToken::ColorBackground);
			result.border = theme.GetColor(SdStyleToken::ColorBorder);
			result.radius = theme.GetMetric(SdStyleToken::RadiusSmall);

			for (const SdStyleRule& rule : rules)
			{
				if (rule.widgetClass != SdStyleWidgetClass::Default && rule.widgetClass != widgetClass)
					continue;
				if (rule.interactionState != SdStyleInteractionState::Normal && rule.interactionState != interactionState)
					continue;
				if (rule.matchLayer && rule.layerPriority != layerPriority)
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
