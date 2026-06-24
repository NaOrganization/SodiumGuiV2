#pragma once

#include "Effects/SdBlurEffect.hpp"
#include "Effects/SdMaskEffect.hpp"

#include <algorithm>

namespace Sodium
{
	struct SdDropShadowEffect final : ISdEffect
	{
		SdVec2 offset = { 0.0f, 4.0f };
		float radius = 12.0f;
		float spread = 0.0f;
		SdColorRgba color = { 0, 0, 0, 128 };

		SdEffectType GetType() const noexcept override { return SdEffectType::DropShadow; }
		bool RequiresIsolatedLayer() const noexcept override { return true; }
		bool RequiresBackdropCapture() const noexcept override { return false; }

		SdRect ExpandBounds(const SdRect& sourceBounds) const noexcept override
		{
			const float expand = std::max(0.0f, radius) + std::max(0.0f, spread);
			return {
				sourceBounds.min.x - expand + std::min(0.0f, offset.x),
				sourceBounds.min.y - expand + std::min(0.0f, offset.y),
				sourceBounds.max.x + expand + std::max(0.0f, offset.x),
				sourceBounds.max.y + expand + std::max(0.0f, offset.y)
			};
		}

		void BuildGraph(SdEffectBuildContext& context) const override
		{
			if (!context.target.IsValid())
				return;

			if (!context.mask.IsValid())
			{
				SdMaskEffect maskEffect = {};
				maskEffect.BuildGraph(context);
			}

			Rhi::SdRenderGraphTexture temp = context.graph.CreateTexture({
				std::max(1u, context.pixelWidth),
				std::max(1u, context.pixelHeight),
				Rhi::SdTextureFormat::R8Unorm,
				Rhi::SdTextureUsage::RenderTarget | Rhi::SdTextureUsage::ShaderRead,
				1,
				true,
				"Sodium.DropShadow.BlurTemp"
			});
			Rhi::SdRenderGraphTexture blurred = context.graph.CreateTexture({
				std::max(1u, context.pixelWidth),
				std::max(1u, context.pixelHeight),
				Rhi::SdTextureFormat::R8Unorm,
				Rhi::SdTextureUsage::RenderTarget | Rhi::SdTextureUsage::ShaderRead,
				1,
				true,
				"Sodium.DropShadow.BlurredMask"
			});

			Rhi::SdRenderGraphPassHandle horizontal = context.graph.AddPass({ "Sodium.DropShadow.BlurHorizontal", Rhi::SdRenderGraphPassType::Fullscreen });
			context.graph.ReadTexture(horizontal, context.mask);
			context.graph.WriteTexture(horizontal, temp);

			Rhi::SdRenderGraphPassHandle vertical = context.graph.AddPass({ "Sodium.DropShadow.BlurVertical", Rhi::SdRenderGraphPassType::Fullscreen });
			context.graph.ReadTexture(vertical, temp);
			context.graph.WriteTexture(vertical, blurred);

			Rhi::SdRenderGraphPassHandle colorize = context.graph.AddPass({ "Sodium.DropShadow.Colorize", Rhi::SdRenderGraphPassType::Fullscreen });
			context.graph.ReadTexture(colorize, blurred);
			context.graph.WriteTexture(colorize, context.target);
		}
	};

	struct SdInnerShadowEffect final : ISdEffect
	{
		SdVec2 offset = { 0.0f, 2.0f };
		float radius = 8.0f;
		SdColorRgba color = { 0, 0, 0, 96 };

		SdEffectType GetType() const noexcept override { return SdEffectType::InnerShadow; }
		bool RequiresIsolatedLayer() const noexcept override { return true; }
		bool RequiresBackdropCapture() const noexcept override { return false; }
		SdRect ExpandBounds(const SdRect& sourceBounds) const noexcept override { return sourceBounds; }

		void BuildGraph(SdEffectBuildContext& context) const override
		{
			if (!context.mask.IsValid())
			{
				SdMaskEffect maskEffect = {};
				maskEffect.BuildGraph(context);
			}

			Rhi::SdRenderGraphPassHandle pass = context.graph.AddPass({ "Sodium.InnerShadow.Composite", Rhi::SdRenderGraphPassType::Fullscreen });
			context.graph.ReadTexture(pass, context.mask);
			context.graph.WriteTexture(pass, context.target);
		}
	};
}
