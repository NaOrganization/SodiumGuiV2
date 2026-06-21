#pragma once

#include "SdBoxLayout.h"

#include <algorithm>
#include <unordered_map>
#include <vector>

namespace Sodium
{
	struct SdLayoutNode final
	{
		SdWidgetId widgetId = 0;
		SdUInt32 parentIndex = SdInvalidIndex<SdUInt32>;
		SdUInt32 firstChildIndex = SdInvalidIndex<SdUInt32>;
		SdUInt32 nextSiblingIndex = SdInvalidIndex<SdUInt32>;
		SdLayoutConstraints constraints = {};
		SdLayoutResult result = {};
		SdRect targetRect = {};
		SdRect animatedRect = {};
		SdRect manualRect = {};
		SdRect childContentRect = {};
		SdRect computedClipRect = {};
		SdSpacing childBorder = {};
		SdSpacing contentPadding = {};
		float layoutWeight = 1.0f;
		float gap = 0.0f;
		bool manualLayout = false;
		bool arrangeChildren = false;
		bool clipChildren = false;
	};

	struct SdLayoutNodeInput final
	{
		SdWidgetId widgetId = 0;
		SdWidgetId parentId = 0;
		SdLayoutConstraints constraints = {};
		SdLayoutResult result = {};
		SdRect manualRect = {};
		SdSpacing childBorder = {};
		SdSpacing contentPadding = {};
		float layoutWeight = 1.0f;
		float gap = 0.0f;
		bool manualLayout = false;
		bool arrangeChildren = false;
		bool clipChildren = false;
	};

	class SdLayoutSystem final
	{
	private:
		std::vector<SdLayoutNode> nodes = {};
		std::vector<SdUInt32> rootIndices = {};
		std::vector<SdUInt32> lastChildIndices = {};
		std::unordered_map<SdWidgetId, SdUInt32> nodeIndexByWidgetId = {};

		static SdVec2 ClampSize(SdVec2 size, const SdLayoutConstraints& constraints) noexcept
		{
			size.x = std::max(size.x, constraints.minSize.x);
			size.y = std::max(size.y, constraints.minSize.y);
			if (constraints.maxSize.x > 0.0f)
				size.x = std::min(size.x, constraints.maxSize.x);
			if (constraints.maxSize.y > 0.0f)
				size.y = std::min(size.y, constraints.maxSize.y);
			return size;
		}

		static SdRect IntersectRect(const SdRect& left, const SdRect& right) noexcept
		{
			SdRect result = {
				std::max(left.min.x, right.min.x),
				std::max(left.min.y, right.min.y),
				std::min(left.max.x, right.max.x),
				std::min(left.max.y, right.max.y)
			};
			result.max.x = std::max(result.min.x, result.max.x);
			result.max.y = std::max(result.min.y, result.max.y);
			return result;
		}

		static SdRect InsetRect(const SdRect& rect, const SdSpacing& padding) noexcept
		{
			SdRect result = {
				rect.min.x + padding.left,
				rect.min.y + padding.top,
				rect.max.x - padding.right,
				rect.max.y - padding.bottom
			};
			result.max.x = std::max(result.min.x, result.max.x);
			result.max.y = std::max(result.min.y, result.max.y);
			return result;
		}

		bool ParentArrangesNode(const SdLayoutNode& node) const noexcept
		{
			return node.parentIndex != SdInvalidIndex<SdUInt32>
				&& node.parentIndex < nodes.size()
				&& nodes[node.parentIndex].arrangeChildren;
		}

		void SetNodeTarget(SdLayoutNode& node, const SdRect& targetRect, const SdRect& clipRect)
		{
			node.targetRect = targetRect;
			node.result.finalRect = targetRect;
			node.computedClipRect = clipRect;
			node.childContentRect = InsetRect(InsetRect(targetRect, node.childBorder), node.contentPadding);
		}

