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

	struct SdCustomEffect final : ISdEffect
	{
		SdCustomEffectDesc desc = {};

		SdEffectType GetType() const noexcept override { return SdEffectType::Custom; }
		bool RequiresIsolatedLayer() const noexcept override { return desc.requiresIsolatedLayer; }
		bool RequiresBackdropCapture() const noexcept override { return desc.requiresBackdropCapture; }
		SdRect ExpandBounds(const SdRect& sourceBounds) const noexcept override { return sourceBounds; }

		void BuildGraph(SdEffectBuildContext& context) const override
		{
			for (const SdEffectPassDesc& effectPass : desc.passes)
			{
				const Rhi::SdRenderGraphPassType graphPassType = effectPass.type == SdEffectPassType::Copy
					? Rhi::SdRenderGraphPassType::Copy
					: (effectPass.type == SdEffectPassType::Compute ? Rhi::SdRenderGraphPassType::Compute : Rhi::SdRenderGraphPassType::Fullscreen);
				Rhi::SdRenderGraphPassHandle pass = context.graph.AddPass({ effectPass.name, graphPassType });
				if (context.source.IsValid())
					context.graph.ReadTexture(pass, context.source);
				if (context.target.IsValid())
					context.graph.WriteTexture(pass, context.target);
			}
		}
	};
}
