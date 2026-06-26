#pragma once

#include "Style/SdStyleCore.h"

#include <algorithm>
#include <unordered_map>
#include <vector>

namespace Sodium
{
	struct SdResolvedBoxEdges final
	{
		float left = 0.0f;
		float top = 0.0f;
		float right = 0.0f;
		float bottom = 0.0f;
	};

	struct SdResolvedBoxStyle final
	{
		SdDisplay display = SdDisplay::Block;
		SdPosition position = SdPosition::Static;
		SdOverflow overflowX = SdOverflow::Visible;
		SdOverflow overflowY = SdOverflow::Visible;
		SdBoxSizing boxSizing = SdBoxSizing::ContentBox;
		float width = 0.0f;
		float height = 0.0f;
		float minWidth = 0.0f;
		float minHeight = 0.0f;
		float maxWidth = 0.0f;
		float maxHeight = 0.0f;
		SdResolvedBoxEdges inset = {};
		SdResolvedBoxEdges margin = {};
		SdResolvedBoxEdges padding = {};
		SdResolvedBoxEdges border = {};
		SdFlexDirection flexDirection = SdFlexDirection::Column;
		SdJustifyContent justifyContent = SdJustifyContent::FlexStart;
		SdAlignItems alignItems = SdAlignItems::Stretch;
		float gap = 0.0f;
		float flexGrow = 0.0f;
		float flexShrink = 1.0f;
		float flexBasis = 0.0f;
		SdFloat floatMode = SdFloat::None;
		SdInt32 gridColumnCount = 0;
		SdInt32 gridRowCount = 0;
		float gridAutoColumns = 0.0f;
		float gridAutoRows = 0.0f;
		float gridColumnGap = 0.0f;
		float gridRowGap = 0.0f;
		SdInt32 gridColumn = 0;
		SdInt32 gridRow = 0;
		SdInt32 gridColumnSpan = 1;
		SdInt32 gridRowSpan = 1;
	};

	struct SdBoxNode final
	{
		SdStyleNodeId styleNodeId = SdInvalidStyleNodeId;
		SdUInt32 parentBoxIndex = SdInvalidIndex<SdUInt32>;
		SdUInt32 firstChildIndex = SdInvalidIndex<SdUInt32>;
		SdUInt32 nextSiblingIndex = SdInvalidIndex<SdUInt32>;
		SdDisplay display = SdDisplay::Block;
		SdPosition position = SdPosition::Static;
		SdRect marginBox = {};
		SdRect borderBox = {};
		SdRect paddingBox = {};
		SdRect contentBox = {};
		SdRect explicitBorderBox = {};
		SdVec2 intrinsicSize = {};
		SdVec2 measuredContentSize = {};
		SdVec2 measuredBorderSize = {};
		SdResolvedBoxStyle usedStyleValues = {};
		SdBoxStyle style = {};
	};

	inline float SdResolveLength(SdLength length, float basis, float fallback = 0.0f) noexcept
	{
		switch (length.unit)
		{
		case SdLengthUnit::Pixels:
			return length.value;
		case SdLengthUnit::Percent:
			return basis * length.value * 0.01f;
		case SdLengthUnit::Em:
		case SdLengthUnit::Rem:
			return length.value * 16.0f;
		case SdLengthUnit::Auto:
		case SdLengthUnit::Variable:
		default:
			return fallback;
		}
	}

	inline bool SdIsExplicitLength(SdLength length) noexcept
	{
		return length.unit != SdLengthUnit::Auto;
	}

	inline SdResolvedBoxEdges SdResolveBoxEdges(SdBoxEdges edges, SdVec2 basis) noexcept
	{
		return {
			SdResolveLength(edges.left, basis.x),
			SdResolveLength(edges.top, basis.y),
			SdResolveLength(edges.right, basis.x),
			SdResolveLength(edges.bottom, basis.y)
		};
	}

	inline SdResolvedBoxEdges SdResolveBorderEdges(SdBorder border, SdVec2 basis) noexcept
	{
		return {
			SdResolveLength(border.left.width, basis.x),
			SdResolveLength(border.top.width, basis.y),
			SdResolveLength(border.right.width, basis.x),
			SdResolveLength(border.bottom.width, basis.y)
		};
	}

	inline SdRect SdInsetRect(SdRect rect, SdResolvedBoxEdges edges) noexcept
	{
		rect.min.x += edges.left;
		rect.min.y += edges.top;
		rect.max.x -= edges.right;
		rect.max.y -= edges.bottom;
		if (rect.max.x < rect.min.x)
			rect.max.x = rect.min.x;
		if (rect.max.y < rect.min.y)
			rect.max.y = rect.min.y;
		return rect;
	}

	inline SdRect SdOutsetRect(SdRect rect, SdResolvedBoxEdges edges) noexcept
	{
		rect.min.x -= edges.left;
		rect.min.y -= edges.top;
		rect.max.x += edges.right;
		rect.max.y += edges.bottom;
		return rect;
	}

	inline SdResolvedBoxStyle SdResolveBoxStyle(const SdBoxStyle& style, SdVec2 basis, SdVec2 intrinsicSize) noexcept
	{
		SdResolvedBoxStyle result = {};
		result.display = style.display;
		result.position = style.position;
		result.overflowX = style.overflowX;
		result.overflowY = style.overflowY;
		result.boxSizing = style.boxSizing;
		result.inset = SdResolveBoxEdges(style.inset, basis);
		result.margin = SdResolveBoxEdges(style.margin, basis);
		result.padding = SdResolveBoxEdges(style.padding, basis);
		result.border = SdResolveBorderEdges(style.border, basis);
		result.width = SdResolveLength(style.width, basis.x, intrinsicSize.x);
		result.height = SdResolveLength(style.height, basis.y, intrinsicSize.y);
		result.minWidth = SdResolveLength(style.minWidth, basis.x, 0.0f);
		result.minHeight = SdResolveLength(style.minHeight, basis.y, 0.0f);
		result.maxWidth = SdResolveLength(style.maxWidth, basis.x, 0.0f);
		result.maxHeight = SdResolveLength(style.maxHeight, basis.y, 0.0f);
		result.flexDirection = style.flexDirection;
		result.justifyContent = style.justifyContent;
		result.alignItems = style.alignItems;
		result.gap = SdResolveLength(style.gap, basis.x, 0.0f);
		result.flexGrow = style.flexGrow;
		result.flexShrink = style.flexShrink;
		result.flexBasis = SdResolveLength(style.flexBasis, basis.x, 0.0f);
		result.floatMode = style.floatMode;
		result.gridColumnCount = style.gridColumnCount;
		result.gridRowCount = style.gridRowCount;
		result.gridAutoColumns = SdResolveLength(style.gridAutoColumns, basis.x, 0.0f);
		result.gridAutoRows = SdResolveLength(style.gridAutoRows, basis.y, 0.0f);
		result.gridColumnGap = SdResolveLength(style.gridColumnGap, basis.x, result.gap);
		result.gridRowGap = SdResolveLength(style.gridRowGap, basis.y, result.gap);
		result.gridColumn = style.gridColumn;
		result.gridRow = style.gridRow;
		result.gridColumnSpan = std::max<SdInt32>(1, style.gridColumnSpan);
		result.gridRowSpan = std::max<SdInt32>(1, style.gridRowSpan);
		result.width = std::max(result.minWidth, result.width);
		result.height = std::max(result.minHeight, result.height);
		if (result.maxWidth > 0.0f)
			result.width = std::min(result.width, result.maxWidth);
		if (result.maxHeight > 0.0f)
			result.height = std::min(result.height, result.maxHeight);
		return result;
	}

