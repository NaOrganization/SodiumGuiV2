#pragma once

#include "Effects/SdBlurEffect.hpp"

namespace Sodium
{
	struct SdBackdropBlurEffect final : ISdEffect
	{
		float radius = 16.0f;
		SdColorRgba tintColor = { 255, 255, 255, 48 };
		float saturation = 1.2f;
		float brightness = 1.0f;

		SdEffectType GetType() const noexcept override { return SdEffectType::BackdropBlur; }
		bool RequiresIsolatedLayer() const noexcept override { return true; }
		bool RequiresBackdropCapture() const noexcept override { return true; }
		SdRect ExpandBounds(const SdRect& sourceBounds) const noexcept override { return sourceBounds; }

		void BuildGraph(SdEffectBuildContext& context) const override
		{
			if (!context.backdrop.IsValid() || !context.target.IsValid())
				return;

			Rhi::SdRenderGraphTexture captured = context.graph.CreateTexture(SdMakeEffectTextureDesc(
				std::max(1u, context.pixelWidth),
				std::max(1u, context.pixelHeight),
				Rhi::SdTextureFormat::Rgba8Unorm,
				"Sodium.BackdropBlur.Capture"));

			Rhi::SdRenderGraphPassHandle capturePass = context.graph.AddPass({ "Sodium.BackdropBlur.Capture", Rhi::SdRenderGraphPassType::Copy });
			context.graph.ReadTexture(capturePass, context.backdrop);
			context.graph.WriteTexture(capturePass, captured);

			SdEffectBuildContext blurContext = context;
			blurContext.source = captured;
			SdBlurEffect blur = {};
			blur.radius = radius;
			blur.BuildGraph(blurContext);
		}
	};
}
