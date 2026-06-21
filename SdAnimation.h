#pragma once

#include "SdUiCore.h"

#include <algorithm>
#include <cassert>
#include <vector>

namespace Sodium
{
	enum class SdAnimationChannelKind : SdUInt8
	{
		LayoutWeight,
		Opacity,
		RectPosition,
		RectSize,
		StyleColor,
		ScrollOffset
	};

	using SdAnimationChannelId = SdUInt32;

	struct SdAnimationChannel final
	{
		SdAnimationChannelId id = 0;
		SdWidgetId widgetId = 0;
		SdAnimationChannelKind kind = SdAnimationChannelKind::LayoutWeight;
		float startValue = 0.0f;
		float value = 0.0f;
		float target = 0.0f;
		SdDuration elapsed = {};
		SdTransition transition = {};
		bool active = false;
	};

	struct SdAnimationWidgetState final
	{
		SdAnimationChannelId layoutWeight = 0;
		SdAnimationChannelId opacity = 0;
		SdAnimationChannelId rectX = 0;
		SdAnimationChannelId rectY = 0;
		SdAnimationChannelId rectWidth = 0;
		SdAnimationChannelId rectHeight = 0;
		SdAnimationChannelId scrollOffset = 0;
	};

	class SdAnimationSystem final
	{
	private:
		std::vector<SdAnimationChannel> channels = {};
		std::vector<SdAnimationChannelId> activeChannels = {};
		SdAnimationChannelId nextChannelId = 1;

		SdAnimationChannel* FindChannel(SdAnimationChannelId id) noexcept
		{
			for (SdAnimationChannel& channel : channels)
			{
				if (channel.id == id)
					return &channel;
			}
			return nullptr;
		}

		SdAnimationChannel& CreateChannel(SdWidgetId widgetId, SdAnimationChannelKind kind, float initialValue)
		{
			SdAnimationChannel& channel = channels.emplace_back();
			channel.id = nextChannelId++;
			channel.widgetId = widgetId;
			channel.kind = kind;
			channel.startValue = initialValue;
			channel.value = initialValue;
			channel.target = initialValue;
			return channel;
		}

		void SetTarget(SdAnimationChannel& channel, float target, const SdTransition& transition)
		{
			if (channel.target == target && channel.transition.duration == transition.duration && channel.transition.easing == transition.easing)
				return;
			channel.startValue = channel.value;
			channel.target = target;
			channel.elapsed = {};
			channel.transition = transition;
			channel.active = channel.value != channel.target;
			if (channel.active && std::find(activeChannels.begin(), activeChannels.end(), channel.id) == activeChannels.end())
				activeChannels.push_back(channel.id);
		}

		static bool ShouldUpdateKind(
			SdAnimationChannelKind kind,
			bool updateLayoutWeight,
			bool updateOpacity,
			bool updateRectPosition,
			bool updateRectSize,
			bool updateStyleColor,
			bool updateScrollOffset) noexcept
		{
			switch (kind)
			{
			case SdAnimationChannelKind::LayoutWeight:
				return updateLayoutWeight;
			case SdAnimationChannelKind::Opacity:
				return updateOpacity;
			case SdAnimationChannelKind::RectPosition:
				return updateRectPosition;
			case SdAnimationChannelKind::RectSize:
				return updateRectSize;
			case SdAnimationChannelKind::StyleColor:
				return updateStyleColor;
			case SdAnimationChannelKind::ScrollOffset:
				return updateScrollOffset;
			default:
				return false;
			}
		}

	public:
		SdAnimationChannelId EnsureChannel(SdWidgetId widgetId, SdAnimationChannelKind kind, float initialValue)
		{
			SdAnimationChannel& channel = CreateChannel(widgetId, kind, initialValue);
			return channel.id;
		}

		SdAnimationChannel& GetChannel(SdAnimationChannelId id)
		{
			SdAnimationChannel* channel = FindChannel(id);
			assert(channel);
			return *channel;
		}

		float GetValue(SdAnimationChannelId id, float fallback = 0.0f) noexcept
		{
			SdAnimationChannel* channel = FindChannel(id);
			return channel ? channel->value : fallback;
		}

		void SetTarget(SdAnimationChannelId id, float target, const SdTransition& transition = {})
		{
			if (SdAnimationChannel* channel = FindChannel(id))
				SetTarget(*channel, target, transition);
		}

		void SetImmediate(SdAnimationChannelId id, float value)
		{
			if (SdAnimationChannel* channel = FindChannel(id))
			{
				channel->value = value;
				channel->startValue = value;
				channel->target = value;
				channel->elapsed = {};
				channel->active = false;
				activeChannels.erase(std::remove(activeChannels.begin(), activeChannels.end(), id), activeChannels.end());
			}
		}

		bool IsActive(SdAnimationChannelId id) noexcept
		{
			SdAnimationChannel* channel = FindChannel(id);
			return channel ? channel->active : false;
		}

		void Update(
			SdDuration deltaTime,
			bool updateLayoutWeight = true,
			bool updateOpacity = true,
			bool updateRectPosition = true,
			bool updateRectSize = true,
			bool updateStyleColor = true,
			bool updateScrollOffset = true)
		{
			const float seconds = std::max(0.001f, static_cast<float>(deltaTime.count()) / 1000000000.0f);
			for (SdSize i = 0; i < activeChannels.size();)
			{
				SdAnimationChannel* channel = FindChannel(activeChannels[i]);
				if (!channel)
				{
					activeChannels[i] = activeChannels.back();
					activeChannels.pop_back();
					continue;
				}
				if (!ShouldUpdateKind(channel->kind, updateLayoutWeight, updateOpacity, updateRectPosition, updateRectSize, updateStyleColor, updateScrollOffset))
				{
					++i;
					continue;
				}

				channel->elapsed += std::chrono::duration_cast<SdDuration>(std::chrono::duration<float>(seconds));
				const float durationSeconds = std::max(0.001f, static_cast<float>(channel->transition.duration.count()) / 1000000000.0f);
				const float elapsedSeconds = std::max(0.0f, static_cast<float>(channel->elapsed.count()) / 1000000000.0f);
				const float t = std::clamp(elapsedSeconds / durationSeconds, 0.0f, 1.0f);
				const float eased = SdApplyEasing(t, channel->transition.easing);
				channel->value = channel->startValue + (channel->target - channel->startValue) * eased;
				channel->active = t < 1.0f && channel->value != channel->target;
				if (!channel->active)
				{
					channel->value = channel->target;
					activeChannels[i] = activeChannels.back();
					activeChannels.pop_back();
					continue;
				}
				++i;
			}
		}

		SdSize GetActiveChannelCount() const noexcept
		{
			return activeChannels.size();
		}

		void RemoveWidget(SdWidgetId widgetId)
		{
			channels.erase(std::remove_if(channels.begin(), channels.end(), [widgetId](const SdAnimationChannel& channel)
			{
				return channel.widgetId == widgetId;
			}), channels.end());
			activeChannels.erase(std::remove_if(activeChannels.begin(), activeChannels.end(), [this](SdAnimationChannelId id)
			{
				return FindChannel(id) == nullptr;
			}), activeChannels.end());
		}
	};
}