	inline float SdHorizontalBoxExtras(const SdResolvedBoxStyle& style) noexcept
	{
		return style.padding.left + style.padding.right + style.border.left + style.border.right;
	}

	inline float SdVerticalBoxExtras(const SdResolvedBoxStyle& style) noexcept
	{
		return style.padding.top + style.padding.bottom + style.border.top + style.border.bottom;
	}

	inline float SdClampResolvedAxisSize(float resolvedSize, float minimum, float maximum) noexcept
	{
		resolvedSize = std::max(minimum, resolvedSize);
		if (maximum > 0.0f)
			resolvedSize = std::min(resolvedSize, maximum);
		return std::max(0.0f, resolvedSize);
	}

	inline float SdResolveBorderBoxAxisSize(float resolvedSize, float extras, bool sizeUsesBorderBox) noexcept
	{
		return std::max(0.0f, sizeUsesBorderBox ? resolvedSize : resolvedSize + extras);
	}

	enum class SdAutoSizeInterpretation : SdUInt8
	{
		BorderBox,
		ContentBox
	};

	inline float SdResolveBorderBoxWidth(
		const SdBoxStyle& sourceStyle,
		const SdResolvedBoxStyle& usedStyle,
		float autoSize = 0.0f,
		SdAutoSizeInterpretation autoInterpretation = SdAutoSizeInterpretation::BorderBox) noexcept
	{
		const bool explicitWidth = SdIsExplicitLength(sourceStyle.width);
		float resolvedSize = explicitWidth || autoSize <= 0.0f ? usedStyle.width : autoSize;
		resolvedSize = SdClampResolvedAxisSize(resolvedSize, usedStyle.minWidth, usedStyle.maxWidth);
		const bool sizeUsesBorderBox = explicitWidth
			? usedStyle.boxSizing == SdBoxSizing::BorderBox
			: autoInterpretation == SdAutoSizeInterpretation::BorderBox;
		return SdResolveBorderBoxAxisSize(resolvedSize, SdHorizontalBoxExtras(usedStyle), sizeUsesBorderBox);
	}

	inline float SdResolveBorderBoxHeight(
		const SdBoxStyle& sourceStyle,
		const SdResolvedBoxStyle& usedStyle,
		float autoSize = 0.0f,
		SdAutoSizeInterpretation autoInterpretation = SdAutoSizeInterpretation::BorderBox) noexcept
	{
		const bool explicitHeight = SdIsExplicitLength(sourceStyle.height);
		float resolvedSize = explicitHeight || autoSize <= 0.0f ? usedStyle.height : autoSize;
		resolvedSize = SdClampResolvedAxisSize(resolvedSize, usedStyle.minHeight, usedStyle.maxHeight);
		const bool sizeUsesBorderBox = explicitHeight
			? usedStyle.boxSizing == SdBoxSizing::BorderBox
			: autoInterpretation == SdAutoSizeInterpretation::BorderBox;
		return SdResolveBorderBoxAxisSize(resolvedSize, SdVerticalBoxExtras(usedStyle), sizeUsesBorderBox);
	}

	inline SdUsedBox SdUsedBoxFromNode(const SdBoxNode& box) noexcept
	{
		return {
			box.marginBox,
			box.borderBox,
			box.paddingBox,
			box.contentBox
		};
	}

	inline SdUsedBox SdBuildUsedBox(SdRect borderBox, const SdBoxStyle& style) noexcept
	{
		const SdVec2 basis = borderBox.Size();
		const SdResolvedBoxEdges margin = SdResolveBoxEdges(style.margin, basis);
		const SdResolvedBoxEdges padding = SdResolveBoxEdges(style.padding, basis);
		const SdResolvedBoxEdges border = SdResolveBorderEdges(style.border, basis);
		SdUsedBox used = {};
		used.borderBox = borderBox;
		used.marginBox = SdOutsetRect(borderBox, margin);
		used.paddingBox = SdInsetRect(borderBox, border);
		used.contentBox = SdInsetRect(used.paddingBox, padding);
		return used;
	}

	class SdBoxTree final
	{
	private:
		struct SdFloatPlacement final
		{
			SdRect marginBox = {};
			SdFloat side = SdFloat::None;
		};

		struct SdFloatLine final
		{
			float left = 0.0f;
			float right = 0.0f;
		};

		struct SdGridPlacement final
		{
			SdInt32 column = 0;
			SdInt32 row = 0;
			SdInt32 columnSpan = 1;
			SdInt32 rowSpan = 1;
		};

		std::vector<SdBoxNode> boxes = {};
		std::vector<SdUInt32> rootIndices = {};
		std::vector<SdUInt32> lastChildIndices = {};
		std::unordered_map<SdStyleNodeId, SdUInt32> boxIndexByStyleNodeId = {};
		SdRect viewportBox = {};

		static bool IsInlineLevel(SdDisplay display) noexcept
		{
			return display == SdDisplay::Inline || display == SdDisplay::InlineBlock;
		}

		static bool IsOutOfFlowPosition(SdPosition position) noexcept
		{
			return position == SdPosition::Absolute || position == SdPosition::Fixed;
		}

		static bool HasExplicitBorderBox(SdRect rect) noexcept
		{
			return rect.Width() > 0.0f || rect.Height() > 0.0f;
		}

		static SdRect TranslateRect(SdRect rect, SdVec2 offset) noexcept
		{
			return { rect.min + offset, rect.max + offset };
		}

		static float OuterWidth(const SdBoxNode& box) noexcept
		{
			return box.usedStyleValues.margin.left + box.measuredBorderSize.x + box.usedStyleValues.margin.right;
		}

		static float OuterHeight(const SdBoxNode& box) noexcept
		{
			return box.usedStyleValues.margin.top + box.measuredBorderSize.y + box.usedStyleValues.margin.bottom;
		}

		void SetBoxEmpty(SdBoxNode& box) noexcept
		{
			box.marginBox = {};
			box.borderBox = {};
			box.paddingBox = {};
			box.contentBox = {};
			box.measuredContentSize = {};
			box.measuredBorderSize = {};
		}

		void SetSubtreeEmpty(SdUInt32 boxIndex)
		{
			if (boxIndex >= boxes.size())
				return;
			SetBoxEmpty(boxes[boxIndex]);
			for (SdUInt32 childIndex = boxes[boxIndex].firstChildIndex;
				childIndex != SdInvalidIndex<SdUInt32>;
				childIndex = boxes[childIndex].nextSiblingIndex)
			{
				SetSubtreeEmpty(childIndex);
			}
		}

		void SetBoxRect(SdBoxNode& box, SdRect borderBox)
		{
			box.borderBox = borderBox;
			box.marginBox = SdOutsetRect(borderBox, box.usedStyleValues.margin);
			box.paddingBox = SdInsetRect(borderBox, box.usedStyleValues.border);
			box.contentBox = SdInsetRect(box.paddingBox, box.usedStyleValues.padding);
		}

