#pragma once

#include "SdStyleCore.h"

#include <algorithm>
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
		SdVec2 intrinsicSize = {};
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
		rect.max.x = std::max(rect.min.x, rect.max.x);
		rect.max.y = std::max(rect.min.y, rect.max.y);
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
		result.width = std::max(result.minWidth, result.width);
		result.height = std::max(result.minHeight, result.height);
		if (result.maxWidth > 0.0f)
			result.width = std::min(result.width, result.maxWidth);
		if (result.maxHeight > 0.0f)
			result.height = std::min(result.height, result.maxHeight);
		return result;
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
		std::vector<SdBoxNode> boxes = {};
		std::vector<SdUInt32> rootIndices = {};
		std::vector<SdUInt32> lastChildIndices = {};

		void SetBoxRect(SdBoxNode& box, SdRect borderBox)
		{
			box.borderBox = borderBox;
			box.marginBox = SdOutsetRect(borderBox, box.usedStyleValues.margin);
			box.paddingBox = SdInsetRect(borderBox, box.usedStyleValues.border);
			box.contentBox = SdInsetRect(box.paddingBox, box.usedStyleValues.padding);
		}

		void LayoutChildBlock(SdUInt32 parentIndex)
		{
			SdBoxNode& parent = boxes[parentIndex];
			if (parent.display == SdDisplay::None)
				return;

			if (parent.display == SdDisplay::Flex)
			{
				LayoutFlexChildren(parentIndex);
				return;
			}

			float y = parent.contentBox.min.y;
			for (SdUInt32 childIndex = parent.firstChildIndex;
				childIndex != SdInvalidIndex<SdUInt32>;
				childIndex = boxes[childIndex].nextSiblingIndex)
			{
				SdBoxNode& child = boxes[childIndex];
				if (child.display == SdDisplay::None)
					continue;

				const SdVec2 basis = parent.contentBox.Size();
				child.usedStyleValues = SdResolveBoxStyle(child.style, basis, child.intrinsicSize);
				const float borderWidth = std::max(0.0f, child.usedStyleValues.width
					+ child.usedStyleValues.padding.left + child.usedStyleValues.padding.right
					+ child.usedStyleValues.border.left + child.usedStyleValues.border.right);
				const float borderHeight = std::max(0.0f, child.usedStyleValues.height
					+ child.usedStyleValues.padding.top + child.usedStyleValues.padding.bottom
					+ child.usedStyleValues.border.top + child.usedStyleValues.border.bottom);
				const float x = parent.contentBox.min.x + child.usedStyleValues.margin.left;
				y += child.usedStyleValues.margin.top;
				SetBoxRect(child, { x, y, x + borderWidth, y + borderHeight });
				LayoutChildBlock(childIndex);
				y = child.marginBox.max.y;
			}
		}

		void LayoutFlexChildren(SdUInt32 parentIndex)
		{
			SdBoxNode& parent = boxes[parentIndex];
			const bool row = parent.usedStyleValues.flexDirection == SdFlexDirection::Row;
			float main = row ? parent.contentBox.min.x : parent.contentBox.min.y;
			for (SdUInt32 childIndex = parent.firstChildIndex;
				childIndex != SdInvalidIndex<SdUInt32>;
				childIndex = boxes[childIndex].nextSiblingIndex)
			{
				SdBoxNode& child = boxes[childIndex];
				if (child.display == SdDisplay::None)
					continue;

				child.usedStyleValues = SdResolveBoxStyle(child.style, parent.contentBox.Size(), child.intrinsicSize);
				const float childWidth = std::max(0.0f, child.usedStyleValues.width
					+ child.usedStyleValues.padding.left + child.usedStyleValues.padding.right
					+ child.usedStyleValues.border.left + child.usedStyleValues.border.right);
				const float childHeight = std::max(0.0f, child.usedStyleValues.height
					+ child.usedStyleValues.padding.top + child.usedStyleValues.padding.bottom
					+ child.usedStyleValues.border.top + child.usedStyleValues.border.bottom);
				SdRect rect = {};
				if (row)
				{
					rect = {
						main + child.usedStyleValues.margin.left,
						parent.contentBox.min.y + child.usedStyleValues.margin.top,
						main + child.usedStyleValues.margin.left + childWidth,
						parent.contentBox.min.y + child.usedStyleValues.margin.top + childHeight
					};
					main = rect.max.x + child.usedStyleValues.margin.right + parent.usedStyleValues.gap;
				}
				else
				{
					rect = {
						parent.contentBox.min.x + child.usedStyleValues.margin.left,
						main + child.usedStyleValues.margin.top,
						parent.contentBox.min.x + child.usedStyleValues.margin.left + childWidth,
						main + child.usedStyleValues.margin.top + childHeight
					};
					main = rect.max.y + child.usedStyleValues.margin.bottom + parent.usedStyleValues.gap;
				}
				SetBoxRect(child, rect);
				LayoutChildBlock(childIndex);
			}
		}

	public:
		void Clear()
		{
			boxes.clear();
			rootIndices.clear();
			lastChildIndices.clear();
		}

		SdUInt32 AddBox(SdStyleNodeId styleNodeId, SdUInt32 parentBoxIndex, const SdBoxStyle& style, SdVec2 intrinsicSize = {})
		{
			const SdUInt32 index = static_cast<SdUInt32>(boxes.size());
			SdBoxNode& box = boxes.emplace_back();
			box.styleNodeId = styleNodeId;
			box.parentBoxIndex = parentBoxIndex;
			box.style = style;
			box.display = style.display;
			box.position = style.position;
			box.intrinsicSize = intrinsicSize;
			lastChildIndices.push_back(SdInvalidIndex<SdUInt32>);
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
			float blockY = containingBlock.min.y;
			for (SdUInt32 rootIndex : rootIndices)
			{
				SdBoxNode& root = boxes[rootIndex];
				root.usedStyleValues = SdResolveBoxStyle(root.style, containingBlock.Size(), root.intrinsicSize);
				const float width = root.usedStyleValues.width > 0.0f ? root.usedStyleValues.width : containingBlock.Width();
				const float height = root.usedStyleValues.height;
				const SdRect rect = {
					containingBlock.min.x + root.usedStyleValues.margin.left,
					blockY + root.usedStyleValues.margin.top,
					containingBlock.min.x + root.usedStyleValues.margin.left + width,
					blockY + root.usedStyleValues.margin.top + height
				};
				SetBoxRect(root, rect);
				LayoutChildBlock(rootIndex);
				blockY = root.marginBox.max.y;
			}
		}

		const std::vector<SdBoxNode>& GetBoxes() const noexcept
		{
			return boxes;
		}
	};
}
