#pragma once

#include "SdStyleProperty.h"

#include <algorithm>
#include <vector>

namespace Sodium
{
	struct SdPropertyAnimationChannel final
	{
		SdStyleNodeId styleNodeId = SdInvalidStyleNodeId;
		SdPropertyId propertyId = 0;
		SdStyleFieldImpact impact = SdStyleFieldImpact::Discrete;
		SdStyleInterpolation interpolation = SdStyleInterpolation::None;
		SdTransition transition = {};
		SdDuration delay = {};
		SdDuration elapsed = {};
		SdStyleValue startValue = {};
		SdStyleValue targetValue = {};
		SdStyleValue currentValue = {};
		bool active = false;
		bool expensiveLayout = false;
		bool discrete = false;
	};

	class SdStyleAnimationChannels final
	{
	private:
		std::vector<SdPropertyAnimationChannel> channels = {};

		static SdStyleValue InterpolateValue(
			const SdStyleValue& start,
			const SdStyleValue& target,
			SdStyleInterpolation interpolation,
			float amount) noexcept
		{
			const float t = std::clamp(amount, 0.0f, 1.0f);
			if (start.kind != target.kind)
				return target;

			switch (interpolation)
			{
			case SdStyleInterpolation::Float:
				if (start.kind == SdStyleValueKind::Float)
					return SdStyleValue::FromFloat(start.number + (target.number - start.number) * t);
				break;
			case SdStyleInterpolation::Color:
				if (start.kind == SdStyleValueKind::Color)
				{
					const auto lerpByte = [t](SdUInt8 left, SdUInt8 right) noexcept
					{
						const float value = static_cast<float>(left) + (static_cast<float>(right) - static_cast<float>(left)) * t;
						return static_cast<SdUInt8>(std::clamp(value, 0.0f, 255.0f));
					};
					return SdStyleValue::FromColor({
						lerpByte(start.color.r, target.color.r),
						lerpByte(start.color.g, target.color.g),
						lerpByte(start.color.b, target.color.b),
						lerpByte(start.color.a, target.color.a)
					});
				}
				break;
			case SdStyleInterpolation::Spacing:
				if (start.kind == SdStyleValueKind::Spacing)
				{
					return SdStyleValue::FromSpacing({
						start.spacing.left + (target.spacing.left - start.spacing.left) * t,
						start.spacing.top + (target.spacing.top - start.spacing.top) * t,
						start.spacing.right + (target.spacing.right - start.spacing.right) * t,
						start.spacing.bottom + (target.spacing.bottom - start.spacing.bottom) * t
					});
				}
				break;
			case SdStyleInterpolation::Vec2:
				if (start.kind == SdStyleValueKind::Vec2)
				{
					return SdStyleValue::FromVec2({
						start.vec2.x + (target.vec2.x - start.vec2.x) * t,
						start.vec2.y + (target.vec2.y - start.vec2.y) * t
					});
				}
				break;
			case SdStyleInterpolation::None:
			default:
				break;
			}
			return target;
		}

	public:
		SdPropertyAnimationChannel& Ensure(SdStyleNodeId styleNodeId, SdPropertyId propertyId)
		{
			for (SdPropertyAnimationChannel& channel : channels)
			{
				if (channel.styleNodeId == styleNodeId && channel.propertyId == propertyId)
					return channel;
			}

			SdPropertyAnimationChannel& channel = channels.emplace_back();
			channel.styleNodeId = styleNodeId;
			channel.propertyId = propertyId;
			return channel;
		}

		SdPropertyAnimationChannel* Find(SdStyleNodeId styleNodeId, SdPropertyId propertyId) noexcept
		{
			for (SdPropertyAnimationChannel& channel : channels)
			{
				if (channel.styleNodeId == styleNodeId && channel.propertyId == propertyId)
					return &channel;
			}
			return nullptr;
		}

		const SdPropertyAnimationChannel* Find(SdStyleNodeId styleNodeId, SdPropertyId propertyId) const noexcept
		{
			for (const SdPropertyAnimationChannel& channel : channels)
			{
				if (channel.styleNodeId == styleNodeId && channel.propertyId == propertyId)
					return &channel;
			}
			return nullptr;
		}

		SdUInt32 CountActive() const noexcept
		{
			SdUInt32 count = 0;
			for (const SdPropertyAnimationChannel& channel : channels)
			{
				if (channel.active)
					++count;
			}
			return count;
		}

		SdUInt32 CountActiveLayoutTransitions() const noexcept
		{
			SdUInt32 count = 0;
			for (const SdPropertyAnimationChannel& channel : channels)
			{
				if (channel.active && channel.impact == SdStyleFieldImpact::Layout)
					++count;
			}
			return count;
		}

		bool HasActiveStyleNode(SdStyleNodeId styleNodeId) const noexcept
		{
			for (const SdPropertyAnimationChannel& channel : channels)
			{
				if (channel.active && channel.styleNodeId == styleNodeId)
					return true;
			}
			return false;
		}

		bool HasActiveAny(SdStyleNodeId rootStyleNodeId, SdSpan<const SdStyleNodeId> partStyleNodeIds) const noexcept
		{
			if (HasActiveStyleNode(rootStyleNodeId))
				return true;
			for (SdStyleNodeId partStyleNodeId : partStyleNodeIds)
			{
				if (HasActiveStyleNode(partStyleNodeId))
					return true;
			}
			return false;
		}

		const std::vector<SdPropertyAnimationChannel>& GetChannels() const noexcept
		{
			return channels;
		}

		void Update(SdDuration deltaTime)
		{
			const float seconds = std::max(0.001f, static_cast<float>(deltaTime.count()) / 1000000000.0f);
			for (SdPropertyAnimationChannel& channel : channels)
			{
				if (!channel.active)
					continue;

				channel.elapsed += std::chrono::duration_cast<SdDuration>(std::chrono::duration<float>(seconds));
				const float durationSeconds = std::max(0.001f, static_cast<float>(channel.transition.duration.count()) / 1000000000.0f);
				const float elapsedSeconds = std::max(0.0f, static_cast<float>(channel.elapsed.count()) / 1000000000.0f);
				const float t = std::clamp(elapsedSeconds / durationSeconds, 0.0f, 1.0f);
				const float eased = SdApplyEasing(t, channel.transition.easing);
				channel.currentValue = InterpolateValue(channel.startValue, channel.targetValue, channel.interpolation, eased);
				channel.active = t < 1.0f;
				if (!channel.active)
					channel.currentValue = channel.targetValue;
			}
		}

		void RemoveStyleNode(SdStyleNodeId styleNodeId)
		{
			channels.erase(std::remove_if(channels.begin(), channels.end(), [styleNodeId](const SdPropertyAnimationChannel& channel)
			{
				return channel.styleNodeId == styleNodeId;
			}), channels.end());
		}
	};
}