		void ResolveUsedStyle(SdBoxNode& box, SdVec2 basis) noexcept
		{
			box.usedStyleValues = SdResolveBoxStyle(box.style, basis, box.intrinsicSize);
			box.display = box.usedStyleValues.display;
			box.position = box.usedStyleValues.position;
		}

		SdVec2 IntrinsicContentSize(const SdBoxNode& box) const noexcept
		{
			return {
				std::max(0.0f, box.intrinsicSize.x - SdHorizontalBoxExtras(box.usedStyleValues)),
				std::max(0.0f, box.intrinsicSize.y - SdVerticalBoxExtras(box.usedStyleValues))
			};
		}

		float ResolveLayoutBorderWidth(const SdBoxNode& box, float availableBorderWidth, bool stretchAutoWidth) const noexcept
		{
			if (SdIsExplicitLength(box.style.width))
				return SdResolveBorderBoxWidth(box.style, box.usedStyleValues);
			if (stretchAutoWidth)
			{
				return SdResolveBorderBoxWidth(
					box.style,
					box.usedStyleValues,
					std::max(0.0f, availableBorderWidth),
					SdAutoSizeInterpretation::BorderBox);
			}
			return SdResolveBorderBoxWidth(
				box.style,
				box.usedStyleValues,
				box.measuredContentSize.x,
				SdAutoSizeInterpretation::ContentBox);
		}

		float ResolveLayoutBorderHeight(const SdBoxNode& box, float availableBorderHeight = 0.0f, bool stretchAutoHeight = false) const noexcept
		{
			if (SdIsExplicitLength(box.style.height))
				return SdResolveBorderBoxHeight(box.style, box.usedStyleValues);
			if (stretchAutoHeight)
			{
				return SdResolveBorderBoxHeight(
					box.style,
					box.usedStyleValues,
					std::max(0.0f, availableBorderHeight),
					SdAutoSizeInterpretation::BorderBox);
			}
			return SdResolveBorderBoxHeight(
				box.style,
				box.usedStyleValues,
				box.measuredContentSize.y,
				SdAutoSizeInterpretation::ContentBox);
		}

		SdVec2 RelativeOffset(const SdBoxNode& box) const noexcept
		{
			if (box.usedStyleValues.position != SdPosition::Relative)
				return {};
			SdVec2 offset = {};
			if (SdIsExplicitLength(box.style.inset.left))
				offset.x = box.usedStyleValues.inset.left;
			else if (SdIsExplicitLength(box.style.inset.right))
				offset.x = -box.usedStyleValues.inset.right;
			if (SdIsExplicitLength(box.style.inset.top))
				offset.y = box.usedStyleValues.inset.top;
			else if (SdIsExplicitLength(box.style.inset.bottom))
				offset.y = -box.usedStyleValues.inset.bottom;
			return offset;
		}

		SdRect ApplyRelativeOffset(const SdBoxNode& box, SdRect rect) const noexcept
		{
			return TranslateRect(rect, RelativeOffset(box));
		}

		SdSize CountNormalChildren(SdUInt32 parentIndex, bool includeInline = true) const noexcept
		{
			SdSize count = 0;
			for (SdUInt32 childIndex = boxes[parentIndex].firstChildIndex;
				childIndex != SdInvalidIndex<SdUInt32>;
				childIndex = boxes[childIndex].nextSiblingIndex)
			{
				const SdBoxNode& child = boxes[childIndex];
				if (child.display == SdDisplay::None || IsOutOfFlowPosition(child.usedStyleValues.position))
					continue;
				if (!includeInline && IsInlineLevel(child.display))
					continue;
				++count;
			}
			return count;
		}

		void MeasureSubtree(SdUInt32 boxIndex, SdVec2 containingSize)
		{
			SdBoxNode& box = boxes[boxIndex];
			ResolveUsedStyle(box, containingSize);
			if (box.display == SdDisplay::None)
			{
				SetSubtreeEmpty(boxIndex);
				return;
			}

			const float availableBorderWidth = std::max(
				0.0f,
				containingSize.x - box.usedStyleValues.margin.left - box.usedStyleValues.margin.right);
			const bool shrinkWrap = IsInlineLevel(box.display) || box.usedStyleValues.floatMode != SdFloat::None;
			float tentativeBorderWidth = ResolveLayoutBorderWidth(box, availableBorderWidth, !shrinkWrap);
			if (shrinkWrap && !SdIsExplicitLength(box.style.width))
				tentativeBorderWidth = std::max(tentativeBorderWidth, box.intrinsicSize.x);
			const float tentativeContentWidth = std::max(0.0f, tentativeBorderWidth - SdHorizontalBoxExtras(box.usedStyleValues));

			for (SdUInt32 childIndex = box.firstChildIndex;
				childIndex != SdInvalidIndex<SdUInt32>;
				childIndex = boxes[childIndex].nextSiblingIndex)
			{
				MeasureSubtree(childIndex, { tentativeContentWidth, containingSize.y });
			}

			SdVec2 measuredContent = IntrinsicContentSize(box);
			const SdVec2 childrenContent = MeasureChildrenContent(boxIndex, tentativeContentWidth);
			measuredContent.x = std::max(measuredContent.x, childrenContent.x);
			measuredContent.y = std::max(measuredContent.y, childrenContent.y);

			const bool finalShrinkWrap = IsInlineLevel(box.display) || box.usedStyleValues.floatMode != SdFloat::None;
			const float finalAutoWidth = finalShrinkWrap ? measuredContent.x : availableBorderWidth;
			const SdAutoSizeInterpretation widthInterpretation = finalShrinkWrap
				? SdAutoSizeInterpretation::ContentBox
				: SdAutoSizeInterpretation::BorderBox;
			box.measuredBorderSize.x = SdIsExplicitLength(box.style.width)
				? SdResolveBorderBoxWidth(box.style, box.usedStyleValues)
				: SdResolveBorderBoxWidth(box.style, box.usedStyleValues, finalAutoWidth, widthInterpretation);
			box.measuredBorderSize.y = SdIsExplicitLength(box.style.height)
				? SdResolveBorderBoxHeight(box.style, box.usedStyleValues)
				: SdResolveBorderBoxHeight(box.style, box.usedStyleValues, measuredContent.y, SdAutoSizeInterpretation::ContentBox);
			box.measuredContentSize = {
				std::max(0.0f, box.measuredBorderSize.x - SdHorizontalBoxExtras(box.usedStyleValues)),
				std::max(0.0f, box.measuredBorderSize.y - SdVerticalBoxExtras(box.usedStyleValues))
			};
		}

		SdVec2 MeasureChildrenContent(SdUInt32 parentIndex, float contentWidth) const
		{
			const SdBoxNode& parent = boxes[parentIndex];
			switch (parent.display)
			{
			case SdDisplay::Flex:
				return MeasureFlexContent(parentIndex);
			case SdDisplay::Grid:
				return MeasureGridContent(parentIndex, contentWidth);
			case SdDisplay::None:
				return {};
			case SdDisplay::Block:
			case SdDisplay::Inline:
			case SdDisplay::InlineBlock:
			default:
				return MeasureBlockContent(parentIndex, contentWidth);
			}
		}

