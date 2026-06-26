#pragma once

#include "Effects/SdEffect.hpp"

#include <algorithm>
#include <vector>

namespace Sodium
{
	enum class SdLayerMode : SdUInt8
	{
		Direct,
		Isolated,
		Backdrop,
		Masked
	};

	struct SdRenderLayerDesc final
	{
		SdLayerMode mode = SdLayerMode::Direct;
		SdRect bounds = {};
		SdRect expandedBounds = {};
		std::vector<SdEffectHandle> effects = {};
		bool requiresMask = false;
		bool requiresBackdrop = false;
		bool canBatchWithParent = true;
	};

	class SdLayerResolver final
	{
	public:
		static SdLayerMode ResolveLayerMode(SdSpan<const SdEffectLayerRequirements> requirements) noexcept
		{
			for (const SdEffectLayerRequirements& requirement : requirements)
			{
				if (requirement.requiresBackdrop)
					return SdLayerMode::Backdrop;
			}

			for (const SdEffectLayerRequirements& requirement : requirements)
			{
				if (requirement.requiresMask)
					return SdLayerMode::Masked;
				if (requirement.requiresIsolatedLayer)
					return SdLayerMode::Isolated;
			}

			return SdLayerMode::Direct;
		}

		static SdRenderLayerDesc BuildLayerDesc(
			SdRect bounds,
			SdSpan<const SdEffectLayerRequirements> requirements,
			SdSpan<const SdEffectHandle> effects = {})
		{
			SdRenderLayerDesc desc = {};
			desc.bounds = bounds;
			desc.expandedBounds = bounds;
			desc.mode = ResolveLayerMode(requirements);
			desc.requiresBackdrop = desc.mode == SdLayerMode::Backdrop;
			desc.requiresMask = desc.mode == SdLayerMode::Masked;
			desc.canBatchWithParent = desc.mode == SdLayerMode::Direct;
			desc.effects.assign(effects.begin(), effects.end());

			for (const SdEffectLayerRequirements& requirement : requirements)
			{
				if (requirement.expandedBounds.Width() > 0.0f && requirement.expandedBounds.Height() > 0.0f)
				{
					desc.expandedBounds =
					{
						std::min(desc.expandedBounds.min.x, requirement.expandedBounds.min.x),
						std::min(desc.expandedBounds.min.y, requirement.expandedBounds.min.y),
						std::max(desc.expandedBounds.max.x, requirement.expandedBounds.max.x),
						std::max(desc.expandedBounds.max.y, requirement.expandedBounds.max.y)
					};
				}
				desc.requiresBackdrop = desc.requiresBackdrop || requirement.requiresBackdrop;
				desc.requiresMask = desc.requiresMask || requirement.requiresMask;
				if (requirement.requiresIsolatedLayer)
					desc.canBatchWithParent = false;
			}
			return desc;
		}
	};
}
