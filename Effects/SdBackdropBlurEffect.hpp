#pragma once

#include "Effects/SdBlurEffect.hpp"

namespace Sodium
{
	struct SdBackdropBlurEffect final : ISdEffector
	{
		float radius = 16.0f;
		SdColorRgba tintColor = { 255, 255, 255, 48 };
		float saturation = 1.2f;
		float brightness = 1.0f;
		SdEffectResourceCache resources = {};

		SdEffectTypeId GetTypeId() const noexcept override { return SdBackdropBlurEffectTypeId; }

		bool Initialize(const SdEffectInitContext& context) override
		{
			return BlurEffectDetail::InitializeResources(context.device, resources);
		}

		void Shutdown(Rhi::ISdGpuDevice& device) noexcept override
		{
			resources.Shutdown(device);
		}

		SdEffectLayerRequirements QueryLayerRequirements(const SdEffectCommandView& command) const noexcept override
		{
			SdEffectLayerRequirements requirements = {};
			requirements.requiresSource = false;
			requirements.requiresTarget = true;
			requirements.requiresBackdrop = true;
			requirements.requiresIsolatedLayer = true;
			requirements.expandedBounds = command.payload.sourceBounds;
			return requirements;
		}

		bool Apply(const SdEffectApplyContext& context) override
		{
			if (!context.layers)
				return false;

			SdBackdropBlurEffectParameters parameters = {};
			parameters.radius = radius;
			parameters.tintColor = tintColor;
			parameters.saturation = saturation;
			parameters.brightness = brightness;
			if (const SdBackdropBlurEffectParameters* commandParameters = SdTryReadEffectParameters<SdBackdropBlurEffectParameters>(context.parameters))
				parameters = *commandParameters;
			if (parameters.radius <= 0.0f)
				return false;

			const SdEffectRenderLayer targetLayer = context.layers->FindRenderLayer(context.payload.targetLayer);
			if (!targetLayer.IsValid())
				return false;

			const SdRect frameRect = BlurEffectDetail::ResolveLayerBounds(targetLayer);
			SdRect clippedRect = context.payload.sourceBounds;
			if (BlurEffectDetail::HasArea(context.payload.clipRect))
				clippedRect = BlurEffectDetail::IntersectRect(clippedRect, context.payload.clipRect);
			clippedRect = BlurEffectDetail::IntersectRect(clippedRect, frameRect);
			if (!BlurEffectDetail::HasArea(clippedRect))
				return false;

			const float capturePadding = std::ceil(std::max(0.0f, parameters.radius));
			const SdRect captureBounds = BlurEffectDetail::IntersectRect(
				{
					clippedRect.min.x - capturePadding,
					clippedRect.min.y - capturePadding,
					clippedRect.max.x + capturePadding,
					clippedRect.max.y + capturePadding
				},
				frameRect);
			const Rhi::SdRectI sourceRect = BlurEffectDetail::ToRenderArea(captureBounds);
			const SdUInt32 captureWidth = std::max(1u, sourceRect.Width());
			const SdUInt32 captureHeight = std::max(1u, sourceRect.Height());
			const SdRect capturePixelBounds =
			{
				static_cast<float>(sourceRect.left),
				static_cast<float>(sourceRect.top),
				static_cast<float>(sourceRect.right),
				static_cast<float>(sourceRect.bottom)
			};

			const Rhi::SdTextureDesc captureDesc =
			{
				captureWidth,
				captureHeight,
				1,
				1,
				BlurEffectDetail::ResolveLayerFormat(targetLayer),
				Rhi::SdTextureUsage::ShaderRead | Rhi::SdTextureUsage::CopyDst,
				1,
				false,
				SODIUM_STRING("Sodium.Effect.BackdropBlur.Capture")
			};
			const Rhi::SdTextureHandle captureTexture = resources.GetOrCreateTexture(context.device, SdEffectResourceCache::TextureSlot::BackdropCapture, captureDesc);
			if (!captureTexture.IsValid())
				return false;
			if (!context.device.CopyCurrentRenderTargetToTexture(captureTexture, sourceRect))
				return false;

			const SdEffectRenderLayer sourceLayer =
			{
				SdInvalidRenderLayerId,
				captureTexture,
				captureDesc.format,
				captureWidth,
				captureHeight,
				capturePixelBounds
			};
			const SdColorLinear tintColor = parameters.tintColor.ToLinear();
			const bool applied = BlurEffectDetail::ApplyBlur(
				context,
				resources,
				sourceLayer,
				targetLayer,
				parameters.radius,
				context.payload.sourceBounds,
				capturePixelBounds,
				context.payload.clipRect,
				BlurEffectDetail::NormalizeCornerRadii(parameters.cornerRadii, parameters.cornerRadius),
				{ tintColor.r, tintColor.g, tintColor.b, tintColor.a },
				parameters.saturation,
				parameters.brightness);
			return applied;
		}
	};
}