		SdVec2 MeasureBlockContent(SdUInt32 parentIndex, float contentWidth) const
		{
			const SdBoxNode& parent = boxes[parentIndex];
			float y = 0.0f;
			float lineWidth = 0.0f;
			float lineHeight = 0.0f;
			float maxWidth = 0.0f;
			float maxFloatBottom = 0.0f;
			bool hasLine = false;
			const float wrapWidth = contentWidth > 0.0f ? contentWidth : parent.measuredContentSize.x;
			const auto flushLine = [&]()
			{
				if (!hasLine)
					return;
				y += lineHeight;
				maxWidth = std::max(maxWidth, lineWidth);
				lineWidth = 0.0f;
				lineHeight = 0.0f;
				hasLine = false;
			};

			for (SdUInt32 childIndex = parent.firstChildIndex;
				childIndex != SdInvalidIndex<SdUInt32>;
				childIndex = boxes[childIndex].nextSiblingIndex)
			{
				const SdBoxNode& child = boxes[childIndex];
				if (child.display == SdDisplay::None || IsOutOfFlowPosition(child.usedStyleValues.position))
					continue;

				const float childOuterWidth = OuterWidth(child);
				const float childOuterHeight = OuterHeight(child);
				if (child.usedStyleValues.floatMode != SdFloat::None)
				{
					maxFloatBottom = std::max(maxFloatBottom, y + childOuterHeight);
					maxWidth = std::max(maxWidth, childOuterWidth);
					continue;
				}

				if (IsInlineLevel(child.display))
				{
					const bool wraps = wrapWidth > 0.0f
						&& hasLine
						&& lineWidth + parent.usedStyleValues.gap + childOuterWidth > wrapWidth;
					if (wraps)
						flushLine();
					if (hasLine)
						lineWidth += parent.usedStyleValues.gap;
					lineWidth += childOuterWidth;
					lineHeight = std::max(lineHeight, childOuterHeight);
					hasLine = true;
					continue;
				}

				flushLine();
				y += childOuterHeight;
				maxWidth = std::max(maxWidth, childOuterWidth);
			}
			flushLine();
			return { maxWidth, std::max(y, maxFloatBottom) };
		}

		SdVec2 MeasureFlexContent(SdUInt32 parentIndex) const
		{
			const SdBoxNode& parent = boxes[parentIndex];
			const bool row = parent.usedStyleValues.flexDirection == SdFlexDirection::Row;
			float main = 0.0f;
			float cross = 0.0f;
			SdSize visibleChildCount = 0;
			for (SdUInt32 childIndex = parent.firstChildIndex;
				childIndex != SdInvalidIndex<SdUInt32>;
				childIndex = boxes[childIndex].nextSiblingIndex)
			{
				const SdBoxNode& child = boxes[childIndex];
				if (child.display == SdDisplay::None || IsOutOfFlowPosition(child.usedStyleValues.position))
					continue;
				const float childMain = row ? OuterWidth(child) : OuterHeight(child);
				const float childCross = row ? OuterHeight(child) : OuterWidth(child);
				main += childMain;
				cross = std::max(cross, childCross);
				++visibleChildCount;
			}
			if (visibleChildCount > 1)
				main += parent.usedStyleValues.gap * static_cast<float>(visibleChildCount - 1);
			return row ? SdVec2{ main, cross } : SdVec2{ cross, main };
		}

		SdInt32 ResolveGridColumnCount(const SdBoxNode& parent, SdSize itemCount) const noexcept
		{
			(void)itemCount;
			if (parent.usedStyleValues.gridColumnCount > 0)
				return parent.usedStyleValues.gridColumnCount;
			return 1;
		}

		SdGridPlacement ResolveGridPlacement(const SdBoxNode& child, SdInt32 autoIndex, SdInt32 columnCount) const noexcept
		{
			SdGridPlacement placement = {};
			placement.columnSpan = std::max<SdInt32>(1, child.usedStyleValues.gridColumnSpan);
			placement.rowSpan = std::max<SdInt32>(1, child.usedStyleValues.gridRowSpan);
			placement.column = child.usedStyleValues.gridColumn > 0
				? child.usedStyleValues.gridColumn - 1
				: autoIndex % columnCount;
			placement.row = child.usedStyleValues.gridRow > 0
				? child.usedStyleValues.gridRow - 1
				: autoIndex / columnCount;
			placement.column = std::max<SdInt32>(0, placement.column);
			placement.row = std::max<SdInt32>(0, placement.row);
			return placement;
		}

		SdVec2 MeasureGridContent(SdUInt32 parentIndex, float contentWidth) const
		{
			const SdBoxNode& parent = boxes[parentIndex];
			const SdSize itemCount = CountNormalChildren(parentIndex);
			const SdInt32 columnCount = ResolveGridColumnCount(parent, itemCount);
			const float columnGap = parent.usedStyleValues.gridColumnGap;
			const float rowGap = parent.usedStyleValues.gridRowGap;
			const float explicitColumnWidth = parent.usedStyleValues.gridAutoColumns;
			const float explicitRowHeight = parent.usedStyleValues.gridAutoRows;
			const float availableColumnWidth = columnCount > 0
				? std::max(0.0f, (contentWidth - columnGap * static_cast<float>(columnCount - 1)) / static_cast<float>(columnCount))
				: 0.0f;
			float columnWidth = explicitColumnWidth > 0.0f ? explicitColumnWidth : availableColumnWidth;
			SdInt32 rowCount = std::max<SdInt32>(0, parent.usedStyleValues.gridRowCount);
			SdInt32 autoIndex = 0;
			std::vector<float> rowHeights = {};
			for (SdUInt32 childIndex = parent.firstChildIndex;
				childIndex != SdInvalidIndex<SdUInt32>;
				childIndex = boxes[childIndex].nextSiblingIndex)
			{
				const SdBoxNode& child = boxes[childIndex];
				if (child.display == SdDisplay::None || IsOutOfFlowPosition(child.usedStyleValues.position))
					continue;
				const SdGridPlacement placement = ResolveGridPlacement(child, autoIndex, columnCount);
				++autoIndex;
				rowCount = std::max(rowCount, placement.row + placement.rowSpan);
				if (explicitColumnWidth <= 0.0f && contentWidth <= 0.0f)
					columnWidth = std::max(columnWidth, OuterWidth(child));
				if (explicitRowHeight <= 0.0f)
				{
					if (static_cast<SdInt32>(rowHeights.size()) < rowCount)
						rowHeights.resize(rowCount, 0.0f);
					const float perRowHeight = OuterHeight(child) / static_cast<float>(placement.rowSpan);
					for (SdInt32 row = placement.row; row < placement.row + placement.rowSpan; ++row)
						rowHeights[static_cast<SdSize>(row)] = std::max(rowHeights[static_cast<SdSize>(row)], perRowHeight);
				}
			}
			const float gridWidth = columnWidth * static_cast<float>(columnCount)
				+ columnGap * static_cast<float>(std::max<SdInt32>(0, columnCount - 1));
			float gridHeight = 0.0f;
			if (explicitRowHeight > 0.0f)
			{
				gridHeight = explicitRowHeight * static_cast<float>(rowCount)
					+ rowGap * static_cast<float>(std::max<SdInt32>(0, rowCount - 1));
			}
			else
			{
				for (float rowHeight : rowHeights)
					gridHeight += rowHeight;
				if (!rowHeights.empty())
					gridHeight += rowGap * static_cast<float>(rowHeights.size() - 1);
			}
			return { gridWidth, gridHeight };
		}

