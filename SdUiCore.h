#pragma once

#include "SdCore.h"

#include <algorithm>

namespace Sodium
{
	enum class SdRootLayer : SdUInt16
	{
		Background = 0,
		Content = 100,
		Floating = 200,
		Popup = 300,
		Tooltip = 400,
		ModalBackdrop = 500,
		Modal = 600,
		DragPreview = 700,
		DebugOverlay = 800
	};

	enum class SdPortalRoot : SdUInt8
	{
		None,
		Popup,
		Tooltip,
		Modal,
		DragPreview,
		DebugOverlay
	};

	inline constexpr SdRootLayer SdRootLayerFromPortalRoot(SdPortalRoot portalRoot) noexcept
	{
		switch (portalRoot)
		{
		case SdPortalRoot::Popup:
			return SdRootLayer::Popup;
		case SdPortalRoot::Tooltip:
			return SdRootLayer::Tooltip;
		case SdPortalRoot::Modal:
			return SdRootLayer::Modal;
		case SdPortalRoot::DragPreview:
			return SdRootLayer::DragPreview;
		case SdPortalRoot::DebugOverlay:
			return SdRootLayer::DebugOverlay;
		case SdPortalRoot::None:
		default:
			return SdRootLayer::Content;
		}
	}

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

	using SdStyleId = SdUInt64;
	using SdStyleClassId = SdUInt64;
	using SdStyleScopeId = SdUInt64;
	using SdPropertyId = SdUInt64;
	using SdThemeVariableId = SdUInt64;

	constexpr SdStyleId SdStyleIdLiteral(const char* text, std::size_t length) noexcept
	{
		SdUInt64 hash = 14695981039346656037ull;

		for (std::size_t i = 0; i < length; ++i)
		{
			hash ^= static_cast<SdUInt8>(text[i]);
			hash *= 1099511628211ull;
		}

		return SdStyleId{ hash };
	}

	namespace Literals
	{
		inline constexpr SdStyleId operator""_SdId(const char* text, std::size_t length) noexcept
		{
			return SdStyleIdLiteral(text, length);
		}
	}

	using namespace Literals;

	namespace Detail
	{
		template<class T>
		SdUInt64 SdTypeHash() noexcept
		{
			static const int anchor = 0;
			return static_cast<SdUInt64>(reinterpret_cast<std::uintptr_t>(&anchor));
		}
	}

	struct SdStyleIdentity final
	{
		SdSpan<const SdStyleClassId> classes = {};
		SdStyleScopeId scope = 0;
	};

	namespace SdWidgetTargetIds
	{
		inline constexpr SdStyleId Global = "Sodium.Style.Target.Global"_SdId;
		inline constexpr SdStyleId Default = "Sodium.Style.Target.Default"_SdId;
		inline constexpr SdStyleId Text = "Sodium.Style.Target.Text"_SdId;
		inline constexpr SdStyleId Panel = "Sodium.Style.Target.Panel"_SdId;
		inline constexpr SdStyleId Button = "Sodium.Style.Target.Button"_SdId;
		inline constexpr SdStyleId CheckBox = "Sodium.Style.Target.CheckBox"_SdId;
		inline constexpr SdStyleId Window = "Sodium.Style.Target.Window"_SdId;
		inline constexpr SdStyleId ImageViewer = "Sodium.Style.Target.ImageViewer"_SdId;
		inline constexpr SdStyleId Slider = "Sodium.Style.Target.Slider"_SdId;
		inline constexpr SdStyleId TextInput = "Sodium.Style.Target.TextInput"_SdId;
		inline constexpr SdStyleId ScrollView = "Sodium.Style.Target.ScrollView"_SdId;
		inline constexpr SdStyleId Popup = "Sodium.Style.Target.Popup"_SdId;
		inline constexpr SdStyleId ContextMenu = "Sodium.Style.Target.ContextMenu"_SdId;
		inline constexpr SdStyleId Tooltip = "Sodium.Style.Target.Tooltip"_SdId;
	}

