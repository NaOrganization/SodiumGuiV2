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
		ColorBorder,
		ColorBorderStrong,
		ColorDanger,
		ColorSelection,
		SpacingSmall,
		SpacingMedium,
		RadiusSmall,
		DurationFast,
		Count
	};

	using SdStyleTokenTag = SdUInt64;
	using SdStyleClassId = SdUInt64;
	using SdStyleScopeId = SdUInt64;

	inline constexpr SdStyleTokenTag SdStyleTokenTagLiteral(const char* text) noexcept
	{
		SdStyleTokenTag hash = 14695981039346656037ull;
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
		return SdStyleTokenTagLiteral(text);
	}

	inline constexpr SdStyleScopeId SdStyleScopeLiteral(const char* text) noexcept
	{
		return SdStyleTokenTagLiteral(text);
	}

	struct SdStyleIdentity final
	{
		SdSpan<const SdStyleClassId> classes = {};
		SdStyleScopeId scope = 0;
	};

	namespace SdStyleTargetTags
	{
		inline constexpr SdStyleTokenTag Global = SdStyleTokenTagLiteral("Sodium.Style.Target.Global");
		inline constexpr SdStyleTokenTag Default = SdStyleTokenTagLiteral("Sodium.Style.Target.Default");
		inline constexpr SdStyleTokenTag Text = SdStyleTokenTagLiteral("Sodium.Style.Target.Text");
		inline constexpr SdStyleTokenTag Panel = SdStyleTokenTagLiteral("Sodium.Style.Target.Panel");
		inline constexpr SdStyleTokenTag Button = SdStyleTokenTagLiteral("Sodium.Style.Target.Button");
		inline constexpr SdStyleTokenTag CheckBox = SdStyleTokenTagLiteral("Sodium.Style.Target.CheckBox");
		inline constexpr SdStyleTokenTag Window = SdStyleTokenTagLiteral("Sodium.Style.Target.Window");
		inline constexpr SdStyleTokenTag ImageViewer = SdStyleTokenTagLiteral("Sodium.Style.Target.ImageViewer");
		inline constexpr SdStyleTokenTag Slider = SdStyleTokenTagLiteral("Sodium.Style.Target.Slider");
		inline constexpr SdStyleTokenTag TextInput = SdStyleTokenTagLiteral("Sodium.Style.Target.TextInput");
		inline constexpr SdStyleTokenTag ScrollView = SdStyleTokenTagLiteral("Sodium.Style.Target.ScrollView");
		inline constexpr SdStyleTokenTag Popup = SdStyleTokenTagLiteral("Sodium.Style.Target.Popup");
		inline constexpr SdStyleTokenTag ContextMenu = SdStyleTokenTagLiteral("Sodium.Style.Target.ContextMenu");
		inline constexpr SdStyleTokenTag Tooltip = SdStyleTokenTagLiteral("Sodium.Style.Target.Tooltip");
	}

	namespace SdStylePropertyTags
	{
		inline constexpr SdStyleTokenTag Color = SdStyleTokenTagLiteral("Sodium.Style.Property.Color");
		inline constexpr SdStyleTokenTag Background = SdStyleTokenTagLiteral("Sodium.Style.Property.Background");
		inline constexpr SdStyleTokenTag Border = SdStyleTokenTagLiteral("Sodium.Style.Property.Border");
		inline constexpr SdStyleTokenTag Padding = SdStyleTokenTagLiteral("Sodium.Style.Property.Padding");
		inline constexpr SdStyleTokenTag Width = SdStyleTokenTagLiteral("Sodium.Style.Property.Width");
		inline constexpr SdStyleTokenTag Height = SdStyleTokenTagLiteral("Sodium.Style.Property.Height");
		inline constexpr SdStyleTokenTag Radius = SdStyleTokenTagLiteral("Sodium.Style.Property.Radius");
		inline constexpr SdStyleTokenTag Opacity = SdStyleTokenTagLiteral("Sodium.Style.Property.Opacity");
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
		ColorToken,
		MetricToken
	};

	struct SdStyleValue final
	{
		SdStyleValueKind kind = SdStyleValueKind::None;
		SdColor color = SdColorTransparent;
		float number = 0.0f;
		SdSpacing spacing = {};
		SdVec2 vec2 = {};
		SdStyleToken token = SdStyleToken::Count;

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

		static constexpr SdStyleValue FromColorToken(SdStyleToken value) noexcept
		{
			SdStyleValue result = {};
			result.kind = SdStyleValueKind::ColorToken;
			result.token = value;
			return result;
		}

		static constexpr SdStyleValue FromMetricToken(SdStyleToken value) noexcept
		{
			SdStyleValue result = {};
			result.kind = SdStyleValueKind::MetricToken;
			result.token = value;
			return result;
		}
	};

	struct SdStyleDeclaration final
	{
		SdStyleTokenTag propertyTag = 0;
		SdStyleValue value = {};
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
		std::vector<SdStyleDeclaration> properties = {};

		void SetValue(SdStyleTokenTag propertyTag, SdStyleValue value)
		{
			for (SdStyleDeclaration& declaration : properties)
			{
				if (declaration.propertyTag == propertyTag)
				{
					declaration.value = value;
					ApplyKnownProperty(propertyTag, value);
					return;
				}
			}

			properties.push_back({ propertyTag, value });
			ApplyKnownProperty(propertyTag, value);
		}

		const SdStyleValue* FindValue(SdStyleTokenTag propertyTag) const noexcept
		{
			for (const SdStyleDeclaration& declaration : properties)
			{
				if (declaration.propertyTag == propertyTag)
					return &declaration.value;
			}
			return nullptr;
		}

		SdColor GetColor(SdStyleTokenTag propertyTag, SdColor fallback = SdColorTransparent) const noexcept
		{
			const SdStyleValue* value = FindValue(propertyTag);
			if (!value || value->kind != SdStyleValueKind::Color)
				return fallback;
			return value->color;
		}

		float GetFloat(SdStyleTokenTag propertyTag, float fallback = 0.0f) const noexcept
		{
			const SdStyleValue* value = FindValue(propertyTag);
			if (!value || value->kind != SdStyleValueKind::Float)
				return fallback;
			return value->number;
		}

		SdSpacing GetSpacing(SdStyleTokenTag propertyTag, SdSpacing fallback = {}) const noexcept
		{
			const SdStyleValue* value = FindValue(propertyTag);
			if (!value || value->kind != SdStyleValueKind::Spacing)
				return fallback;
			return value->spacing;
		}

		SdVec2 GetVec2(SdStyleTokenTag propertyTag, SdVec2 fallback = {}) const noexcept
		{
			const SdStyleValue* value = FindValue(propertyTag);
			if (!value || value->kind != SdStyleValueKind::Vec2)
				return fallback;
			return value->vec2;
		}

	private:
		void ApplyKnownProperty(SdStyleTokenTag propertyTag, const SdStyleValue& value) noexcept
		{
			if (value.kind == SdStyleValueKind::Color)
			{
				if (propertyTag == SdStylePropertyTags::Color)
					color = value.color;
				else if (propertyTag == SdStylePropertyTags::Background)
					background = value.color;
				else if (propertyTag == SdStylePropertyTags::Border)
					border = value.color;
				return;
			}

			if (value.kind == SdStyleValueKind::Float)
			{
				if (propertyTag == SdStylePropertyTags::Width)
					width = value.number;
				else if (propertyTag == SdStylePropertyTags::Height)
					height = value.number;
				else if (propertyTag == SdStylePropertyTags::Radius)
					radius = value.number;
				else if (propertyTag == SdStylePropertyTags::Opacity)
					opacity = value.number;
				return;
			}

			if (value.kind == SdStyleValueKind::Spacing && propertyTag == SdStylePropertyTags::Padding)
				padding = value.spacing;
		}
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
		SdDuration duration = 160ms;
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