		static void PruneFloats(std::vector<SdFloatPlacement>& floats, float y)
		{
			floats.erase(
				std::remove_if(
					floats.begin(),
					floats.end(),
					[y](const SdFloatPlacement& floatBox)
					{
						return floatBox.marginBox.max.y <= y;
					}),
				floats.end());
		}

		SdFloatLine ResolveFloatLine(SdRect contentBox, const std::vector<SdFloatPlacement>& floats, float y) const noexcept
		{
			SdFloatLine line{ contentBox.min.x, contentBox.max.x };
			for (const SdFloatPlacement& floatBox : floats)
			{
				if (y < floatBox.marginBox.min.y || y >= floatBox.marginBox.max.y)
					continue;
				if (floatBox.side == SdFloat::Left)
					line.left = std::max(line.left, floatBox.marginBox.max.x);
				else if (floatBox.side == SdFloat::Right)
					line.right = std::min(line.right, floatBox.marginBox.min.x);
			}
			if (line.right < line.left)
				line.right = line.left;
			return line;
		}

		float NextFloatBottom(const std::vector<SdFloatPlacement>& floats, float y) const noexcept
		{
			float next = y;
			bool found = false;
			for (const SdFloatPlacement& floatBox : floats)
			{
				if (y >= floatBox.marginBox.min.y && y < floatBox.marginBox.max.y)
				{
					next = found ? std::min(next, floatBox.marginBox.max.y) : floatBox.marginBox.max.y;
					found = true;
				}
			}
			return found ? next : y;
		}

		void LayoutSubtree(SdUInt32 boxIndex)
		{
			SdBoxNode& box = boxes[boxIndex];
			if (box.display == SdDisplay::None)
				return;
			switch (box.display)
			{
			case SdDisplay::Flex:
				LayoutFlexChildren(boxIndex);
				break;
			case SdDisplay::Grid:
				LayoutGridChildren(boxIndex);
				break;
			case SdDisplay::None:
				break;
			case SdDisplay::Block:
			case SdDisplay::Inline:
			case SdDisplay::InlineBlock:
			default:
				LayoutBlockChildren(boxIndex);
				break;
			}
		}

		void LayoutPositionedChild(SdUInt32 childIndex, SdRect containingBlock)
		{
			SdBoxNode& child = boxes[childIndex];
			if (child.display == SdDisplay::None)
			{
				SetSubtreeEmpty(childIndex);
				return;
			}
			if (HasExplicitBorderBox(child.explicitBorderBox))
			{
				SetBoxRect(child, child.explicitBorderBox);
				LayoutSubtree(childIndex);
				return;
			}

			const SdRect positionBlock = child.usedStyleValues.position == SdPosition::Fixed ? viewportBox : containingBlock;
			const bool leftSet = SdIsExplicitLength(child.style.inset.left);
			const bool rightSet = SdIsExplicitLength(child.style.inset.right);
			const bool topSet = SdIsExplicitLength(child.style.inset.top);
			const bool bottomSet = SdIsExplicitLength(child.style.inset.bottom);
			float width = ResolveLayoutBorderWidth(child, positionBlock.Width(), false);
			float height = ResolveLayoutBorderHeight(child);
			if (!SdIsExplicitLength(child.style.width) && leftSet && rightSet)
			{
				width = std::max(
					0.0f,
					positionBlock.Width()
						- child.usedStyleValues.inset.left
						- child.usedStyleValues.inset.right
						- child.usedStyleValues.margin.left
						- child.usedStyleValues.margin.right);
			}
			if (!SdIsExplicitLength(child.style.height) && topSet && bottomSet)
			{
				height = std::max(
					0.0f,
					positionBlock.Height()
						- child.usedStyleValues.inset.top
						- child.usedStyleValues.inset.bottom
						- child.usedStyleValues.margin.top
						- child.usedStyleValues.margin.bottom);
			}

			float x = positionBlock.min.x + child.usedStyleValues.margin.left;
			if (leftSet)
				x = positionBlock.min.x + child.usedStyleValues.inset.left + child.usedStyleValues.margin.left;
			else if (rightSet)
				x = positionBlock.max.x - child.usedStyleValues.inset.right - child.usedStyleValues.margin.right - width;

			float y = positionBlock.min.y + child.usedStyleValues.margin.top;
			if (topSet)
				y = positionBlock.min.y + child.usedStyleValues.inset.top + child.usedStyleValues.margin.top;
			else if (bottomSet)
				y = positionBlock.max.y - child.usedStyleValues.inset.bottom - child.usedStyleValues.margin.bottom - height;

			SetBoxRect(child, { x, y, x + width, y + height });
			LayoutSubtree(childIndex);
		}

		void LayoutFloatChild(
			SdUInt32 childIndex,
			SdBoxNode& parent,
			std::vector<SdFloatPlacement>& floats,
			float y,
			float& maxFloatBottom)
		{
			SdBoxNode& child = boxes[childIndex];
			const float width = ResolveLayoutBorderWidth(child, parent.contentBox.Width(), false);
			const float height = ResolveLayoutBorderHeight(child);
			float top = y + child.usedStyleValues.margin.top;
			for (;;)
			{
				PruneFloats(floats, top);
				const SdFloatLine line = ResolveFloatLine(parent.contentBox, floats, top);
				const float requiredWidth = child.usedStyleValues.margin.left + width + child.usedStyleValues.margin.right;
				if (line.right - line.left >= requiredWidth || floats.empty())
					break;
				const float next = NextFloatBottom(floats, top);
				if (next <= top)
					break;
				top = next;
			}

			const SdFloatLine line = ResolveFloatLine(parent.contentBox, floats, top);
			const float x = child.usedStyleValues.floatMode == SdFloat::Right
				? line.right - child.usedStyleValues.margin.right - width
				: line.left + child.usedStyleValues.margin.left;
			const SdRect unshifted = { x, top, x + width, top + height };
			SetBoxRect(child, ApplyRelativeOffset(child, unshifted));
			LayoutSubtree(childIndex);
			SdFloatPlacement placement = {};
			placement.marginBox = SdOutsetRect(unshifted, child.usedStyleValues.margin);
			placement.side = child.usedStyleValues.floatMode;
			floats.push_back(placement);
			maxFloatBottom = std::max(maxFloatBottom, placement.marginBox.max.y);
		}

