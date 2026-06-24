#pragma once

#include "Effects/SdEffect.hpp"

#include <algorithm>

namespace Sodium
{
	struct SdMaskEffect final : ISdEffect
	{
		SdMaskType type = SdMaskType::Rectangle;
		SdRect bounds = {};
		float radius = 0.0f;
		SdSpan<const SdVec2> polygonPoints = {};
		Rhi::SdTextureHandle alphaTexture = {};

		SdEffectType GetType() const noexcept override { return SdEffectType::Mask; }
		bool RequiresIsolatedLayer() const noexcept override { return true; }
		bool RequiresBackdropCapture() const noexcept override { return false; }
		SdRect ExpandBounds(const SdRect& sourceBounds) const noexcept override { return sourceBounds; }

		void BuildGraph(SdEffectBuildContext& context) const override
		{
			if (!context.mask.IsValid())
				context.mask = context.graph.CreateTexture({
					std::max(1u, context.pixelWidth),
					std::max(1u, context.pixelHeight),
					Rhi::SdTextureFormat::R8Unorm,
					Rhi::SdTextureUsage::RenderTarget | Rhi::SdTextureUsage::ShaderRead,
					1,
					true,
					"Sodium.Mask.Output"
				});

			Rhi::SdRenderGraphPassHandle pass = context.graph.AddPass({
				type == SdMaskType::Polygon ? "Sodium.Mask.Polygon" : "Sodium.Mask.RoundedRect",
				Rhi::SdRenderGraphPassType::Raster
			});
			context.graph.WriteTexture(pass, context.mask);
		}
	};
}
