#pragma once

#include "Widget/SdUiCore.h"

#include <type_traits>
#include <vector>

namespace Sodium
{
	inline constexpr SdStyleNodeId SdInvalidStyleNodeId = SdInvalidIndex<SdStyleNodeId>;

	enum class SdLengthUnit : SdUInt8
	{
		Auto,
		Pixels,
		Percent,
		Em,
		Rem,
		Variable
	};

	struct SdLength final
	{
		SdLengthUnit unit = SdLengthUnit::Auto;
		float value = 0.0f;
		SdDesignTokenId variableId = 0;

		static constexpr SdLength Auto() noexcept
		{
			return {};
		}

		static constexpr SdLength Pixels(float value) noexcept
		{
			SdLength result = {};
			result.unit = SdLengthUnit::Pixels;
			result.value = value;
			return result;
		}

		static constexpr SdLength Percent(float value) noexcept
		{
			SdLength result = {};
			result.unit = SdLengthUnit::Percent;
			result.value = value;
			return result;
		}

		static constexpr SdLength Em(float value) noexcept
		{
			SdLength result = {};
			result.unit = SdLengthUnit::Em;
			result.value = value;
			return result;
		}

		static constexpr SdLength Rem(float value) noexcept
		{
			SdLength result = {};
			result.unit = SdLengthUnit::Rem;
			result.value = value;
			return result;
		}

		static constexpr SdLength Variable(SdDesignTokenId variableId) noexcept
		{
			SdLength result = {};
			result.unit = SdLengthUnit::Variable;
			result.variableId = variableId;
			return result;
		}

		friend constexpr bool operator==(const SdLength&, const SdLength&) = default;
	};

	struct SdBoxEdges final
	{
		SdLength left = SdLength::Pixels(0.0f);
		SdLength top = SdLength::Pixels(0.0f);
		SdLength right = SdLength::Pixels(0.0f);
		SdLength bottom = SdLength::Pixels(0.0f);

		static constexpr SdBoxEdges All(SdLength value) noexcept
		{
			return { value, value, value, value };
		}

		static constexpr SdBoxEdges FromSpacing(SdSpacing value) noexcept
		{
			return {
				SdLength::Pixels(value.left),
				SdLength::Pixels(value.top),
				SdLength::Pixels(value.right),
				SdLength::Pixels(value.bottom)
			};
		}

		friend constexpr bool operator==(const SdBoxEdges&, const SdBoxEdges&) = default;
	};

	enum class SdDisplay : SdUInt8
	{
		None,
		Block,
		InlineBlock,
		Flex
	};

	enum class SdPosition : SdUInt8
	{
		Static,
		Relative,
		Absolute
	};

	enum class SdOverflow : SdUInt8
	{
		Visible,
		Hidden,
		Clip,
		Scroll,
		Auto
	};

	enum class SdBoxSizing : SdUInt8
	{
		ContentBox,
		BorderBox
	};

	enum class SdFlexDirection : SdUInt8
	{
		Row,
		Column
	};

	enum class SdJustifyContent : SdUInt8
	{
		FlexStart,
		Center,
		FlexEnd,
		SpaceBetween
	};

	enum class SdAlignItems : SdUInt8
	{
		Stretch,
		FlexStart,
		Center,
		FlexEnd
	};

	struct SdBorderSide final
	{
		SdLength width = SdLength::Pixels(0.0f);
		SdColor color = SdColorTransparent;

		friend constexpr bool operator==(const SdBorderSide&, const SdBorderSide&) = default;
	};

	struct SdBorder final
	{
		SdBorderSide left = {};
		SdBorderSide top = {};
		SdBorderSide right = {};
		SdBorderSide bottom = {};

		static constexpr SdBorder All(SdLength width, SdColor color) noexcept
		{
			const SdBorderSide side{ width, color };
			return { side, side, side, side };
		}

		friend constexpr bool operator==(const SdBorder&, const SdBorder&) = default;
	};

	struct SdTransform final
	{
		SdVec2 translate = {};
		SdVec2 scale = { 1.0f, 1.0f };
		float rotateRadians = 0.0f;

		friend constexpr bool operator==(const SdTransform&, const SdTransform&) = default;
	};

	struct SdStyleTransition final
	{
		SdPropertyId propertyId = 0;
		SdDuration duration = 160ms;
		SdDuration delay = {};
		SdAnimationEasing easing = SdAnimationEasing::OutCubic;
		SdTransitionBehavior behavior = SdTransitionBehavior::Normal;
		bool allowExpensiveLayout = false;

		friend constexpr bool operator==(const SdStyleTransition&, const SdStyleTransition&) = default;
	};

	struct SdTransitionList final
	{
		std::vector<SdStyleTransition> entries = {};

		friend bool operator==(const SdTransitionList&, const SdTransitionList&) = default;
	};

