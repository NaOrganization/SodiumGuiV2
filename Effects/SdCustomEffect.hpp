#pragma once

#include "Effects/SdEffect.hpp"

namespace Sodium
{
	struct SdEffectPassDesc final
	{
		SdUtf8String name = {};
		SdEffectPassType type = SdEffectPassType::Fullscreen;
		Rhi::SdPipelineHandle pipeline = {};
		Rhi::SdResourceSetLayoutHandle resourceLayout = {};
		std::vector<SdUtf8String> inputs = {};
		std::vector<SdUtf8String> outputs = {};
	};

	struct SdCustomEffectDesc final
	{
		SdUtf8String name = {};
		std::vector<SdEffectPassDesc> passes = {};
		bool requiresIsolatedLayer = true;
		bool requiresBackdropCapture = false;
	};

	struct SdCustomEffect final : ISdEffector
	{
		SdCustomEffectDesc desc = {};

		SdEffectTypeId GetTypeId() const noexcept override { return SdCustomEffectTypeId; }
		bool Initialize(const SdEffectInitContext&) override { return true; }
		void Shutdown(Rhi::ISdGpuDevice&) noexcept override {}

		SdEffectLayerRequirements QueryLayerRequirements(const SdEffectCommandView& command) const noexcept override
		{
			SdEffectLayerRequirements requirements = {};
			requirements.requiresSource = true;
			requirements.requiresTarget = true;
			requirements.requiresBackdrop = desc.requiresBackdropCapture;
			requirements.requiresIsolatedLayer = desc.requiresIsolatedLayer;
			requirements.expandedBounds = command.payload.sourceBounds;
			return requirements;
		}

		bool Apply(const SdEffectApplyContext&) override
		{
			return false;
		}
	};
}
