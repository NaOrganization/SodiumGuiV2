#pragma once

#include "Effects/SdEffect.hpp"

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
		static SdLayerMode ResolveLayerMode(SdSpan<const ISdEffect* const> effects) noexcept
		{
			for (const ISdEffect* effect : effects)
			{
				if (!effect)
					continue;
				if (effect->RequiresBackdropCapture())
					return SdLayerMode::Backdrop;
			}

			for (const ISdEffect* effect : effects)
			{
				if (!effect)
					continue;
				if (effect->GetType() == SdEffectType::Mask)
					return SdLayerMode::Masked;
				if (effect->RequiresIsolatedLayer())
					return SdLayerMode::Isolated;
			}

			return SdLayerMode::Direct;
		}

		static SdRenderLayerDesc BuildLayerDesc(SdRect bounds, SdSpan<const ISdEffect* const> effects)
		{
			SdRenderLayerDesc desc = {};
			desc.bounds = bounds;
			desc.expandedBounds = bounds;
			desc.mode = ResolveLayerMode(effects);
			desc.requiresBackdrop = desc.mode == SdLayerMode::Backdrop;
			desc.requiresMask = desc.mode == SdLayerMode::Masked;
			desc.canBatchWithParent = desc.mode == SdLayerMode::Direct;

			for (const ISdEffect* effect : effects)
			{
				if (!effect)
					continue;
				desc.expandedBounds = effect->ExpandBounds(desc.expandedBounds);
				desc.requiresBackdrop = desc.requiresBackdrop || effect->RequiresBackdropCapture();
				desc.requiresMask = desc.requiresMask || effect->GetType() == SdEffectType::Mask;
				if (effect->RequiresIsolatedLayer())
					desc.canBatchWithParent = false;
			}
			return desc;
		}
	};
}