	namespace SdThemeVariableIds
	{
		inline constexpr SdThemeVariableId Text = "text"_SdId;
		inline constexpr SdThemeVariableId Background = "background"_SdId;
		inline constexpr SdThemeVariableId WindowBg = "window.bg"_SdId;
		inline constexpr SdThemeVariableId PanelBg = "panel.bg"_SdId;
		inline constexpr SdThemeVariableId ButtonBg = "button.bg"_SdId;
		inline constexpr SdThemeVariableId ButtonBgHover = "button.bg.hover"_SdId;
		inline constexpr SdThemeVariableId ButtonBgPressed = "button.bg.pressed"_SdId;
		inline constexpr SdThemeVariableId ButtonText = "button.text"_SdId;
		inline constexpr SdThemeVariableId Accent = "accent"_SdId;
		inline constexpr SdThemeVariableId Border = "border"_SdId;
		inline constexpr SdThemeVariableId BorderStrong = "border.strong"_SdId;
		inline constexpr SdThemeVariableId Danger = "danger"_SdId;
		inline constexpr SdThemeVariableId Selection = "selection"_SdId;
		inline constexpr SdThemeVariableId SpacingSmall = "spacing.small"_SdId;
		inline constexpr SdThemeVariableId SpacingMedium = "spacing.medium"_SdId;
		inline constexpr SdThemeVariableId FontButton = "font.button"_SdId;
		inline constexpr SdThemeVariableId RadiusSmall = "radius.small"_SdId;
		inline constexpr SdThemeVariableId DurationFast = "duration.fast"_SdId;
	}

	enum class SdStyleInteractionState : SdUInt8
	{
		Normal,
		Hovered,
		Pressed,
		Focused
	};

	enum class SdStyleFieldImpact : SdUInt8
	{
		Layout,
		Paint,
		Composite,
		Discrete
	};

	enum class SdStyleInterpolation : SdUInt8
	{
		None,
		Float,
		Color,
		Spacing,
		Vec2
	};

	struct SdSpacing final
	{
		float left = 0.0f;
		float top = 0.0f;
		float right = 0.0f;
		float bottom = 0.0f;
	};

	enum class SdStyleValueKind : SdUInt8
	{
		None,
		Color,
		Float,
		Spacing,
		Vec2,
		Length,
		ColorVariable,
		MetricVariable
	};

	struct SdStyleValue final
	{
		SdStyleValueKind kind = SdStyleValueKind::None;
		SdColor color = SdColorTransparent;
		float number = 0.0f;
		SdSpacing spacing = {};
		SdVec2 vec2 = {};
		SdUInt8 lengthUnit = 0;
		float lengthValue = 0.0f;
		SdThemeVariableId lengthVariableId = 0;
		SdThemeVariableId variableId = 0;

		static constexpr SdStyleValue FromColor(SdColor value) noexcept
		{
			SdStyleValue result = {};
			result.kind = SdStyleValueKind::Color;
			result.color = value;
			return result;
		}

		static constexpr SdStyleValue FromFloat(float value) noexcept
		{
			SdStyleValue result = {};
			result.kind = SdStyleValueKind::Float;
			result.number = value;
			return result;
		}

		static constexpr SdStyleValue FromSpacing(SdSpacing value) noexcept
		{
			SdStyleValue result = {};
			result.kind = SdStyleValueKind::Spacing;
			result.spacing = value;
			return result;
		}

		static constexpr SdStyleValue FromVec2(SdVec2 value) noexcept
		{
			SdStyleValue result = {};
			result.kind = SdStyleValueKind::Vec2;
			result.vec2 = value;
			return result;
		}

		static constexpr SdStyleValue FromLength(SdUInt8 unit, float value, SdThemeVariableId variableId = 0) noexcept
		{
			SdStyleValue result = {};
			result.kind = SdStyleValueKind::Length;
			result.lengthUnit = unit;
			result.lengthValue = value;
			result.lengthVariableId = variableId;
			return result;
		}

		static constexpr SdStyleValue FromColorVariable(SdThemeVariableId value) noexcept
		{
			SdStyleValue result = {};
			result.kind = SdStyleValueKind::ColorVariable;
			result.variableId = value;
			return result;
		}

		static constexpr SdStyleValue FromMetricVariable(SdThemeVariableId value) noexcept
		{
			SdStyleValue result = {};
			result.kind = SdStyleValueKind::MetricVariable;
			result.variableId = value;
			return result;
		}
	};

	inline constexpr SdStyleValue ThemeColor(SdThemeVariableId id) noexcept
	{
		return SdStyleValue::FromColorVariable(id);
	}

	inline constexpr SdStyleValue ThemeMetric(SdThemeVariableId id) noexcept
	{
		return SdStyleValue::FromMetricVariable(id);
	}

	struct SdThemeView final
	{
	};

	enum class SdAnimationEasing : SdUInt8
	{
		Linear,
		OutCubic
	};

	enum class SdTransitionBehavior : SdUInt8
	{
		Normal,
		AllowDiscrete
	};

	struct SdTransition final
	{
		SdDuration duration = 160ms;
		SdAnimationEasing easing = SdAnimationEasing::OutCubic;
		SdDuration delay = {};
		SdTransitionBehavior behavior = SdTransitionBehavior::Normal;
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
