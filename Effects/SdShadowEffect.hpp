#pragma once

#include "Effects/SdBlurEffect.hpp"
#include "Effects/SdMaskEffect.hpp"

#include <algorithm>
#include <array>

namespace Sodium
{
	namespace ShadowEffectDetail
	{
		inline constexpr const char* kShadowPixelShader = SODIUM_STRING(R"(
			cbuffer ShadowParams : register(b0)
			{
				float2 rectMin;
				float2 rectMax;
				float2 offset;
				float radius;
				float blurRadius;
				float spread;
				float outerOnly;
				float shadowMode;
				float padding0;
				float4 color;
				float4 cornerRadii;
			};

			struct PSInput
			{
				float4 position : SV_POSITION;
				float2 uv : TEXCOORD0;
			};

			float4 NormalizeCornerRadii(float2 size, float4 radii, float uniformRadius)
			{
				if (radii.x <= 0.0f && radii.y <= 0.0f && radii.z <= 0.0f && radii.w <= 0.0f)
					radii = float4(uniformRadius, uniformRadius, uniformRadius, uniformRadius);

				radii = max(radii, float4(0.0f, 0.0f, 0.0f, 0.0f));

				float scale = 1.0f;

				float topSum = radii.x + radii.y;
				float bottomSum = radii.w + radii.z;
				float leftSum = radii.x + radii.w;
				float rightSum = radii.y + radii.z;

				if (topSum > 0.0f)
					scale = min(scale, size.x / topSum);
				if (bottomSum > 0.0f)
					scale = min(scale, size.x / bottomSum);
				if (leftSum > 0.0f)
					scale = min(scale, size.y / leftSum);
				if (rightSum > 0.0f)
					scale = min(scale, size.y / rightSum);

				return radii * saturate(scale);
			}

			float RoundedRectDistance(float2 pixelPosition, float2 minPoint, float2 maxPoint, float4 radii)
			{
				float2 size = max(maxPoint - minPoint, float2(0.0f, 0.0f));
				radii = NormalizeCornerRadii(size, radii, 0.0f);

				float tl = radii.x;
				float tr = radii.y;
				float br = radii.z;
				float bl = radii.w;

				if (pixelPosition.x < minPoint.x + tl && pixelPosition.y < minPoint.y + tl)
					return length(pixelPosition - (minPoint + float2(tl, tl))) - tl;
				if (pixelPosition.x > maxPoint.x - tr && pixelPosition.y < minPoint.y + tr)
					return length(pixelPosition - float2(maxPoint.x - tr, minPoint.y + tr)) - tr;
				if (pixelPosition.x > maxPoint.x - br && pixelPosition.y > maxPoint.y - br)
					return length(pixelPosition - (maxPoint - float2(br, br))) - br;
				if (pixelPosition.x < minPoint.x + bl && pixelPosition.y > maxPoint.y - bl)
					return length(pixelPosition - float2(minPoint.x + bl, maxPoint.y - bl)) - bl;

				float2 outsideDistance = max(max(minPoint - pixelPosition, pixelPosition - maxPoint), float2(0.0f, 0.0f));
				if (outsideDistance.x > 0.0f || outsideDistance.y > 0.0f)
					return length(outsideDistance);

				float2 insideDistance = min(pixelPosition - minPoint, maxPoint - pixelPosition);
				return -min(insideDistance.x, insideDistance.y);
			}

			float SoftInsideMask(float distanceValue, float softness)
			{
				return 1.0f - smoothstep(-softness, softness, distanceValue);
			}

			float4 main(PSInput input) : SV_Target
			{
				float2 pixelPos = input.position.xy;

				float2 sourceSize = max(rectMax - rectMin, float2(0.0f, 0.0f));
				float maxShrink = max(min(sourceSize.x, sourceSize.y) * 0.5f - 0.001f, 0.0f);

				float safeSpread = max(spread, -maxShrink);
				float2 spreadVec = float2(safeSpread, safeSpread);

				float2 shadowMin = rectMin + offset - spreadVec;
				float2 shadowMax = rectMax + offset + spreadVec;

				float4 sourceCornerRadii = NormalizeCornerRadii(sourceSize, cornerRadii, max(radius, 0.0f));
				float4 shadowCornerRadii = max(sourceCornerRadii + safeSpread, float4(0.0f, 0.0f, 0.0f, 0.0f));
				shadowCornerRadii = NormalizeCornerRadii(max(shadowMax - shadowMin, float2(0.0f, 0.0f)), shadowCornerRadii, max(radius + safeSpread, 0.0f));

				float sourceDistance = RoundedRectDistance(pixelPos, rectMin, rectMax, sourceCornerRadii);
				float shadowDistance = RoundedRectDistance(pixelPos, shadowMin, shadowMax, shadowCornerRadii);

				float edgeAA = 0.5f;
				float softness = max(blurRadius, edgeAA);

				float sourceInsideAlpha = SoftInsideMask(sourceDistance, edgeAA);
				float sourceOutsideAlpha = 1.0f - sourceInsideAlpha;

				float shadowAlpha = SoftInsideMask(shadowDistance, softness);

				float outerMask = lerp(1.0f, sourceOutsideAlpha, saturate(outerOnly));
				float outerShadowAlpha = shadowAlpha * outerMask;

				float innerShadowAlpha = (1.0f - shadowAlpha) * sourceInsideAlpha;

				float alpha = lerp(outerShadowAlpha, innerShadowAlpha, saturate(shadowMode));

				return float4(color.rgb, color.a * alpha);
			}
		)");