		void LayoutInlineRun(
			SdUInt32 parentIndex,
			SdUInt32& childIndex,
			float& y,
			std::vector<SdFloatPlacement>& floats,
			float& maxFloatBottom)
		{
			SdBoxNode& parent = boxes[parentIndex];
			float lineTop = y;
			PruneFloats(floats, lineTop);
			SdFloatLine line = ResolveFloatLine(parent.contentBox, floats, lineTop);
			float x = line.left;
			float lineHeight = 0.0f;
			bool hasLine = false;

			while (childIndex != SdInvalidIndex<SdUInt32>)
			{
				SdBoxNode& child = boxes[childIndex];
				const SdUInt32 nextSibling = child.nextSiblingIndex;
				if (child.display == SdDisplay::None)
				{
					SetSubtreeEmpty(childIndex);
					childIndex = nextSibling;
					continue;
				}
				if (IsOutOfFlowPosition(child.usedStyleValues.position))
				{
					LayoutPositionedChild(childIndex, parent.contentBox);
					childIndex = nextSibling;
					continue;
				}
				if (child.usedStyleValues.floatMode != SdFloat::None)
				{
					LayoutFloatChild(childIndex, parent, floats, y, maxFloatBottom);
					childIndex = nextSibling;
					continue;
				}
				if (!IsInlineLevel(child.display))
					break;

				const float availableWidth = std::max(
					0.0f,
					line.right - line.left - child.usedStyleValues.margin.left - child.usedStyleValues.margin.right);
				const float width = ResolveLayoutBorderWidth(child, availableWidth, false);
				const float height = ResolveLayoutBorderHeight(child);
				const float outerWidth = child.usedStyleValues.margin.left + width + child.usedStyleValues.margin.right;
				const bool wraps = hasLine && x + outerWidth > line.right;
				if (wraps)
				{
					y = lineTop + lineHeight;
					lineTop = y;
					PruneFloats(floats, lineTop);
					line = ResolveFloatLine(parent.contentBox, floats, lineTop);
					x = line.left;
					lineHeight = 0.0f;
					hasLine = false;
				}

				const float itemX = x + child.usedStyleValues.margin.left;
				const float itemY = lineTop + child.usedStyleValues.margin.top;
				const SdRect unshifted = { itemX, itemY, itemX + width, itemY + height };
				SetBoxRect(child, ApplyRelativeOffset(child, unshifted));
				LayoutSubtree(childIndex);
				x = itemX + width + child.usedStyleValues.margin.right + parent.usedStyleValues.gap;
				lineHeight = std::max(lineHeight, child.usedStyleValues.margin.top + height + child.usedStyleValues.margin.bottom);
				hasLine = true;
				childIndex = nextSibling;
			}

			if (hasLine)
				y = lineTop + lineHeight;
		}

		void LayoutBlockChildren(SdUInt32 parentIndex)
		{
			SdBoxNode& parent = boxes[parentIndex];
			std::vector<SdFloatPlacement> floats = {};
			floats.reserve(CountNormalChildren(parentIndex));
			float y = parent.contentBox.min.y;
			float maxFloatBottom = y;

			for (SdUInt32 childIndex = parent.firstChildIndex;
				childIndex != SdInvalidIndex<SdUInt32>;)
			{
				SdBoxNode& child = boxes[childIndex];
				const SdUInt32 nextSibling = child.nextSiblingIndex;
				if (child.display == SdDisplay::None)
				{
					SetSubtreeEmpty(childIndex);
					childIndex = nextSibling;
					continue;
				}
				if (IsOutOfFlowPosition(child.usedStyleValues.position))
				{
					LayoutPositionedChild(childIndex, parent.contentBox);
					childIndex = nextSibling;
					continue;
				}
				if (child.usedStyleValues.floatMode != SdFloat::None)
				{
					LayoutFloatChild(childIndex, parent, floats, y, maxFloatBottom);
					childIndex = nextSibling;
					continue;
				}
				if (IsInlineLevel(child.display))
				{
					LayoutInlineRun(parentIndex, childIndex, y, floats, maxFloatBottom);
					continue;
				}

				const float top = y + child.usedStyleValues.margin.top;
				PruneFloats(floats, top);
				const SdFloatLine line = ResolveFloatLine(parent.contentBox, floats, top);
				const float availableBorderWidth = std::max(
					0.0f,
					line.right - line.left - child.usedStyleValues.margin.left - child.usedStyleValues.margin.right);
				const float width = ResolveLayoutBorderWidth(child, availableBorderWidth, true);
				const float height = ResolveLayoutBorderHeight(child);
				const float x = line.left + child.usedStyleValues.margin.left;
				const SdRect unshifted = { x, top, x + width, top + height };
				SetBoxRect(child, ApplyRelativeOffset(child, unshifted));
				LayoutSubtree(childIndex);
				y = unshifted.max.y + child.usedStyleValues.margin.bottom;
				childIndex = nextSibling;
			}
			(void)maxFloatBottom;
		}

		float FlexChildBaseWidth(const SdBoxNode& child, bool row) const noexcept
		{
			const bool usesFlexBasis = row && SdIsExplicitLength(child.style.flexBasis);
			if (usesFlexBasis)
			{
				const bool sizeUsesBorderBox = child.usedStyleValues.boxSizing == SdBoxSizing::BorderBox;
				return SdResolveBorderBoxAxisSize(
					child.usedStyleValues.flexBasis,
					SdHorizontalBoxExtras(child.usedStyleValues),
					sizeUsesBorderBox);
			}
			return ResolveLayoutBorderWidth(child, child.measuredBorderSize.x, false);
		}

		float FlexChildBaseHeight(const SdBoxNode& child, bool row) const noexcept
		{
			const bool usesFlexBasis = !row && SdIsExplicitLength(child.style.flexBasis);
			if (usesFlexBasis)
			{
				const bool sizeUsesBorderBox = child.usedStyleValues.boxSizing == SdBoxSizing::BorderBox;
				return SdResolveBorderBoxAxisSize(
					child.usedStyleValues.flexBasis,
					SdVerticalBoxExtras(child.usedStyleValues),
					sizeUsesBorderBox);
			}
			return ResolveLayoutBorderHeight(child);
		}

