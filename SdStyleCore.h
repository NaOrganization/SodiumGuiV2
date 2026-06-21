#pragma once

#include "SdUiCore.h"

#include <vector>

namespace Sodium
{
	using SdStyleNodeId = SdUInt32;

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
		SdThemeVariableId variableId = 0;

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

		static constexpr SdLength Variable(SdThemeVariableId variableId) noexcept
		{
			SdLength result = {};
			result.unit = SdLengthUnit::Variable;
			result.variableId = variableId;
			return result;
		}
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
	};

	struct SdTransform final
	{
		SdVec2 translate = {};
		SdVec2 scale = { 1.0f, 1.0f };
		float rotateRadians = 0.0f;
	};

	enum class SdTransitionBehavior : SdUInt8
	{
		Normal,
		AllowDiscrete
	};

	struct SdStyleTransition final
	{
		SdPropertyId propertyId = 0;
		SdDuration duration = 160ms;
		SdDuration delay = {};
		SdAnimationEasing easing = SdAnimationEasing::OutCubic;
		SdTransitionBehavior behavior = SdTransitionBehavior::Normal;
		bool allowExpensiveLayout = false;
	};

	struct SdTransitionList final
	{
		std::vector<SdStyleTransition> entries = {};
	};

	struct SdBoxStyle
	{
		SdDisplay display = SdDisplay::Block;
		SdPosition position = SdPosition::Static;
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
		float opacity = 1.0f;
		SdFlexDirection flexDirection = SdFlexDirection::Column;
		SdJustifyContent justifyContent = SdJustifyContent::FlexStart;
		SdAlignItems alignItems = SdAlignItems::Stretch;
		SdLength gap = SdLength::Pixels(0.0f);
		float flexGrow = 0.0f;
		float flexShrink = 1.0f;
		SdLength flexBasis = SdLength::Auto();
		SdTransform transform = {};
		SdTransitionList transitions = {};
	};

	struct SdWidgetRootStyle : SdBoxStyle
	{
	};

	struct SdWidgetPartStyle : SdBoxStyle
	{
		bool inheritText = true;
	};

	struct SdStylePart final
	{
		SdUInt64 value = 0;

		static constexpr SdStylePart Root() noexcept
		{
			return {};
		}

		static constexpr SdStylePart Make(SdUInt64 value) noexcept
		{
			return { value };
		}

		static constexpr SdStylePart Make(const char* text) noexcept
		{
			return { SdStyleIdLiteral(text) };
		}

		constexpr bool IsRoot() const noexcept
		{
			return value == 0;
		}

		friend constexpr bool operator==(const SdStylePart&, const SdStylePart&) = default;
	};

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
	};
}
