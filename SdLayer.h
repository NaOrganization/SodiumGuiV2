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

	struct SdLayerDrawRecord final
	{
		SdWidgetId widgetId = 0;
		SdLayerPriority layerPriority = SdLayerPriority::Content;
		SdRect clipRect = {};
		SdUInt32 paintOrder = 0;
	};

	struct SdLayerDrawChannel final
	{
		SdLayerPriority layerPriority = SdLayerPriority::Content;
		SdUInt32 firstRecord = 0;
		SdUInt32 recordCount = 0;
	};

	class SdLayerSystem final
	{
	private:
		std::vector<SdLayerDrawRecord> drawRecords = {};
		std::vector<SdLayerDrawChannel> drawChannels = {};
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
			drawRecords.clear();
			drawChannels.clear();
			hitTestRecords.clear();
		}

		void AddDrawRecord(const SdLayerDrawRecord& record)
		{
			if (drawChannels.empty() || drawChannels.back().layerPriority != record.layerPriority)
			{
				drawChannels.push_back({
					record.layerPriority,
					static_cast<SdUInt32>(drawRecords.size()),
					0
				});
			}
			drawRecords.push_back(record);
			++drawChannels.back().recordCount;
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

		const std::vector<SdLayerDrawRecord>& GetDrawRecords() const noexcept
		{
			return drawRecords;
		}

		const std::vector<SdLayerDrawChannel>& GetDrawChannels() const noexcept
		{
			return drawChannels;
		}
	};

	struct SdInteractionState final
	{
		SdWidgetId hoveredWidget = 0;
		SdWidgetId activeWidget = 0;
		SdWidgetId focusedWidget = 0;
		SdWidgetId capturedWidget = 0;
		SdWidgetId pressedWidget = 0;
		SdWidgetId releasedWidget = 0;
		SdWidgetId clickedWidget = 0;
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
			state.pressedWidget = 0;
			state.releasedWidget = 0;
			state.clickedWidget = 0;
			const SdWidgetId hitWidget = layerSystem.HitTest(input.GetMousePosition());
			state.hoveredWidget = state.capturedWidget != 0 ? state.capturedWidget : hitWidget;

			if (input.IsMouseButtonDown(SdMouseButton::Left) && hitWidget != 0)
			{
				state.activeWidget = hitWidget;
				state.capturedWidget = hitWidget;
				state.focusedWidget = hitWidget;
				state.pressedWidget = hitWidget;
				state.pressedButton = SdMouseButton::Left;
			}

			if (input.IsMouseButtonUp(SdMouseButton::Left))
			{
				state.releasedWidget = state.activeWidget;
				state.clickedWidget = state.activeWidget != 0 && state.activeWidget == hitWidget ? state.activeWidget : 0;
				state.clicked = state.clickedWidget != 0;
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
			return state.clickedWidget == widgetId;
		}

		bool IsFocused(SdWidgetId widgetId) const noexcept
		{
			return state.focusedWidget == widgetId;
		}

		bool WasPressed(SdWidgetId widgetId) const noexcept
		{
			return state.pressedWidget == widgetId;
		}

		bool WasReleased(SdWidgetId widgetId) const noexcept
		{
			return state.releasedWidget == widgetId;
		}

		bool IsCaptured(SdWidgetId widgetId) const noexcept
		{
			return state.capturedWidget == widgetId;
		}
	};
}