		void LayoutFlexChildren(SdUInt32 parentIndex)
		{
			SdBoxNode& parent = boxes[parentIndex];
			const bool row = parent.usedStyleValues.flexDirection == SdFlexDirection::Row;
			const float parentMainSize = row ? parent.contentBox.Width() : parent.contentBox.Height();
			float occupiedMainSize = 0.0f;
			float totalFlexGrow = 0.0f;
			float totalWeightedFlexShrink = 0.0f;
			SdSize visibleChildCount = 0;

			for (SdUInt32 childIndex = parent.firstChildIndex;
				childIndex != SdInvalidIndex<SdUInt32>;
				childIndex = boxes[childIndex].nextSiblingIndex)
			{
				SdBoxNode& child = boxes[childIndex];
				if (child.display == SdDisplay::None)
				{
					SetSubtreeEmpty(childIndex);
					continue;
				}
				if (IsOutOfFlowPosition(child.usedStyleValues.position))
					continue;
				const float childWidth = FlexChildBaseWidth(child, row);
				const float childHeight = FlexChildBaseHeight(child, row);
				occupiedMainSize += row
					? child.usedStyleValues.margin.left + childWidth + child.usedStyleValues.margin.right
					: child.usedStyleValues.margin.top + childHeight + child.usedStyleValues.margin.bottom;
				totalFlexGrow += std::max(0.0f, child.usedStyleValues.flexGrow);
				totalWeightedFlexShrink += std::max(0.0f, child.usedStyleValues.flexShrink) * (row ? childWidth : childHeight);
				++visibleChildCount;
			}

			if (visibleChildCount > 1)
				occupiedMainSize += parent.usedStyleValues.gap * static_cast<float>(visibleChildCount - 1);

			const float remainingMainSize = std::max(0.0f, parentMainSize - occupiedMainSize);
			const float overflowingMainSize = std::max(0.0f, occupiedMainSize - parentMainSize);
			const bool distributeFlexGrow = remainingMainSize > 0.0f && totalFlexGrow > 0.0f;
			const bool distributeFlexShrink = overflowingMainSize > 0.0f && totalWeightedFlexShrink > 0.0f;
			float main = row ? parent.contentBox.min.x : parent.contentBox.min.y;
			float gap = parent.usedStyleValues.gap;
			if (!distributeFlexGrow)
			{
				switch (parent.usedStyleValues.justifyContent)
				{
				case SdJustifyContent::Center:
					main += remainingMainSize * 0.5f;
					break;
				case SdJustifyContent::FlexEnd:
					main += remainingMainSize;
					break;
				case SdJustifyContent::SpaceBetween:
					if (visibleChildCount > 1)
						gap += remainingMainSize / static_cast<float>(visibleChildCount - 1);
					break;
				case SdJustifyContent::FlexStart:
				default:
					break;
				}
			}

			for (SdUInt32 childIndex = parent.firstChildIndex;
				childIndex != SdInvalidIndex<SdUInt32>;
				childIndex = boxes[childIndex].nextSiblingIndex)
			{
				SdBoxNode& child = boxes[childIndex];
				if (child.display == SdDisplay::None)
				{
					SetSubtreeEmpty(childIndex);
					continue;
				}
				if (IsOutOfFlowPosition(child.usedStyleValues.position))
				{
					LayoutPositionedChild(childIndex, parent.contentBox);
					continue;
				}

				float childWidth = FlexChildBaseWidth(child, row);
				float childHeight = FlexChildBaseHeight(child, row);
				if (distributeFlexGrow)
				{
					const float growDelta = remainingMainSize * (std::max(0.0f, child.usedStyleValues.flexGrow) / totalFlexGrow);
					if (row)
						childWidth += growDelta;
					else
						childHeight += growDelta;
				}
				else if (distributeFlexShrink)
				{
					const float weightedShrink = std::max(0.0f, child.usedStyleValues.flexShrink) * (row ? childWidth : childHeight);
					const float shrinkDelta = overflowingMainSize * (weightedShrink / totalWeightedFlexShrink);
					if (row)
						childWidth = std::max(0.0f, childWidth - shrinkDelta);
					else
						childHeight = std::max(0.0f, childHeight - shrinkDelta);
				}

				const auto resolveCrossAxis = [&parent](float childCrossSize, float marginStart, float marginEnd, float parentCrossMin, float parentCrossSize)
				{
					const float availableCrossSize = std::max(0.0f, parentCrossSize - marginStart - marginEnd);
					float crossSize = childCrossSize;
					float cross = parentCrossMin + marginStart;
					switch (parent.usedStyleValues.alignItems)
					{
					case SdAlignItems::Center:
						cross += std::max(0.0f, availableCrossSize - childCrossSize) * 0.5f;
						break;
					case SdAlignItems::FlexEnd:
						cross += std::max(0.0f, availableCrossSize - childCrossSize);
						break;
					case SdAlignItems::Stretch:
						crossSize = availableCrossSize;
						break;
					case SdAlignItems::FlexStart:
					default:
						break;
					}
					return SdVec2{ cross, std::max(0.0f, crossSize) };
				};

				SdRect unshifted = {};
				if (row)
				{
					const SdVec2 cross = resolveCrossAxis(
						childHeight,
						child.usedStyleValues.margin.top,
						child.usedStyleValues.margin.bottom,
						parent.contentBox.min.y,
						parent.contentBox.Height());
					unshifted = {
						main + child.usedStyleValues.margin.left,
						cross.x,
						main + child.usedStyleValues.margin.left + childWidth,
						cross.x + cross.y
					};
					main = unshifted.max.x + child.usedStyleValues.margin.right + gap;
				}
				else
				{
					const SdVec2 cross = resolveCrossAxis(
						childWidth,
						child.usedStyleValues.margin.left,
						child.usedStyleValues.margin.right,
						parent.contentBox.min.x,
						parent.contentBox.Width());
					unshifted = {
						cross.x,
						main + child.usedStyleValues.margin.top,
						cross.x + cross.y,
						main + child.usedStyleValues.margin.top + childHeight
					};
					main = unshifted.max.y + child.usedStyleValues.margin.bottom + gap;
				}
				SetBoxRect(child, ApplyRelativeOffset(child, unshifted));
				LayoutSubtree(childIndex);
			}
		}

		void BuildGridRows(
			SdUInt32 parentIndex,
			SdInt32 columnCount,
			float columnWidth,
			std::vector<float>& rowHeights,
			SdInt32& rowCount) const
		{
			const SdBoxNode& parent = boxes[parentIndex];
			rowCount = std::max<SdInt32>(0, parent.usedStyleValues.gridRowCount);
			rowHeights.clear();
			SdInt32 autoIndex = 0;
			for (SdUInt32 childIndex = parent.firstChildIndex;
				childIndex != SdInvalidIndex<SdUInt32>;
				childIndex = boxes[childIndex].nextSiblingIndex)
			{
				const SdBoxNode& child = boxes[childIndex];
				if (child.display == SdDisplay::None || IsOutOfFlowPosition(child.usedStyleValues.position))
					continue;
				const SdGridPlacement placement = ResolveGridPlacement(child, autoIndex, columnCount);
				++autoIndex;
				rowCount = std::max(rowCount, placement.row + placement.rowSpan);
				if (static_cast<SdInt32>(rowHeights.size()) < rowCount)
					rowHeights.resize(rowCount, parent.usedStyleValues.gridAutoRows);
				if (parent.usedStyleValues.gridAutoRows <= 0.0f)
				{
					const float childHeight = ResolveLayoutBorderHeight(child);
					const float outerHeight = child.usedStyleValues.margin.top + childHeight + child.usedStyleValues.margin.bottom;
					const float perRowHeight = outerHeight / static_cast<float>(placement.rowSpan);
					for (SdInt32 row = placement.row; row < placement.row + placement.rowSpan; ++row)
						rowHeights[static_cast<SdSize>(row)] = std::max(rowHeights[static_cast<SdSize>(row)], perRowHeight);
				}
				(void)columnWidth;
			}
		}

		float GridRowOffset(const std::vector<float>& rowHeights, float rowGap, SdInt32 row) const noexcept
		{
			float offset = 0.0f;
			for (SdInt32 i = 0; i < row && i < static_cast<SdInt32>(rowHeights.size()); ++i)
				offset += rowHeights[static_cast<SdSize>(i)] + rowGap;
			return offset;
		}

		float GridRowSpanSize(const std::vector<float>& rowHeights, float rowGap, SdInt32 row, SdInt32 span) const noexcept
		{
			float size = 0.0f;
			for (SdInt32 i = row; i < row + span && i < static_cast<SdInt32>(rowHeights.size()); ++i)
			{
				if (i > row)
					size += rowGap;
				size += rowHeights[static_cast<SdSize>(i)];
			}
			return size;
		}