	struct SdBoxStyle
	{
		SdDisplay display = SdDisplay::Block;
		SdPosition position = SdPosition::Static;
		SdInt32 zIndex = 0;
		SdOverflow overflowX = SdOverflow::Visible;
		SdOverflow overflowY = SdOverflow::Visible;
		SdBoxSizing boxSizing = SdBoxSizing::ContentBox;
		SdLength width = SdLength::Auto();
		SdLength height = SdLength::Auto();
		SdLength minWidth = SdLength::Auto();
		SdLength minHeight = SdLength::Auto();
		SdLength maxWidth = SdLength::Auto();
		SdLength maxHeight = SdLength::Auto();
		SdBoxEdges margin = {};
		SdBoxEdges padding = {};
		SdBorder border = {};
		SdLength radius = SdLength::Pixels(0.0f);
		SdColor color = SdColorWhite;
		SdColor backgroundColor = SdColorTransparent;
		SdDesignTokenId colorVariable = 0;
		SdDesignTokenId backgroundColorVariable = 0;
		SdDesignTokenId borderColorVariable = 0;
		float opacity = 1.0f;
		float fontSize = 16.0f;
		float lineHeight = 0.0f;
		SdDesignTokenId fontSizeVariable = 0;
		SdDesignTokenId lineHeightVariable = 0;
		SdFlexDirection flexDirection = SdFlexDirection::Column;
		SdJustifyContent justifyContent = SdJustifyContent::FlexStart;
		SdAlignItems alignItems = SdAlignItems::Stretch;
		SdLength gap = SdLength::Pixels(0.0f);
		float flexGrow = 0.0f;
		float flexShrink = 1.0f;
		SdLength flexBasis = SdLength::Auto();
		SdTransform transform = {};
		SdTransitionList transitions = {};

		friend bool operator==(const SdBoxStyle&, const SdBoxStyle&) = default;
	};

	struct SdWidgetRootStyle : SdBoxStyle
	{
		friend bool operator==(const SdWidgetRootStyle&, const SdWidgetRootStyle&) = default;
	};

	struct SdWidgetPartStyle : SdBoxStyle
	{
		bool inheritText = true;

		friend bool operator==(const SdWidgetPartStyle&, const SdWidgetPartStyle&) = default;
	};

	struct SdStylePart final : SdId<SdStylePartTag>
	{
		constexpr SdStylePart() noexcept = default;
		constexpr SdStylePart(SdUInt64 value) noexcept
			: SdId<SdStylePartTag>(value)
		{
		}

		static constexpr SdStylePart Root() noexcept
		{
			return {};
		}

		static constexpr SdStylePart Make(SdUInt64 value) noexcept
		{
			return { value };
		}

		constexpr bool IsRoot() const noexcept
		{
			return value == 0;
		}

		friend constexpr bool operator==(const SdStylePart&, const SdStylePart&) = default;
		friend constexpr bool operator!=(const SdStylePart&, const SdStylePart&) = default;
	};

	namespace SdWidgetPartIds
	{
		namespace Button
		{
			inline constexpr SdStylePart Content = SdStylePart::Make("Sodium.Button.Part.Content"_SdId);
			inline constexpr SdStylePart Icon = SdStylePart::Make("Sodium.Button.Part.Icon"_SdId);
			inline constexpr SdStylePart Label = SdStylePart::Make("Sodium.Button.Part.Label"_SdId);
		}

		namespace CheckBox
		{
			inline constexpr SdStylePart Box = SdStylePart::Make("Sodium.CheckBox.Part.Box"_SdId);
			inline constexpr SdStylePart Indicator = SdStylePart::Make("Sodium.CheckBox.Part.Indicator"_SdId);
			inline constexpr SdStylePart Label = SdStylePart::Make("Sodium.CheckBox.Part.Label"_SdId);
		}

		namespace Slider
		{
			inline constexpr SdStylePart Label = SdStylePart::Make("Sodium.Slider.Part.Label"_SdId);
			inline constexpr SdStylePart Track = SdStylePart::Make("Sodium.Slider.Part.Track"_SdId);
			inline constexpr SdStylePart Fill = SdStylePart::Make("Sodium.Slider.Part.Fill"_SdId);
			inline constexpr SdStylePart Thumb = SdStylePart::Make("Sodium.Slider.Part.Thumb"_SdId);
		}

		namespace TextInput
		{
			inline constexpr SdStylePart Field = SdStylePart::Make("Sodium.TextInput.Part.Field"_SdId);
			inline constexpr SdStylePart Value = SdStylePart::Make("Sodium.TextInput.Part.Value"_SdId);
			inline constexpr SdStylePart Placeholder = SdStylePart::Make("Sodium.TextInput.Part.Placeholder"_SdId);
			inline constexpr SdStylePart Selection = SdStylePart::Make("Sodium.TextInput.Part.Selection"_SdId);
			inline constexpr SdStylePart Caret = SdStylePart::Make("Sodium.TextInput.Part.Caret"_SdId);
		}

