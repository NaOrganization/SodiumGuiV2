#pragma once

#include "Core/SdStableTypeId.h"

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

	namespace Literals
	{
		constexpr SdUInt64 operator"" _SdId(const char* text, std::size_t length) noexcept
		{
			SdUInt64 hash = 14695981039346656037ull;
			for (std::size_t i = 0; i < length; ++i)
			{
				hash ^= static_cast<SdUInt8>(text[i]);
				hash *= 1099511628211ull;
			}
			return hash;
		}
	}

	using namespace Literals;

	namespace Detail
	{
		template<class T>
		consteval SdUInt64 SdTypeHash()
		{
			return SdStableTypeId<T>();
		}
	}

	struct SdStyleIdentity final
	{
		SdSpan<const SdStyleClassId> classes = {};
		SdStyleScopeId scope = 0;
	};

	namespace SdStyleTargetIds
	{
		inline constexpr SdStyleTargetId Global = "Sodium.Style.Target.Global"_SdId;
		inline constexpr SdStyleTargetId Default = "Sodium.Style.Target.Default"_SdId;
		inline constexpr SdStyleTargetId Text = "Sodium.Style.Target.Text"_SdId;
		inline constexpr SdStyleTargetId Panel = "Sodium.Style.Target.Panel"_SdId;
		inline constexpr SdStyleTargetId Button = "Sodium.Style.Target.Button"_SdId;
		inline constexpr SdStyleTargetId CheckBox = "Sodium.Style.Target.CheckBox"_SdId;
		inline constexpr SdStyleTargetId Window = "Sodium.Style.Target.Window"_SdId;
		inline constexpr SdStyleTargetId ImageViewer = "Sodium.Style.Target.ImageViewer"_SdId;
		inline constexpr SdStyleTargetId Slider = "Sodium.Style.Target.Slider"_SdId;
		inline constexpr SdStyleTargetId TextInput = "Sodium.Style.Target.TextInput"_SdId;
		inline constexpr SdStyleTargetId ScrollView = "Sodium.Style.Target.ScrollView"_SdId;
		inline constexpr SdStyleTargetId Popup = "Sodium.Style.Target.Popup"_SdId;
		inline constexpr SdStyleTargetId ContextMenu = "Sodium.Style.Target.ContextMenu"_SdId;
		inline constexpr SdStyleTargetId Tooltip = "Sodium.Style.Target.Tooltip"_SdId;
	}

	namespace SdDesignTokenIds
	{
		inline constexpr SdDesignTokenId Text = "text"_SdId;
		inline constexpr SdDesignTokenId Background = "background"_SdId;
		inline constexpr SdDesignTokenId WindowBg = "window.bg"_SdId;
		inline constexpr SdDesignTokenId PanelBg = "panel.bg"_SdId;
		inline constexpr SdDesignTokenId ButtonBg = "button.bg"_SdId;
		inline constexpr SdDesignTokenId ButtonBgHover = "button.bg.hover"_SdId;
		inline constexpr SdDesignTokenId ButtonBgPressed = "button.bg.pressed"_SdId;
		inline constexpr SdDesignTokenId ButtonText = "button.text"_SdId;
		inline constexpr SdDesignTokenId Accent = "accent"_SdId;
		inline constexpr SdDesignTokenId Border = "border"_SdId;
		inline constexpr SdDesignTokenId BorderStrong = "border.strong"_SdId;
		inline constexpr SdDesignTokenId Danger = "danger"_SdId;
		inline constexpr SdDesignTokenId Selection = "selection"_SdId;
		inline constexpr SdDesignTokenId SpacingSmall = "spacing.small"_SdId;
		inline constexpr SdDesignTokenId SpacingMedium = "spacing.medium"_SdId;
		inline constexpr SdDesignTokenId FontButton = "font.button"_SdId;
		inline constexpr SdDesignTokenId RadiusSmall = "radius.small"_SdId;
		inline constexpr SdDesignTokenId DurationFast = "duration.fast"_SdId;
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
		SdDesignTokenId lengthVariableId = 0;
		SdDesignTokenId variableId = 0;

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

		static constexpr SdStyleValue FromLength(SdUInt8 unit, float value, SdDesignTokenId variableId = 0) noexcept
		{
			SdStyleValue result = {};
			result.kind = SdStyleValueKind::Length;
			result.lengthUnit = unit;
			result.lengthValue = value;
			result.lengthVariableId = variableId;
			return result;
		}

		static constexpr SdStyleValue FromColorVariable(SdDesignTokenId value) noexcept
		{
			SdStyleValue result = {};
			result.kind = SdStyleValueKind::ColorVariable;
			result.variableId = value;
			return result;
		}

		static constexpr SdStyleValue FromMetricVariable(SdDesignTokenId value) noexcept
		{
			SdStyleValue result = {};
			result.kind = SdStyleValueKind::MetricVariable;
			result.variableId = value;
			return result;
		}
	};

	inline constexpr SdStyleValue DesignColor(SdDesignTokenId id) noexcept
	{
		return SdStyleValue::FromColorVariable(id);
	}

	inline constexpr SdStyleValue DesignMetric(SdDesignTokenId id) noexcept
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
