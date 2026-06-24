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
				float radius;
				float cornerRadius;
				float2 padding0;
				float2 textureMin;
				float2 textureSize;
			};

			struct PSInput
			{
				float4 position : SV_POSITION;
				float2 uv : TEXCOORD0;
			};

			float4 main(PSInput input) : SV_Target
			{
				int sampleRadius = (int)ceil(clamp(radius, 1.0f, 32.0f));
				float sigma = max((float)sampleRadius * 0.5f, 1.0f);
				float twoSigmaSquared = 2.0f * sigma * sigma;
				float2 stepValue = direction * texelSize;
				float4 color = float4(0.0f, 0.0f, 0.0f, 0.0f);
				float totalWeight = 0.0f;

				[loop]
				for (int offset = -32; offset <= 32; ++offset)
				{
					if (abs(offset) > sampleRadius)
						continue;

					float weight = exp(-((float)(offset * offset)) / twoSigmaSquared);
					color += sourceTexture.SampleLevel(linearSampler, input.uv + stepValue * (float)offset, 0.0f) * weight;
					totalWeight += weight;
				}

				return color / max(totalWeight, 0.00001f);
			})");

		inline constexpr const char* kCompositePixelShader = SODIUM_STRING(R"(
			Texture2D sourceTexture : register(t0);
			SamplerState linearSampler : register(s0);

			cbuffer BlurParams : register(b0)
			{
				float2 texelSize;
				float2 direction;
				float2 clipMin;
				float2 clipMax;
				float radius;
				float cornerRadius;
				float2 padding0;
				float2 textureMin;
				float2 textureSize;
			};

			struct PSInput
			{
				float4 position : SV_POSITION;
				float2 uv : TEXCOORD0;
			};

			float4 main(PSInput input) : SV_Target
			{
				float alpha = 1.0f;
				if (cornerRadius > 0.5f)
				{
					float2 clampedPoint = clamp(input.position.xy, clipMin + cornerRadius, clipMax - cornerRadius);
					float distanceToCorner = length(input.position.xy - clampedPoint);
					alpha = saturate(cornerRadius + 0.5f - distanceToCorner);
				}
				float2 sampleUv = input.uv;
				if (textureSize.x > 0.0f && textureSize.y > 0.0f)
					sampleUv = saturate((input.position.xy - textureMin) / textureSize);
				return sourceTexture.Sample(linearSampler, sampleUv) * alpha;
			})");

		inline constexpr const char* kShadowPixelShader = SODIUM_STRING(R"(
			cbuffer ShadowParams : register(b0)
			{
				float2 rectMin;
				float2 rectMax;
				float2 offset;
				float radius;
				float blurRadius;
				float spread;
				float3 padding0;
				float4 color;
			};

			struct PSInput
			{
				float4 position : SV_POSITION;
				float2 uv : TEXCOORD0;
			};

			float RoundedRectDistance(float2 pixelPosition, float2 minPoint, float2 maxPoint, float cornerRadius)
			{
				float2 center = (minPoint + maxPoint) * 0.5f;
				float2 halfSize = max((maxPoint - minPoint) * 0.5f - float2(cornerRadius, cornerRadius), float2(0.0f, 0.0f));
				float2 q = abs(pixelPosition - center) - halfSize;
				return length(max(q, float2(0.0f, 0.0f))) + min(max(q.x, q.y), 0.0f) - cornerRadius;
			}

			float4 main(PSInput input) : SV_Target
			{
				float clampedSpread = max(spread, 0.0f);
				float2 shadowMin = rectMin + offset - clampedSpread;
				float2 shadowMax = rectMax + offset + clampedSpread;
				float shadowRadius = max(radius + clampedSpread, 0.0f);
				float distanceToShadow = RoundedRectDistance(input.position.xy, shadowMin, shadowMax, shadowRadius);
				float softness = max(blurRadius, 0.5f);
				float alpha = 1.0f - smoothstep(-softness, softness, distanceToShadow);
				return float4(color.rgb, color.a * alpha);
			})");

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