		inline bool InitializeResources(Rhi::ISdGpuDevice& device, SdEffectResourceCache& cache, Rhi::SdTextureFormat colorFormat = Rhi::SdTextureFormat::Rgba8Unorm)
		{
			if (colorFormat == Rhi::SdTextureFormat::Unknown)
				colorFormat = Rhi::SdTextureFormat::Rgba8Unorm;

			const bool sharedResourcesReady =
				cache.GetShadowResourceSetLayout().IsValid()
				&& cache.GetFullscreenVertexShader().IsValid()
				&& cache.GetShadowPixelShader().IsValid();

			if (!sharedResourcesReady)
			{
				cache.Shutdown(device);

				const Rhi::SdShaderHandle fullscreenVertexShader = device.CreateShaderFromSource({
					Rhi::SdShaderStage::Vertex,
					Rhi::SdShaderLanguage::Hlsl,
					BlurEffectDetail::kFullscreenVertexShader,
					SODIUM_STRING("main"),
					SODIUM_STRING("vs_4_0"),
					SODIUM_STRING("Sodium.Effect.Fullscreen.VS")
					});
				const Rhi::SdShaderHandle shadowPixelShader = device.CreateShaderFromSource({
					Rhi::SdShaderStage::Pixel,
					Rhi::SdShaderLanguage::Hlsl,
					kShadowPixelShader,
					SODIUM_STRING("main"),
					SODIUM_STRING("ps_4_0"),
					SODIUM_STRING("Sodium.Effect.Shadow.PS")
					});
				cache.SetFullscreenVertexShader(fullscreenVertexShader);
				cache.SetShadowPixelShader(shadowPixelShader);
				if (!fullscreenVertexShader.IsValid() || !shadowPixelShader.IsValid())
				{
					cache.Shutdown(device);
					return false;
				}

				const Rhi::SdShaderBindingDesc shadowBindings[] =
				{
					{ SODIUM_STRING("ShadowParams"), Rhi::SdShaderResourceType::UniformBuffer, 0, 0, static_cast<Rhi::SdShaderStageFlags>(Rhi::SdShaderStageFlag::Pixel) }
				};
				const Rhi::SdResourceSetLayoutHandle shadowLayout = device.CreateResourceSetLayout({
					shadowBindings,
					SODIUM_STRING("Sodium.Effect.Shadow.ResourceLayout")
					});
				cache.SetShadowResourceSetLayout(shadowLayout);
				if (!shadowLayout.IsValid())
				{
					cache.Shutdown(device);
					return false;
				}

			}

			if (cache.GetShadowPipeline(colorFormat).IsValid())
				return true;

			Rhi::SdGraphicsPipelineDesc shadowPipelineDesc = {};
			shadowPipelineDesc.vertexShader = cache.GetFullscreenVertexShader();
			shadowPipelineDesc.pixelShader = cache.GetShadowPixelShader();
			shadowPipelineDesc.resourceSetLayout = cache.GetShadowResourceSetLayout();
			shadowPipelineDesc.blendState.color.blendEnabled = true;
			shadowPipelineDesc.rasterState.scissorEnabled = true;
			shadowPipelineDesc.colorFormat = colorFormat;
			shadowPipelineDesc.debugName = SODIUM_STRING("Sodium.Effect.Shadow.Pipeline");

			const Rhi::SdPipelineHandle shadowPipeline = device.CreateGraphicsPipeline(shadowPipelineDesc);
			if (!shadowPipeline.IsValid())
				return false;

			cache.SetShadowPipeline(colorFormat, shadowPipeline);
			return true;
		}