		namespace Window
		{
			inline constexpr SdStylePart Titlebar = SdStylePart::Make("Sodium.Window.Part.Titlebar"_SdId);
			inline constexpr SdStylePart Title = SdStylePart::Make("Sodium.Window.Part.Title"_SdId);
			inline constexpr SdStylePart CloseButton = SdStylePart::Make("Sodium.Window.Part.CloseButton"_SdId);
			inline constexpr SdStylePart Content = SdStylePart::Make("Sodium.Window.Part.Content"_SdId);
			inline constexpr SdStylePart ResizeHandle = SdStylePart::Make("Sodium.Window.Part.ResizeHandle"_SdId);
		}

		namespace ScrollView
		{
			inline constexpr SdStylePart Scrollbar = SdStylePart::Make("Sodium.ScrollView.Part.Scrollbar"_SdId);
			inline constexpr SdStylePart Thumb = SdStylePart::Make("Sodium.ScrollView.Part.Thumb"_SdId);
		}
	}

	enum class SdPseudoStateFlag : SdUInt64
	{
		None = 0,
		Hovered = 1ull << 0,
		Active = 1ull << 1,
		Pressed = Active,
		Focused = 1ull << 2,
		Disabled = 1ull << 3,
		Checked = 1ull << 4,
		Placeholder = 1ull << 5
	};

	struct SdPseudoState final
	{
		SdUInt64 bits = 0;

		constexpr bool Has(SdPseudoStateFlag flag) const noexcept
		{
			return (bits & static_cast<SdUInt64>(flag)) != 0;
		}

		constexpr void Set(SdPseudoStateFlag flag, bool enabled = true) noexcept
		{
			if (enabled)
				bits |= static_cast<SdUInt64>(flag);
			else
				bits &= ~static_cast<SdUInt64>(flag);
		}

		static constexpr SdPseudoState FromInteraction(SdStyleInteractionState interactionState) noexcept
		{
			SdPseudoState result = {};
			switch (interactionState)
			{
			case SdStyleInteractionState::Hovered:
				result.Set(SdPseudoStateFlag::Hovered);
				break;
			case SdStyleInteractionState::Pressed:
				result.Set(SdPseudoStateFlag::Hovered);
				result.Set(SdPseudoStateFlag::Active);
				break;
			case SdStyleInteractionState::Focused:
				result.Set(SdPseudoStateFlag::Focused);
				break;
			case SdStyleInteractionState::Normal:
			default:
				break;
			}
			return result;
		}
	};

	enum class SdStyleNodeKind : SdUInt8
	{
		Root,
		Part
	};

	enum class SdStyleValuePhase : SdUInt8
	{
		Specified,
		Resolved,
		Used,
		Presentation
	};

	struct SdUsedBox final
	{
		SdRect marginBox = {};
		SdRect borderBox = {};
		SdRect paddingBox = {};
		SdRect contentBox = {};
	};

	struct SdStyleNode final
	{
		SdStyleNodeId styleNodeId = SdInvalidStyleNodeId;
		SdWidgetId widgetId = 0;
		SdStyleNodeId parentStyleNodeId = SdInvalidStyleNodeId;
		SdStylePart part = SdStylePart::Root();
		SdStyleNodeKind kind = SdStyleNodeKind::Root;
		SdStyleScopeId scopeId = 0;
		SdPseudoState pseudoState = {};
		SdUInt64 revision = 1;
		SdBoxStyle specifiedStyle = {};
		SdBoxStyle resolvedStyle = {};
		SdBoxStyle presentationStyle = {};
		SdUsedBox usedBox = {};
		SdUsedBox layoutBox = {};
	};

	inline SdStyleValue SdStyleValueFromLength(SdLength value) noexcept
	{
		return SdStyleValue::FromLength(static_cast<SdUInt8>(value.unit), value.value, value.variableId);
	}

	inline SdLength SdStyleValueToLength(const SdStyleValue& value) noexcept
	{
		if (value.kind == SdStyleValueKind::MetricVariable)
			return SdLength::Variable(value.variableId);

		if (value.kind == SdStyleValueKind::Float)
			return SdLength::Pixels(value.number);

		if (value.kind != SdStyleValueKind::Length)
			return {};

		SdLength result = {};
		result.unit = static_cast<SdLengthUnit>(value.lengthUnit);
		result.value = value.lengthValue;
		result.variableId = value.lengthVariableId;
		return result;
	}

	template<class TEnum>
		requires std::is_enum_v<TEnum>
	SdStyleValue SdStyleValueFromEnum(TEnum value) noexcept
	{
		return SdStyleValue::FromFloat(static_cast<float>(static_cast<std::underlying_type_t<TEnum>>(value)));
	}

	template<class TEnum>
		requires std::is_enum_v<TEnum>
	bool SdStyleValueToEnum(const SdStyleValue& value, TEnum& outValue) noexcept
	{
		if (value.kind != SdStyleValueKind::Float)
			return false;
		outValue = static_cast<TEnum>(static_cast<std::underlying_type_t<TEnum>>(value.number));
		return true;
	}
}
