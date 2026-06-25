#pragma once

#include "Effects/SdEffect.hpp"
#include "backends/Dx11/SdDx11Rhi.h"

#include <d3dcompiler.h>
#include <wrl/client.h>

#include <cstring>

#pragma comment(lib, "d3dcompiler.lib")

namespace Sodium::Backends
{
	namespace Detail
	{
		inline constexpr const char* kFullscreenVertexShader = SODIUM_STRING(R"(
			struct VSOutput
			{
				float4 position : SV_POSITION;
				float2 uv : TEXCOORD0;
			};

			VSOutput main(uint vertexId : SV_VertexID)
			{
				float2 positions[3] =
				{
					float2(-1.0f, -1.0f),
					float2(-1.0f,  3.0f),
					float2( 3.0f, -1.0f)
				};

				float2 position = positions[vertexId];
				VSOutput output;
				output.position = float4(position, 0.0f, 1.0f);
				output.uv = float2((position.x + 1.0f) * 0.5f, 1.0f - ((position.y + 1.0f) * 0.5f));
				return output;
			})");

		inline constexpr const char* kBlurPixelShader = SODIUM_STRING(R"(
			Texture2D sourceTexture : register(t0);
			SamplerState linearSampler : register(s0);

			cbuffer BlurParams : register(b0)
			{
				float2 texelSize;
				float2 direction;
				float2 clipMin;
				float2 clipMax;
				float4 cornerRadii;
				float radius;
				float3 padding0;
				float2 textureMin;
				float2 textureSize;
			};

			struct PSInput
			{
				float4 position : SV_POSITION;
				float2 uv : TEXCOORD0;
			};

			float4 SampleSource(float2 uv)
			{
				return sourceTexture.SampleLevel(linearSampler, saturate(uv), 0.0f);
			}

			float4 main(PSInput input) : SV_Target
			{
				float clampedRadius = clamp(radius, 0.0f, 32.0f);

				if (clampedRadius <= 0.001f)
					return SampleSource(input.uv);

				int sampleRadius = (int)ceil(clampedRadius);

				float sigma = max((float)sampleRadius * 0.5f, 1.0f);
				float invTwoSigmaSquared = 1.0f / max(2.0f * sigma * sigma, 0.00001f);

				float2 stepValue = direction * texelSize;

				float4 color = SampleSource(input.uv);
				float totalWeight = 1.0f;

				[loop]
				for (int offset = 1; offset <= sampleRadius; ++offset)
				{
					float offsetF = (float)offset;
					float weight = exp(-(offsetF * offsetF) * invTwoSigmaSquared);

					float2 delta = stepValue * offsetF;

					color += SampleSource(input.uv + delta) * weight;
					color += SampleSource(input.uv - delta) * weight;

					totalWeight += weight * 2.0f;
				}

				return color / max(totalWeight, 0.00001f);
			}
		)");

		inline constexpr const char* kCompositePixelShader = SODIUM_STRING(R"(
			Texture2D sourceTexture : register(t0);
			SamplerState linearSampler : register(s0);

			cbuffer BlurParams : register(b0)
			{
				float2 texelSize;
				float2 direction;
				float2 clipMin;
				float2 clipMax;
				float4 cornerRadii;
				float radius;
				float3 padding0;
				float2 textureMin;
				float2 textureSize;
			};

			struct PSInput
			{
				float4 position : SV_POSITION;
				float2 uv : TEXCOORD0;
			};

			float4 NormalizeCornerRadii(float2 size, float4 radii)
			{
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

			float CornerAlpha(float2 p, float2 center, float radius)
			{
				if (radius <= 0.5f)
					return 1.0f;

				float signedDistance = length(p - center) - radius;
				return saturate(0.5f - signedDistance);
			}

			float RoundedRectAlpha(float2 pixelPos, float2 minPoint, float2 maxPoint, float4 radii)
			{
				if (pixelPos.x < minPoint.x || pixelPos.y < minPoint.y ||
					pixelPos.x > maxPoint.x || pixelPos.y > maxPoint.y)
				{
					return 0.0f;
				}

				float2 size = max(maxPoint - minPoint, float2(0.0f, 0.0f));
				radii = NormalizeCornerRadii(size, radii);

				float tl = radii.x;
				float tr = radii.y;
				float br = radii.z;
				float bl = radii.w;

				float alpha = 1.0f;

				float edgeDistance = min(
					min(pixelPos.x - minPoint.x, maxPoint.x - pixelPos.x),
					min(pixelPos.y - minPoint.y, maxPoint.y - pixelPos.y)
				);

				alpha = min(alpha, saturate(edgeDistance + 0.5f));

				if (pixelPos.x < minPoint.x + tl && pixelPos.y < minPoint.y + tl)
				{
					alpha = min(alpha, CornerAlpha(pixelPos, minPoint + float2(tl, tl), tl));
				}
				else if (pixelPos.x > maxPoint.x - tr && pixelPos.y < minPoint.y + tr)
				{
					alpha = min(alpha, CornerAlpha(pixelPos, float2(maxPoint.x - tr, minPoint.y + tr), tr));
				}
				else if (pixelPos.x > maxPoint.x - br && pixelPos.y > maxPoint.y - br)
				{
					alpha = min(alpha, CornerAlpha(pixelPos, maxPoint - float2(br, br), br));
				}
				else if (pixelPos.x < minPoint.x + bl && pixelPos.y > maxPoint.y - bl)
				{
					alpha = min(alpha, CornerAlpha(pixelPos, float2(minPoint.x + bl, maxPoint.y - bl), bl));
				}

				return alpha;
			}

			float4 main(PSInput input) : SV_Target
			{
				float2 pixelPos = input.position.xy;

				float alpha = RoundedRectAlpha(pixelPos, clipMin, clipMax, cornerRadii);

				if (alpha <= 0.0f)
					return float4(0.0f, 0.0f, 0.0f, 0.0f);

				float2 sampleUv = input.uv;

				if (textureSize.x > 0.0f && textureSize.y > 0.0f)
					sampleUv = (pixelPos - textureMin) / textureSize;

				sampleUv = saturate(sampleUv);

				return sourceTexture.Sample(linearSampler, sampleUv) * alpha;
			}
		)");

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

		inline bool CompileShader(
			const char* source,
			const char* entryPoint,
			const char* target,
			Microsoft::WRL::ComPtr<ID3DBlob>& bytecode)
		{
			Microsoft::WRL::ComPtr<ID3DBlob> errors = {};
			const UINT flags =
#if defined(_DEBUG)
				D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
				D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif
			return SUCCEEDED(::D3DCompile(
				source,
				std::strlen(source),
				nullptr,
				nullptr,
				nullptr,
				entryPoint,
				target,
				flags,
				0,
				bytecode.ReleaseAndGetAddressOf(),
				errors.ReleaseAndGetAddressOf()));
		}
	}