		inline SdRect ExpandShadowBounds(const SdRect& rect, const SdVec2& offset, float radius, float spread) noexcept
		{
			const float expand = std::max(0.0f, radius) + std::max(0.0f, spread);
			return {
				rect.min.x - expand + std::min(0.0f, offset.x),
				rect.min.y - expand + std::min(0.0f, offset.y),
				rect.max.x + expand + std::max(0.0f, offset.x),
				rect.max.y + expand + std::max(0.0f, offset.y)
			};
		}

		inline bool ApplyShadow(
			const SdEffectApplyContext& context,
			SdEffectResourceCache& resources,
			const SdEffectRenderLayer& targetLayer,
			const SdRect& rect,
			const SdRect& clipRect,
			const SdVec2& offset,
			const SdColorRgba& color,
			float radius,
			float spread,
			const SdCornerRadii& cornerRadii,
			float shadowMode,
			SdUtf8StringView debugName)
		{
			if (radius <= 0.0f || color.a == 0 || !targetLayer.IsValid())
				return false;
			const Rhi::SdTextureFormat targetFormat = BlurEffectDetail::ResolveLayerFormat(targetLayer);
			if (!InitializeResources(context.device, resources, targetFormat))
				return false;
			if (!resources.GetShadowResourceSetLayout().IsValid())
				return false;

			const Rhi::SdPipelineHandle shadowPipeline = resources.GetShadowPipeline(targetFormat);
			if (!shadowPipeline.IsValid())
				return false;

			const SdRect frameRect = BlurEffectDetail::ResolveLayerBounds(targetLayer);
			const float positiveSpread = std::max(0.0f, spread);
			const SdRect shadowRect =
			{
				rect.min.x + offset.x - positiveSpread,
				rect.min.y + offset.y - positiveSpread,
				rect.max.x + offset.x + positiveSpread,
				rect.max.y + offset.y + positiveSpread
			};
			const SdRect drawRect = ShadowEffectDetail::ExpandShadowBounds(shadowRect, {}, std::ceil(std::max(0.0f, radius)), 0.0f);
			SdRect clippedRect = BlurEffectDetail::IntersectRect(drawRect, frameRect);
			if (BlurEffectDetail::HasArea(clipRect))
				clippedRect = BlurEffectDetail::IntersectRect(clippedRect, clipRect);
			if (!BlurEffectDetail::HasArea(clippedRect))
				return false;

			const SdColorLinear linearColor = color.ToLinear();
			SdShadowParams params = {};
			params.rectMin = rect.min;
			params.rectMax = rect.max;
			params.offset = offset;
			params.radius = std::max(
				std::max(cornerRadii.topLeft, cornerRadii.topRight),
				std::max(cornerRadii.bottomRight, cornerRadii.bottomLeft));
			params.blurRadius = radius;
			params.spread = spread;
			params.outerOnly = 1.0f;
			params.shadowMode = shadowMode;
			params.color = { linearColor.r, linearColor.g, linearColor.b, linearColor.a };
			params.cornerRadii =
			{
				cornerRadii.topLeft,
				cornerRadii.topRight,
				cornerRadii.bottomRight,
				cornerRadii.bottomLeft
			};

			const Rhi::SdResourceSetHandle resourceSet = resources.GetOrCreateShadowParameterResourceSet(
				context.device,
				context.packet.packetVersion,
				&params,
				sizeof(params),
				debugName);
			if (!resourceSet.IsValid())
				return false;

			const Rhi::SdRectI renderArea = BlurEffectDetail::ToRenderArea(clippedRect);
			const Rhi::SdRenderPassColorAttachment colorAttachment =
			{
				targetLayer.texture,
				Rhi::SdLoadOp::Load,
				Rhi::SdStoreOp::Store,
				{}
			};
			const std::array<Rhi::SdRenderPassColorAttachment, 1> colorAttachments = { colorAttachment };
			const Rhi::SdRenderPassDesc renderPass =
			{
				colorAttachments,
				{},
				renderArea,
				debugName
			};

			context.encoder.BeginRenderPass(renderPass);
			context.encoder.SetPipeline(shadowPipeline);
			context.encoder.SetResourceSet(0, resourceSet);
			context.encoder.SetViewport({
				static_cast<float>(renderArea.left),
				static_cast<float>(renderArea.top),
				static_cast<float>(renderArea.Width()),
				static_cast<float>(renderArea.Height()),
				0.0f,
				1.0f
				});
			context.encoder.SetScissorRect(renderArea);
			context.encoder.Draw(3, 1, 0, 0);
			context.encoder.EndRenderPass();

			return true;
		}
	}

