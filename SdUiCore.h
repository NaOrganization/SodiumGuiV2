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

	inline constexpr SdRootLayer SdRootLayerFromPriority(SdLayerPriority layerPriority) noexcept
	{
		switch (layerPriority)
		{
		case SdLayerPriority::Background:
			return SdRootLayer::Background;
		case SdLayerPriority::Content:
			return SdRootLayer::Content;
		case SdLayerPriority::Floating:
			return SdRootLayer::Floating;
		case SdLayerPriority::Popup:
			return SdRootLayer::Popup;
		case SdLayerPriority::Overlay:
			return SdRootLayer::Tooltip;
		case SdLayerPriority::Debug:
			return SdRootLayer::DebugOverlay;
		default:
			return SdRootLayer::Content;
		}
	}

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

	inline constexpr SdStyleId SdStyleIdLiteral(const char* text) noexcept
	{
		SdStyleId hash = 14695981039346656037ull;
		while (*text != '\0')
		{
			hash ^= static_cast<SdUInt8>(*text);
			hash *= 1099511628211ull;
			++text;
		}
		return hash;
	}

	inline constexpr SdStyleClassId SdStyleClassLiteral(const char* text) noexcept
	{
		return SdStyleIdLiteral(text);
	}

	inline constexpr SdStyleScopeId SdStyleScopeLiteral(const char* text) noexcept
	{
		return SdStyleIdLiteral(text);
	}

	inline constexpr SdThemeVariableId SdThemeVariableLiteral(const char* text) noexcept
	{
		return SdStyleIdLiteral(text);
	}

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
		inline constexpr SdStyleId Global = SdStyleIdLiteral("Sodium.Style.Target.Global");
		inline constexpr SdStyleId Default = SdStyleIdLiteral("Sodium.Style.Target.Default");
		inline constexpr SdStyleId Text = SdStyleIdLiteral("Sodium.Style.Target.Text");
		inline constexpr SdStyleId Panel = SdStyleIdLiteral("Sodium.Style.Target.Panel");
		inline constexpr SdStyleId Button = SdStyleIdLiteral("Sodium.Style.Target.Button");
		inline constexpr SdStyleId CheckBox = SdStyleIdLiteral("Sodium.Style.Target.CheckBox");
		inline constexpr SdStyleId Window = SdStyleIdLiteral("Sodium.Style.Target.Window");
		inline constexpr SdStyleId ImageViewer = SdStyleIdLiteral("Sodium.Style.Target.ImageViewer");
		inline constexpr SdStyleId Slider = SdStyleIdLiteral("Sodium.Style.Target.Slider");
		inline constexpr SdStyleId TextInput = SdStyleIdLiteral("Sodium.Style.Target.TextInput");
		inline constexpr SdStyleId ScrollView = SdStyleIdLiteral("Sodium.Style.Target.ScrollView");
		inline constexpr SdStyleId Popup = SdStyleIdLiteral("Sodium.Style.Target.Popup");
		inline constexpr SdStyleId ContextMenu = SdStyleIdLiteral("Sodium.Style.Target.ContextMenu");
		inline constexpr SdStyleId Tooltip = SdStyleIdLiteral("Sodium.Style.Target.Tooltip");
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

	inline constexpr SdStyleValue ThemeColor(const char* name) noexcept
	{
		return SdStyleValue::FromColorVariable(SdThemeVariableLiteral(name));
	}

	inline constexpr SdStyleValue ThemeMetric(const char* name) noexcept
	{
		return SdStyleValue::FromMetricVariable(SdThemeVariableLiteral(name));
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