		void LayoutGridChildren(SdUInt32 parentIndex)
		{
			SdBoxNode& parent = boxes[parentIndex];
			const SdSize itemCount = CountNormalChildren(parentIndex);
			const SdInt32 columnCount = ResolveGridColumnCount(parent, itemCount);
			const float columnGap = parent.usedStyleValues.gridColumnGap;
			const float rowGap = parent.usedStyleValues.gridRowGap;
			const float columnWidth = parent.usedStyleValues.gridAutoColumns > 0.0f
				? parent.usedStyleValues.gridAutoColumns
				: std::max(
					0.0f,
					(parent.contentBox.Width() - columnGap * static_cast<float>(std::max<SdInt32>(0, columnCount - 1)))
						/ static_cast<float>(std::max<SdInt32>(1, columnCount)));
			std::vector<float> rowHeights = {};
			SdInt32 rowCount = 0;
			BuildGridRows(parentIndex, columnCount, columnWidth, rowHeights, rowCount);
			if (rowHeights.empty() && rowCount > 0)
				rowHeights.resize(rowCount, parent.usedStyleValues.gridAutoRows);

			SdInt32 autoIndex = 0;
			for (SdUInt32 childIndex = parent.firstChildIndex;
				childIndex != SdInvalidIndex<SdUInt32>;
				childIndex = boxes[childIndex].nextSiblingIndex)
			{
				SdBoxNode& child = boxes[childIndex];
				if (child.display == SdDisplay::None)
				{
					SetSubtreeEmpty(childIndex);
					continue;
				}
				if (IsOutOfFlowPosition(child.usedStyleValues.position))
				{
					LayoutPositionedChild(childIndex, parent.contentBox);
					continue;
				}
				const SdGridPlacement placement = ResolveGridPlacement(child, autoIndex, columnCount);
				++autoIndex;
				const float cellX = parent.contentBox.min.x
					+ static_cast<float>(placement.column) * (columnWidth + columnGap);
				const float cellWidth = columnWidth * static_cast<float>(placement.columnSpan)
					+ columnGap * static_cast<float>(std::max<SdInt32>(0, placement.columnSpan - 1));
				const float cellY = parent.contentBox.min.y + GridRowOffset(rowHeights, rowGap, placement.row);
				const float cellHeight = GridRowSpanSize(rowHeights, rowGap, placement.row, placement.rowSpan);
				const float availableWidth = std::max(0.0f, cellWidth - child.usedStyleValues.margin.left - child.usedStyleValues.margin.right);
				const float availableHeight = std::max(0.0f, cellHeight - child.usedStyleValues.margin.top - child.usedStyleValues.margin.bottom);
				const float width = ResolveLayoutBorderWidth(child, availableWidth, !SdIsExplicitLength(child.style.width));
				const bool stretchHeight = parent.usedStyleValues.alignItems == SdAlignItems::Stretch && !SdIsExplicitLength(child.style.height);
				const float height = ResolveLayoutBorderHeight(child, availableHeight, stretchHeight);
				const float x = cellX + child.usedStyleValues.margin.left;
				float y = cellY + child.usedStyleValues.margin.top;
				if (!stretchHeight)
				{
					switch (parent.usedStyleValues.alignItems)
					{
					case SdAlignItems::Center:
						y += std::max(0.0f, availableHeight - height) * 0.5f;
						break;
					case SdAlignItems::FlexEnd:
						y += std::max(0.0f, availableHeight - height);
						break;
					case SdAlignItems::Stretch:
					case SdAlignItems::FlexStart:
					default:
						break;
					}
				}
				const SdRect unshifted = { x, y, x + width, y + height };
				SetBoxRect(child, ApplyRelativeOffset(child, unshifted));
				LayoutSubtree(childIndex);
			}
		}

	public:
		void Clear()
		{
			boxes.clear();
			rootIndices.clear();
			lastChildIndices.clear();
			boxIndexByStyleNodeId.clear();
			viewportBox = {};
		}

		SdUInt32 AddBox(SdStyleNodeId styleNodeId, SdUInt32 parentBoxIndex, const SdBoxStyle& style, SdVec2 intrinsicSize = {}, SdRect explicitBorderBox = {})
		{
			const SdUInt32 index = static_cast<SdUInt32>(boxes.size());
			SdBoxNode& box = boxes.emplace_back();
			box.styleNodeId = styleNodeId;
			box.parentBoxIndex = parentBoxIndex;
			box.style = style;
			box.display = style.display;
			box.position = style.position;
			box.intrinsicSize = intrinsicSize;
			box.explicitBorderBox = explicitBorderBox;
			lastChildIndices.push_back(SdInvalidIndex<SdUInt32>);
			if (styleNodeId != SdInvalidStyleNodeId)
				boxIndexByStyleNodeId[styleNodeId] = index;
			if (parentBoxIndex != SdInvalidIndex<SdUInt32> && parentBoxIndex < boxes.size())
			{
				SdBoxNode& parent = boxes[parentBoxIndex];
				if (parent.firstChildIndex == SdInvalidIndex<SdUInt32>)
					parent.firstChildIndex = index;
				else
					boxes[lastChildIndices[parentBoxIndex]].nextSiblingIndex = index;
				lastChildIndices[parentBoxIndex] = index;
			}
			else
			{
				rootIndices.push_back(index);
			}
			return index;
		}

		void Layout(SdRect containingBlock)
		{
			viewportBox = containingBlock;
			for (SdUInt32 rootIndex : rootIndices)
				MeasureSubtree(rootIndex, containingBlock.Size());

			float blockY = containingBlock.min.y;
			for (SdUInt32 rootIndex : rootIndices)
			{
				SdBoxNode& root = boxes[rootIndex];
				if (root.display == SdDisplay::None)
				{
					SetSubtreeEmpty(rootIndex);
					continue;
				}
				if (HasExplicitBorderBox(root.explicitBorderBox))
				{
					SetBoxRect(root, root.explicitBorderBox);
					LayoutSubtree(rootIndex);
					blockY = root.marginBox.max.y;
					continue;
				}
				if (IsOutOfFlowPosition(root.usedStyleValues.position))
				{
					LayoutPositionedChild(rootIndex, containingBlock);
					continue;
				}

				const bool shrinkWrap = IsInlineLevel(root.display) || root.usedStyleValues.floatMode != SdFloat::None;
				const float availableBorderWidth = std::max(
					0.0f,
					containingBlock.Width() - root.usedStyleValues.margin.left - root.usedStyleValues.margin.right);
				const float width = ResolveLayoutBorderWidth(root, availableBorderWidth, !shrinkWrap);
				const float height = ResolveLayoutBorderHeight(root);
				const float x = containingBlock.min.x + root.usedStyleValues.margin.left;
				const float y = blockY + root.usedStyleValues.margin.top;
				const SdRect unshifted = { x, y, x + width, y + height };
				SetBoxRect(root, ApplyRelativeOffset(root, unshifted));
				LayoutSubtree(rootIndex);
				blockY = unshifted.max.y + root.usedStyleValues.margin.bottom;
			}
		}

		const std::vector<SdBoxNode>& GetBoxes() const noexcept
		{
			return boxes;
		}

		SdSize GetBoxCount() const noexcept
		{
			return boxes.size();
		}

		const SdBoxNode* FindBoxByStyleNodeId(SdStyleNodeId styleNodeId) const noexcept
		{
			const auto it = boxIndexByStyleNodeId.find(styleNodeId);
			if (it == boxIndexByStyleNodeId.end() || it->second >= boxes.size())
				return nullptr;
			return &boxes[it->second];
		}
	};
}