	struct SdDropShadowEffect final : ISdEffector
	{
		SdVec2 offset = { 0.0f, 4.0f };
		float radius = 12.0f;
		float spread = 0.0f;
		SdColorRgba color = { 0, 0, 0, 128 };
		SdEffectResourceCache resources = {};

		SdEffectTypeId GetTypeId() const noexcept override { return SdDropShadowEffectTypeId; }

		bool Initialize(const SdEffectInitContext& context) override
		{
			return ShadowEffectDetail::InitializeResources(context.device, resources);
		}

		void Shutdown(Rhi::ISdGpuDevice& device) noexcept override
		{
			resources.Shutdown(device);
		}

		SdEffectLayerRequirements QueryLayerRequirements(const SdEffectCommandView& command) const noexcept override
		{
			SdDropShadowEffectParameters parameters = {};
			parameters.offset = offset;
			parameters.radius = radius;
			parameters.spread = spread;
			parameters.color = color;
			if (const SdDropShadowEffectParameters* commandParameters = SdTryReadEffectParameters<SdDropShadowEffectParameters>(command.parameters))
				parameters = *commandParameters;

			SdEffectLayerRequirements requirements = {};
			requirements.requiresSource = false;
			requirements.requiresTarget = true;
			requirements.requiresIsolatedLayer = true;
			requirements.expandedBounds = ShadowEffectDetail::ExpandShadowBounds(
				command.payload.sourceBounds,
				parameters.offset,
				parameters.radius,
				parameters.spread);
			return requirements;
		}

		bool Apply(const SdEffectApplyContext& context) override
		{
			if (!context.layers)
				return false;

			SdDropShadowEffectParameters parameters = {};
			parameters.offset = offset;
			parameters.radius = radius;
			parameters.spread = spread;
			parameters.color = color;
			if (const SdDropShadowEffectParameters* commandParameters = SdTryReadEffectParameters<SdDropShadowEffectParameters>(context.parameters))
				parameters = *commandParameters;

			return ShadowEffectDetail::ApplyShadow(
				context,
				resources,
				context.layers->FindRenderLayer(context.payload.targetLayer),
				context.payload.sourceBounds,
				context.payload.clipRect,
				parameters.offset,
				parameters.color,
				parameters.radius,
				parameters.spread,
				parameters.cornerRadii,
				0.0f,
				SODIUM_STRING("Sodium.Effect.DropShadow"));
		}
	};

	struct SdInnerShadowEffect final : ISdEffector
	{
		SdVec2 offset = { 0.0f, 2.0f };
		float radius = 8.0f;
		float spread = 0.0f;
		SdColorRgba color = { 0, 0, 0, 96 };
		SdEffectResourceCache resources = {};

		SdEffectTypeId GetTypeId() const noexcept override { return SdInnerShadowEffectTypeId; }

		bool Initialize(const SdEffectInitContext& context) override
		{
			return ShadowEffectDetail::InitializeResources(context.device, resources);
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
			requirements.requiresIsolatedLayer = true;
			requirements.expandedBounds = command.payload.sourceBounds;
			return requirements;
		}

		bool Apply(const SdEffectApplyContext& context) override
		{
			if (!context.layers)
				return false;

			SdInnerShadowEffectParameters parameters = {};
			parameters.offset = offset;
			parameters.radius = radius;
			parameters.spread = spread;
			parameters.color = color;
			if (const SdInnerShadowEffectParameters* commandParameters = SdTryReadEffectParameters<SdInnerShadowEffectParameters>(context.parameters))
				parameters = *commandParameters;

			return ShadowEffectDetail::ApplyShadow(
				context,
				resources,
				context.layers->FindRenderLayer(context.payload.targetLayer),
				context.payload.sourceBounds,
				context.payload.clipRect,
				parameters.offset,
				parameters.color,
				parameters.radius,
				parameters.spread,
				parameters.cornerRadii,
				1.0f,
				SODIUM_STRING("Sodium.Effect.InnerShadow"));
		}
	};
}
