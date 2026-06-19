#pragma once

#include "SdCore.h"

#include <algorithm>

namespace Sodium
{
	enum class SdLayerPriority : SdUInt8
	{
		Background,
		Content,
		Floating,
		Popup,
		Overlay,
		Debug
	};

	enum class SdLayoutAxis : SdUInt8
	{
		Horizontal,
		Vertical
	};

	enum class SdLayoutAlignment : SdUInt8
	{
		Start,
		Center,
		End,
		Stretch
	};

	struct SdLayoutConstraints final
	{
		SdVec2 minSize = {};
		SdVec2 maxSize = {};
	};

	struct SdLayoutResult final
	{
		SdVec2 desiredSize = {};
		SdRect finalRect = {};
		float baseline = 0.0f;
	};

	enum class SdStyleToken : SdUInt16
	{
		ColorText,
		ColorBackground,
		ColorWindowBg,
		ColorPanelBg,
		ColorButton,
		ColorButtonHovered,
		ColorButtonPressed,
		ColorAccent,
		SpacingSmall,
		SpacingMedium,
		RadiusSmall,
		DurationFast
	};

	enum class SdStyleWidgetClass : SdUInt16
	{
		Default,
		Text,
		Panel,
		Button,
		CheckBox,
		Window,
		ImageViewer
	};

	enum class SdStyleInteractionState : SdUInt8
	{
		Normal,
		Hovered,
		Pressed,
		Focused
	};

	struct SdSpacing final
	{
		float left = 0.0f;
		float top = 0.0f;
		float right = 0.0f;
		float bottom = 0.0f;
	};

	struct SdComputedStyle final
	{
		SdColor color = SdColorWhite;
		SdColor background = SdColorTransparent;
		SdColor border = SdColorTransparent;
		SdSpacing padding = {};
		float width = 0.0f;
		float height = 0.0f;
		float radius = 0.0f;
		float opacity = 1.0f;
	};

	struct SdThemeView final
	{
		SdComputedStyle defaults = {};
	};

	enum class SdAnimationEasing : SdUInt8
	{
		Linear,
		OutCubic
	};

	struct SdTransition final
	{
		SdDuration duration = std::chrono::milliseconds(160);
		SdAnimationEasing easing = SdAnimationEasing::OutCubic;
	};

	struct SdWidgetTransition final
	{
		SdTransition enter = {};
		SdTransition leave = {};
		SdTransition layout = {};
	};

	inline float SdApplyEasing(float value, SdAnimationEasing easing) noexcept
	{
		if (value <= 0.0f)
			return 0.0f;
		if (value >= 1.0f)
			return 1.0f;
		switch (easing)
		{
		case SdAnimationEasing::OutCubic:
		{
			const float inverse = 1.0f - value;
			return 1.0f - inverse * inverse * inverse;
		}
		case SdAnimationEasing::Linear:
		default:
			return value;
		}
	}

	inline float SdMoveToward(float value, float target, float delta) noexcept
	{
		if (value < target)
			return (value + delta > target) ? target : value + delta;
		if (value > target)
			return (value - delta < target) ? target : value - delta;
		return value;
	}
}
