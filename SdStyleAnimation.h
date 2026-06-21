#pragma once

#include "SdStyleProperty.h"

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

		const std::vector<SdPropertyAnimationChannel>& GetChannels() const noexcept
		{
			return channels;
		}
	};
}