		void ArrangeChildren(SdUInt32 parentIndex)
		{
			SdLayoutNode& parent = nodes[parentIndex];
			const SdRect childClipRect = parent.clipChildren
				? IntersectRect(parent.computedClipRect, parent.childContentRect)
				: parent.computedClipRect;

			float childY = parent.childContentRect.min.y;
			for (SdUInt32 childIndex = parent.firstChildIndex;
				childIndex != SdInvalidIndex<SdUInt32>;
				childIndex = nodes[childIndex].nextSiblingIndex)
			{
				SdLayoutNode& child = nodes[childIndex];
				if (child.manualLayout)
				{
					SetNodeTarget(child, child.manualRect, childClipRect);
				}
				else
				{
					const float availableWidth = std::max(0.0f, parent.childContentRect.Width());
					const SdVec2 childSize = child.result.desiredSize;
					const float childWidth = std::min(childSize.x, availableWidth);
					const SdRect targetRect = {
						parent.childContentRect.min.x,
						childY,
						parent.childContentRect.min.x + childWidth,
						childY + childSize.y
					};
					SetNodeTarget(child, targetRect, childClipRect);
					childY += (childSize.y * child.layoutWeight) + parent.gap;
				}

				if (child.arrangeChildren)
					ArrangeChildren(childIndex);
			}
		}

	public:
		void BeginFrame(SdSize expectedNodeCount = 0)
		{
			nodes.clear();
			rootIndices.clear();
			lastChildIndices.clear();
			nodeIndexByWidgetId.clear();
			nodes.reserve(expectedNodeCount);
			rootIndices.reserve(expectedNodeCount);
			lastChildIndices.reserve(expectedNodeCount);
			nodeIndexByWidgetId.reserve(expectedNodeCount);
		}

		SdUInt32 AddNode(const SdLayoutNodeInput& input)
		{
			const SdUInt32 index = static_cast<SdUInt32>(nodes.size());
			SdLayoutNode& node = nodes.emplace_back();
			node.widgetId = input.widgetId;
			node.constraints = input.constraints;
			node.result = input.result;
			node.manualRect = input.manualRect;
			node.childBorder = input.childBorder;
			node.contentPadding = input.contentPadding;
			node.layoutWeight = input.layoutWeight;
			node.gap = input.gap;
			node.manualLayout = input.manualLayout;
			node.arrangeChildren = input.arrangeChildren;
			node.clipChildren = input.clipChildren;
			lastChildIndices.push_back(SdInvalidIndex<SdUInt32>);
			nodeIndexByWidgetId[input.widgetId] = index;

			auto parentIt = nodeIndexByWidgetId.find(input.parentId);
			if (input.parentId != 0 && parentIt != nodeIndexByWidgetId.end())
			{
				node.parentIndex = parentIt->second;
				SdLayoutNode& parent = nodes[node.parentIndex];
				if (parent.firstChildIndex == SdInvalidIndex<SdUInt32>)
				{
					parent.firstChildIndex = index;
				}
				else
				{
					nodes[lastChildIndices[node.parentIndex]].nextSiblingIndex = index;
				}
				lastChildIndices[node.parentIndex] = index;
			}
			else
			{
				rootIndices.push_back(index);
			}

			return index;
		}

		void Measure(SdVec2 displaySize)
		{
			for (SdLayoutNode& node : nodes)
			{
				if (node.constraints.maxSize.x <= 0.0f)
					node.constraints.maxSize.x = displaySize.x;
				if (node.constraints.maxSize.y <= 0.0f)
					node.constraints.maxSize.y = displaySize.y;
				node.result.desiredSize = ClampSize(node.result.desiredSize, node.constraints);
			}
		}

		void Arrange(const SdRect& displayRect)
		{
			for (SdLayoutNode& node : nodes)
			{
				node.targetRect = {};
				node.animatedRect = {};
				node.childContentRect = {};
				node.computedClipRect = displayRect;
			}

			float y = displayRect.min.y + 12.0f;
			for (SdUInt32 index = 0; index < nodes.size(); ++index)
			{
				SdLayoutNode& node = nodes[index];
				if (ParentArrangesNode(node))
					continue;

				if (node.manualLayout)
				{
					SetNodeTarget(node, node.manualRect, displayRect);
				}
				else
				{
					const SdVec2 desiredSize = node.result.desiredSize;
					const SdRect targetRect = {
						displayRect.min.x + 12.0f,
						y,
						displayRect.min.x + 12.0f + desiredSize.x,
						y + desiredSize.y
					};
					SetNodeTarget(node, targetRect, displayRect);
					y += (desiredSize.y * node.layoutWeight) + 8.0f;
				}

				if (node.arrangeChildren)
					ArrangeChildren(index);
			}
		}

		const std::vector<SdLayoutNode>& GetNodes() const noexcept
		{
			return nodes;
		}

		std::vector<SdLayoutNode>& GetNodes() noexcept
		{
			return nodes;
		}
	};
}
