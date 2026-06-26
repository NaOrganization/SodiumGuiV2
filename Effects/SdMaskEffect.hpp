#pragma once

#include "Effects/SdEffect.hpp"

#include <algorithm>

namespace Sodium
{
	struct SdMaskEffect final : ISdEffector
	{
		SdMaskType type = SdMaskType::Rectangle;
		SdRect bounds = {};
		float radius = 0.0f;
		SdSpan<const SdVec2> polygonPoints = {};
		Rhi::SdTextureHandle alphaTexture = {};

		SdEffectTypeId GetTypeId() const noexcept override { return SdMaskEffectTypeId; }

		bool Initialize(const SdEffectInitContext&) override { return true; }
		void Shutdown(Rhi::ISdGpuDevice&) noexcept override {}

		SdEffectLayerRequirements QueryLayerRequirements(const SdEffectCommandView& command) const noexcept override
		{
			SdEffectLayerRequirements requirements = {};
			requirements.requiresSource = true;
			requirements.requiresTarget = true;
			requirements.requiresMask = true;
			requirements.requiresIsolatedLayer = true;
			requirements.expandedBounds = command.payload.sourceBounds;
			return requirements;
		}

		bool Apply(const SdEffectApplyContext&) override
		{
			return false;
		}
	};
}