	inline bool SdDx11InitializeBlurEffectResources(
		SdDx11GpuDevice& device,
		SdEffectResourceCache& cache,
		Rhi::SdTextureFormat colorFormat = Rhi::SdTextureFormat::Rgba8Unorm)
	{
		if (!device.IsInitialized())
			return false;

		Microsoft::WRL::ComPtr<ID3DBlob> fullscreenVs = {};
		Microsoft::WRL::ComPtr<ID3DBlob> blurPs = {};
		Microsoft::WRL::ComPtr<ID3DBlob> compositePs = {};
		if (!Detail::CompileShader(Detail::kFullscreenVertexShader, "main", "vs_4_0", fullscreenVs))
			return false;
		if (!Detail::CompileShader(Detail::kBlurPixelShader, "main", "ps_4_0", blurPs))
			return false;
		if (!Detail::CompileShader(Detail::kCompositePixelShader, "main", "ps_4_0", compositePs))
			return false;

		const Rhi::SdShaderHandle fullscreenVertexShader = device.CreateShader({
			Rhi::SdShaderStage::Vertex,
			SdSpan<const Rhi::SdByte>(static_cast<const Rhi::SdByte*>(fullscreenVs->GetBufferPointer()), fullscreenVs->GetBufferSize()),
			"main",
			"Sodium.Effect.Fullscreen.VS"
			});
		const Rhi::SdShaderHandle blurPixelShader = device.CreateShader({
			Rhi::SdShaderStage::Pixel,
			SdSpan<const Rhi::SdByte>(static_cast<const Rhi::SdByte*>(blurPs->GetBufferPointer()), blurPs->GetBufferSize()),
			"main",
			"Sodium.Effect.Blur.PS"
			});
		const Rhi::SdShaderHandle compositePixelShader = device.CreateShader({
			Rhi::SdShaderStage::Pixel,
			SdSpan<const Rhi::SdByte>(static_cast<const Rhi::SdByte*>(compositePs->GetBufferPointer()), compositePs->GetBufferSize()),
			"main",
			"Sodium.Effect.Composite.PS"
			});
		if (!fullscreenVertexShader.IsValid() || !blurPixelShader.IsValid() || !compositePixelShader.IsValid())
			return false;

		const Rhi::SdShaderBindingDesc bindings[] =
		{
			{ "BlurParams", Rhi::SdShaderResourceType::UniformBuffer, 0, 0, static_cast<Rhi::SdShaderStageFlags>(Rhi::SdShaderStageFlag::Pixel) },
			{ "sourceTexture", Rhi::SdShaderResourceType::Texture2D, 0, 0, static_cast<Rhi::SdShaderStageFlags>(Rhi::SdShaderStageFlag::Pixel) },
			{ "linearSampler", Rhi::SdShaderResourceType::Sampler, 0, 0, static_cast<Rhi::SdShaderStageFlags>(Rhi::SdShaderStageFlag::Pixel) }
		};
		const Rhi::SdResourceSetLayoutHandle layout = device.CreateResourceSetLayout({
			bindings,
			"Sodium.Effect.Blur.ResourceLayout"
			});
		if (!layout.IsValid())
			return false;

		const Rhi::SdBufferHandle paramsBuffer = device.CreateBuffer({
			sizeof(SdBlurParams),
			static_cast<Rhi::SdBufferUsageFlags>(Rhi::SdBufferUsage::Uniform),
			Rhi::SdMemoryUsage::CpuToGpu,
			"Sodium.Effect.Blur.Params"
			}, nullptr);
		if (!paramsBuffer.IsValid())
			return false;

		const Rhi::SdSamplerHandle linearSampler = device.CreateSampler({
			Rhi::SdFilterMode::Linear,
			Rhi::SdFilterMode::Linear,
			Rhi::SdFilterMode::Linear,
			Rhi::SdAddressMode::Clamp,
			Rhi::SdAddressMode::Clamp,
			Rhi::SdAddressMode::Clamp,
			"Sodium.Effect.LinearClamp"
			});
		if (!linearSampler.IsValid())
			return false;

		Rhi::SdGraphicsPipelineDesc blurPipelineDesc = {};
		blurPipelineDesc.vertexShader = fullscreenVertexShader;
		blurPipelineDesc.pixelShader = blurPixelShader;
		blurPipelineDesc.resourceSetLayout = layout;
		blurPipelineDesc.blendState.color.blendEnabled = false;
		blurPipelineDesc.rasterState.scissorEnabled = true;
		blurPipelineDesc.colorFormat = colorFormat;
		blurPipelineDesc.debugName = "Sodium.Effect.Blur.Pipeline";

		Rhi::SdGraphicsPipelineDesc compositePipelineDesc = blurPipelineDesc;
		compositePipelineDesc.pixelShader = compositePixelShader;
		compositePipelineDesc.blendState.color.blendEnabled = true;
		compositePipelineDesc.debugName = "Sodium.Effect.Composite.Pipeline";

		const Rhi::SdPipelineHandle horizontalPipeline = device.CreateGraphicsPipeline(blurPipelineDesc);
		const Rhi::SdPipelineHandle verticalPipeline = device.CreateGraphicsPipeline(blurPipelineDesc);
		const Rhi::SdPipelineHandle compositePipeline = device.CreateGraphicsPipeline(compositePipelineDesc);
		if (!horizontalPipeline.IsValid() || !verticalPipeline.IsValid() || !compositePipeline.IsValid())
			return false;

		cache.SetBlurResourceSetLayout(layout);
		cache.SetBlurParamsBuffer(paramsBuffer);
		cache.SetLinearClampSampler(linearSampler);
		cache.SetBlurHorizontalPipeline(horizontalPipeline);
		cache.SetBlurVerticalPipeline(verticalPipeline);
		cache.SetCompositePipeline(compositePipeline);

		const Rhi::SdShaderBindingDesc shadowBindings[] =
		{
			{ "ShadowParams", Rhi::SdShaderResourceType::UniformBuffer, 0, 0, static_cast<Rhi::SdShaderStageFlags>(Rhi::SdShaderStageFlag::Pixel) }
		};
		const Rhi::SdResourceSetLayoutHandle shadowLayout = device.CreateResourceSetLayout({
			shadowBindings,
			"Sodium.Effect.Shadow.ResourceLayout"
			});
		if (!shadowLayout.IsValid())
			return false;
		cache.SetShadowResourceSetLayout(shadowLayout);

		const Rhi::SdBufferHandle shadowParamsBuffer = device.CreateBuffer({
			sizeof(SdShadowParams),
			static_cast<Rhi::SdBufferUsageFlags>(Rhi::SdBufferUsage::Uniform),
			Rhi::SdMemoryUsage::CpuToGpu,
			"Sodium.Effect.Shadow.Params"
			}, nullptr);
		if (!shadowParamsBuffer.IsValid())
			return false;
		cache.SetShadowParamsBuffer(shadowParamsBuffer);

		Microsoft::WRL::ComPtr<ID3DBlob> shadowPs = {};
		if (!Detail::CompileShader(Detail::kShadowPixelShader, "main", "ps_4_0", shadowPs))
			return false;

		const Rhi::SdShaderHandle shadowPixelShader = device.CreateShader({
			Rhi::SdShaderStage::Pixel,
			SdSpan<const Rhi::SdByte>(static_cast<const Rhi::SdByte*>(shadowPs->GetBufferPointer()), shadowPs->GetBufferSize()),
			"main",
			"Sodium.Effect.Shadow.PS"
			});
		if (!shadowPixelShader.IsValid())
			return false;

		Rhi::SdGraphicsPipelineDesc shadowPipelineDesc = compositePipelineDesc;
		shadowPipelineDesc.pixelShader = shadowPixelShader;
		shadowPipelineDesc.resourceSetLayout = shadowLayout;
		shadowPipelineDesc.debugName = "Sodium.Effect.Shadow.Pipeline";
		const Rhi::SdPipelineHandle shadowPipeline = device.CreateGraphicsPipeline(shadowPipelineDesc);
		if (!shadowPipeline.IsValid())
			return false;

		cache.SetShadowPipeline(shadowPipeline);
		return true;
	}
}
