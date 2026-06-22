#pragma once

#include "SdStyleProperty.h"

#include <algorithm>
#include <unordered_map>
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

	struct SdStyleAnimationChannelStats final
	{
		SdUInt32 ensureCount = 0;
		SdUInt32 ensureHitCount = 0;
		SdUInt32 findCount = 0;
		SdUInt32 findHitCount = 0;
		SdUInt32 channelCreatedCount = 0;
		SdUInt32 indexRebuildCount = 0;
		SdUInt32 targetSetCount = 0;
		SdUInt32 targetSetNoopCount = 0;
		SdUInt32 updateVisitedCount = 0;
		SdUInt32 updateActiveCount = 0;

		void ResetFrameTransient() noexcept
		{
			ensureCount = 0;
			ensureHitCount = 0;
			findCount = 0;
			findHitCount = 0;
			channelCreatedCount = 0;
			indexRebuildCount = 0;
			targetSetCount = 0;
			targetSetNoopCount = 0;
			updateVisitedCount = 0;
			updateActiveCount = 0;
		}
	};

	class SdStyleAnimationChannels final
	{
	private:
		struct ChannelKey final
		{
			SdStyleNodeId styleNodeId = SdInvalidStyleNodeId;
			SdPropertyId propertyId = 0;

			friend bool operator==(const ChannelKey& left, const ChannelKey& right) noexcept
			{
				return left.styleNodeId == right.styleNodeId && left.propertyId == right.propertyId;
			}
		};

		struct ChannelKeyHash final
		{
			SdSize operator()(const ChannelKey& key) const noexcept
			{
				SdSize hash = static_cast<SdSize>(key.styleNodeId);
				hash ^= static_cast<SdSize>(key.propertyId + 0x9e3779b97f4a7c15ull + (hash << 6) + (hash >> 2));
				return hash;
			}
		};

		std::vector<SdPropertyAnimationChannel> channels = {};
		std::unordered_map<ChannelKey, SdSize, ChannelKeyHash> channelIndexByKey = {};
		mutable SdStyleAnimationChannelStats frameStats = {};

		void RebuildIndex()
		{
			channelIndexByKey.clear();
			channelIndexByKey.reserve(channels.size());
			for (SdSize index = 0; index < channels.size(); ++index)
			{
				channelIndexByKey.emplace(ChannelKey{
					channels[index].styleNodeId,
					channels[index].propertyId
				}, index);
			}
			++frameStats.indexRebuildCount;
		}

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
			++frameStats.ensureCount;
			const ChannelKey key{ styleNodeId, propertyId };
			const auto it = channelIndexByKey.find(key);
			if (it != channelIndexByKey.end() && it->second < channels.size())
			{
				SdPropertyAnimationChannel& channel = channels[it->second];
				if (channel.styleNodeId == styleNodeId && channel.propertyId == propertyId)
				{
					++frameStats.ensureHitCount;
					return channel;
				}

				RebuildIndex();
				const auto rebuiltIt = channelIndexByKey.find(key);
				if (rebuiltIt != channelIndexByKey.end() && rebuiltIt->second < channels.size())
				{
					++frameStats.ensureHitCount;
					return channels[rebuiltIt->second];
				}
			}

			SdPropertyAnimationChannel& channel = channels.emplace_back();
			channel.styleNodeId = styleNodeId;
			channel.propertyId = propertyId;
			channelIndexByKey.emplace(key, channels.size() - 1);
			++frameStats.channelCreatedCount;
			return channel;
		}

		SdPropertyAnimationChannel* Find(SdStyleNodeId styleNodeId, SdPropertyId propertyId) noexcept
		{
			++frameStats.findCount;
			const ChannelKey key{ styleNodeId, propertyId };
			const auto it = channelIndexByKey.find(key);
			if (it == channelIndexByKey.end() || it->second >= channels.size())
				return nullptr;
			SdPropertyAnimationChannel& channel = channels[it->second];
			if (channel.styleNodeId == styleNodeId && channel.propertyId == propertyId)
			{
				++frameStats.findHitCount;
				return &channel;
			}
			return nullptr;
		}

		const SdPropertyAnimationChannel* Find(SdStyleNodeId styleNodeId, SdPropertyId propertyId) const noexcept
		{
			++frameStats.findCount;
			const ChannelKey key{ styleNodeId, propertyId };
			const auto it = channelIndexByKey.find(key);
			if (it == channelIndexByKey.end() || it->second >= channels.size())
				return nullptr;
			const SdPropertyAnimationChannel& channel = channels[it->second];
			if (channel.styleNodeId == styleNodeId && channel.propertyId == propertyId)
			{
				++frameStats.findHitCount;
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

		const SdStyleAnimationChannelStats& GetFrameStats() const noexcept
		{
			return frameStats;
		}

		void ResetFrameStats() noexcept
		{
			frameStats.ResetFrameTransient();
		}

		void RecordTargetSet(bool noop) noexcept
		{
			++frameStats.targetSetCount;
			if (noop)
				++frameStats.targetSetNoopCount;
		}

		void Update(SdDuration deltaTime)
		{
			const float seconds = std::max(0.001f, static_cast<float>(deltaTime.count()) / 1000000000.0f);
			for (SdPropertyAnimationChannel& channel : channels)
			{
				++frameStats.updateVisitedCount;
				if (!channel.active)
					continue;
				++frameStats.updateActiveCount;

				channel.elapsed += std::chrono::duration_cast<SdDuration>(std::chrono::duration<float>(seconds));
				if (channel.elapsed < channel.delay)
				{
					channel.currentValue = channel.startValue;
					continue;
				}

				const float durationSeconds = std::max(0.001f, static_cast<float>(channel.transition.duration.count()) / 1000000000.0f);
				const SdDuration delayedElapsed = channel.elapsed - channel.delay;
				const float elapsedSeconds = std::max(0.0f, static_cast<float>(delayedElapsed.count()) / 1000000000.0f);
				const float t = std::clamp(elapsedSeconds / durationSeconds, 0.0f, 1.0f);
				if (channel.discrete)
				{
					channel.active = t < 1.0f;
					channel.currentValue = channel.active ? channel.startValue : channel.targetValue;
					continue;
				}

				const float eased = SdApplyEasing(t, channel.transition.easing);
				channel.currentValue = InterpolateValue(channel.startValue, channel.targetValue, channel.interpolation, eased);
				channel.active = t < 1.0f;
				if (!channel.active)
					channel.currentValue = channel.targetValue;
			}
		}

		void RemoveStyleNode(SdStyleNodeId styleNodeId)
		{
			const SdSize previousSize = channels.size();
			channels.erase(std::remove_if(channels.begin(), channels.end(), [styleNodeId](const SdPropertyAnimationChannel& channel)
			{
				return channel.styleNodeId == styleNodeId;
			}), channels.end());
			if (channels.size() != previousSize)
				RebuildIndex();
		}
	};
}
