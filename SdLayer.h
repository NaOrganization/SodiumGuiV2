#pragma once

#include "SdInput.h"
#include "SdUiCore.h"

#include <algorithm>
#include <vector>

namespace Sodium
{
	struct SdHitTestRecord final
	{
		SdWidgetId widgetId = 0;
		SdLayerPriority layerPriority = SdLayerPriority::Content;
		SdRect rect = {};
		SdRect clipRect = {};
		SdUInt32 paintOrder = 0;
		bool inputEnabled = true;
	};

	class SdLayerSystem final
	{
	private:
		std::vector<SdHitTestRecord> hitTestRecords = {};

		static SdRect IntersectRect(const SdRect& left, const SdRect& right)
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

	public:
		void BeginFrame()
		{
			hitTestRecords.clear();
		}

		void AddHitTestRecord(const SdHitTestRecord& record)
		{
			if (record.inputEnabled)
				hitTestRecords.push_back(record);
		}

		SdWidgetId HitTest(const SdVec2& point) const
		{
			const SdHitTestRecord* best = nullptr;
			for (const SdHitTestRecord& record : hitTestRecords)
			{
				const SdRect clipped = IntersectRect(record.rect, record.clipRect);
				if (!clipped.Contains(point))
					continue;
				if (!best)
				{
					best = &record;
					continue;
				}
				if (static_cast<SdUInt8>(record.layerPriority) > static_cast<SdUInt8>(best->layerPriority))
				{
					best = &record;
					continue;
				}
				if (record.layerPriority == best->layerPriority && record.paintOrder > best->paintOrder)
					best = &record;
			}
			return best ? best->widgetId : 0;
		}

		const std::vector<SdHitTestRecord>& GetHitTestRecords() const noexcept
		{
			return hitTestRecords;
		}
	};

	struct SdInteractionState final
	{
		SdWidgetId hoveredWidget = 0;
		SdWidgetId activeWidget = 0;
		SdWidgetId focusedWidget = 0;
		SdWidgetId capturedWidget = 0;
		SdMouseButton pressedButton = SdMouseButton::Left;
		bool clicked = false;
	};

	class SdInteractionSystem final
	{
	private:
		SdInteractionState state = {};

	public:
		void Update(const SdLayerSystem& layerSystem, const SdInputSnapshot& input)
		{
			state.clicked = false;
			const SdWidgetId hitWidget = layerSystem.HitTest(input.GetMousePosition());
			state.hoveredWidget = state.capturedWidget != 0 ? state.capturedWidget : hitWidget;

			if (input.IsMouseButtonDown(SdMouseButton::Left) && hitWidget != 0)
			{
				state.activeWidget = hitWidget;
				state.capturedWidget = hitWidget;
				state.focusedWidget = hitWidget;
				state.pressedButton = SdMouseButton::Left;
			}

			if (input.IsMouseButtonUp(SdMouseButton::Left))
			{
				state.clicked = state.activeWidget != 0 && state.activeWidget == hitWidget;
				state.capturedWidget = 0;
				state.activeWidget = 0;
			}
		}

		const SdInteractionState& GetState() const noexcept
		{
			return state;
		}

		bool IsHovered(SdWidgetId widgetId) const noexcept
		{
			return state.hoveredWidget == widgetId;
		}

		bool IsPressed(SdWidgetId widgetId) const noexcept
		{
			return state.activeWidget == widgetId || state.capturedWidget == widgetId;
		}

		bool WasClicked(SdWidgetId widgetId) const noexcept
		{
			return state.clicked && state.hoveredWidget == widgetId;
		}

		bool IsFocused(SdWidgetId widgetId) const noexcept
		{
			return state.focusedWidget == widgetId;
		}
	};
}
