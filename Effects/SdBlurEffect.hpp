#pragma once

#include "Effects/SdEffect.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace Sodium
{
	namespace BlurEffectDetail
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
				float4 tintColor;
				float saturation;
				float brightness;
				float2 activeTextureSize;
			};

			struct PSInput
			{
				float4 position : SV_POSITION;
				float2 uv : TEXCOORD0;
			};

			float2 ResolveActiveUv(float2 uv)
			{
				uint sourceWidth = 1;
				uint sourceHeight = 1;
				sourceTexture.GetDimensions(sourceWidth, sourceHeight);
				float2 sourceSize = max(float2((float)sourceWidth, (float)sourceHeight), float2(1.0f, 1.0f));
				float2 activeSize = min(max(activeTextureSize, float2(1.0f, 1.0f)), sourceSize);
				float2 activePixel = clamp(saturate(uv) * activeSize - 0.5f, float2(0.0f, 0.0f), max(activeSize - 1.0f, float2(0.0f, 0.0f)));
				return (activePixel + 0.5f) / sourceSize;
			}

			float4 SampleSource(float2 uv)
			{
				return sourceTexture.SampleLevel(linearSampler, ResolveActiveUv(uv), 0.0f);
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
				float4 tintColor;
				float saturation;
				float brightness;
				float2 activeTextureSize;
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

			float3 ApplyBackdropColor(float3 color)
			{
				float luma = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
				color = lerp(float3(luma, luma, luma), color, saturation);
				color *= max(brightness, 0.0f);
				color = lerp(color, tintColor.rgb, saturate(tintColor.a));
				return color;
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

				uint sourceWidth = 1;
				uint sourceHeight = 1;
				sourceTexture.GetDimensions(sourceWidth, sourceHeight);
				float2 sourceSize = max(float2((float)sourceWidth, (float)sourceHeight), float2(1.0f, 1.0f));
				float2 activeSize = min(max(activeTextureSize, float2(1.0f, 1.0f)), sourceSize);
				float2 activePixel = clamp(sampleUv * activeSize - 0.5f, float2(0.0f, 0.0f), max(activeSize - 1.0f, float2(0.0f, 0.0f)));
				float2 activeUv = (activePixel + 0.5f) / sourceSize;

				float4 color = sourceTexture.Sample(linearSampler, activeUv);
				color.rgb = ApplyBackdropColor(color.rgb);
				return color * alpha;
			}
		)");

		inline bool InitializeResources(Rhi::ISdGpuDevice& device, SdEffectResourceCache& cache, Rhi::SdTextureFormat colorFormat = Rhi::SdTextureFormat::Rgba8Unorm)
		{
			if (colorFormat == Rhi::SdTextureFormat::Unknown)
				colorFormat = Rhi::SdTextureFormat::Rgba8Unorm;

			const bool sharedResourcesReady =
				cache.GetBlurResourceSetLayout().IsValid()
				&& cache.GetLinearClampSampler().IsValid()
				&& cache.GetFullscreenVertexShader().IsValid()
				&& cache.GetBlurPixelShader().IsValid()
				&& cache.GetCompositePixelShader().IsValid();

			if (!sharedResourcesReady)
			{
				cache.Shutdown(device);

				const Rhi::SdShaderHandle fullscreenVertexShader = device.CreateShaderFromSource({
					Rhi::SdShaderStage::Vertex,
					Rhi::SdShaderLanguage::Hlsl,
					kFullscreenVertexShader,
					SODIUM_STRING("main"),
					SODIUM_STRING("vs_4_0"),
					SODIUM_STRING("Sodium.Effect.Fullscreen.VS")
					});
				const Rhi::SdShaderHandle blurPixelShader = device.CreateShaderFromSource({
					Rhi::SdShaderStage::Pixel,
					Rhi::SdShaderLanguage::Hlsl,
					kBlurPixelShader,
					SODIUM_STRING("main"),
					SODIUM_STRING("ps_4_0"),
					SODIUM_STRING("Sodium.Effect.Blur.PS")
					});
				const Rhi::SdShaderHandle compositePixelShader = device.CreateShaderFromSource({
					Rhi::SdShaderStage::Pixel,
					Rhi::SdShaderLanguage::Hlsl,
					kCompositePixelShader,
					SODIUM_STRING("main"),
					SODIUM_STRING("ps_4_0"),
					SODIUM_STRING("Sodium.Effect.Composite.PS")
					});
				cache.SetFullscreenVertexShader(fullscreenVertexShader);
				cache.SetBlurPixelShader(blurPixelShader);
				cache.SetCompositePixelShader(compositePixelShader);
				if (!fullscreenVertexShader.IsValid() || !blurPixelShader.IsValid() || !compositePixelShader.IsValid())
				{
					cache.Shutdown(device);
					return false;
				}

				const Rhi::SdShaderBindingDesc bindings[] =
				{
					{ SODIUM_STRING("BlurParams"), Rhi::SdShaderResourceType::UniformBuffer, 0, 0, static_cast<Rhi::SdShaderStageFlags>(Rhi::SdShaderStageFlag::Pixel) },
					{ SODIUM_STRING("sourceTexture"), Rhi::SdShaderResourceType::Texture2D, 0, 0, static_cast<Rhi::SdShaderStageFlags>(Rhi::SdShaderStageFlag::Pixel) },
					{ SODIUM_STRING("linearSampler"), Rhi::SdShaderResourceType::Sampler, 0, 0, static_cast<Rhi::SdShaderStageFlags>(Rhi::SdShaderStageFlag::Pixel) }
				};
				const Rhi::SdResourceSetLayoutHandle layout = device.CreateResourceSetLayout({
					bindings,
					SODIUM_STRING("Sodium.Effect.Blur.ResourceLayout")
					});
				cache.SetBlurResourceSetLayout(layout);
				if (!layout.IsValid())
				{
					cache.Shutdown(device);
					return false;
				}

				const Rhi::SdSamplerHandle linearSampler = device.CreateSampler({
					Rhi::SdFilterMode::Linear,
					Rhi::SdFilterMode::Linear,
					Rhi::SdFilterMode::Linear,
					Rhi::SdAddressMode::Clamp,
					Rhi::SdAddressMode::Clamp,
					Rhi::SdAddressMode::Clamp,
					SODIUM_STRING("Sodium.Effect.LinearClamp")
					});
				cache.SetLinearClampSampler(linearSampler);
				if (!linearSampler.IsValid())
				{
					cache.Shutdown(device);
					return false;
				}
			}

			if (cache.GetBlurHorizontalPipeline(colorFormat).IsValid()
				&& cache.GetBlurVerticalPipeline(colorFormat).IsValid()
				&& cache.GetCompositePipeline(colorFormat).IsValid())
			{
				return true;
			}

			if (const Rhi::SdPipelineHandle pipeline = cache.GetBlurHorizontalPipeline(colorFormat); pipeline.IsValid())
				device.DestroyPipeline(pipeline);
			if (const Rhi::SdPipelineHandle pipeline = cache.GetBlurVerticalPipeline(colorFormat); pipeline.IsValid())
				device.DestroyPipeline(pipeline);
			if (const Rhi::SdPipelineHandle pipeline = cache.GetCompositePipeline(colorFormat); pipeline.IsValid())
				device.DestroyPipeline(pipeline);
			cache.SetBlurHorizontalPipeline(colorFormat, {});
			cache.SetBlurVerticalPipeline(colorFormat, {});
			cache.SetCompositePipeline(colorFormat, {});

			Rhi::SdGraphicsPipelineDesc blurPipelineDesc = {};
			blurPipelineDesc.vertexShader = cache.GetFullscreenVertexShader();
			blurPipelineDesc.pixelShader = cache.GetBlurPixelShader();
			blurPipelineDesc.resourceSetLayout = cache.GetBlurResourceSetLayout();
			blurPipelineDesc.blendState.color.blendEnabled = false;
			blurPipelineDesc.rasterState.scissorEnabled = true;
			blurPipelineDesc.colorFormat = colorFormat;
			blurPipelineDesc.debugName = SODIUM_STRING("Sodium.Effect.Blur.Pipeline");

			Rhi::SdGraphicsPipelineDesc compositePipelineDesc = blurPipelineDesc;
			compositePipelineDesc.pixelShader = cache.GetCompositePixelShader();
			compositePipelineDesc.blendState.color.blendEnabled = true;
			compositePipelineDesc.debugName = SODIUM_STRING("Sodium.Effect.Composite.Pipeline");

			const Rhi::SdPipelineHandle horizontalPipeline = device.CreateGraphicsPipeline(blurPipelineDesc);
			const Rhi::SdPipelineHandle verticalPipeline = device.CreateGraphicsPipeline(blurPipelineDesc);
			const Rhi::SdPipelineHandle compositePipeline = device.CreateGraphicsPipeline(compositePipelineDesc);
			if (!horizontalPipeline.IsValid() || !verticalPipeline.IsValid() || !compositePipeline.IsValid())
			{
				if (horizontalPipeline.IsValid())
					device.DestroyPipeline(horizontalPipeline);
				if (verticalPipeline.IsValid())
					device.DestroyPipeline(verticalPipeline);
				if (compositePipeline.IsValid())
					device.DestroyPipeline(compositePipeline);
				return false;
			}

			cache.SetBlurHorizontalPipeline(colorFormat, horizontalPipeline);
			cache.SetBlurVerticalPipeline(colorFormat, verticalPipeline);
			cache.SetCompositePipeline(colorFormat, compositePipeline);
			return true;
		}

		inline SdCornerRadii NormalizeCornerRadii(const SdCornerRadii& cornerRadii, float uniformRadius) noexcept
		{
			if (cornerRadii.topLeft > 0.0f || cornerRadii.topRight > 0.0f || cornerRadii.bottomRight > 0.0f || cornerRadii.bottomLeft > 0.0f)
				return cornerRadii;
			return { uniformRadius, uniformRadius, uniformRadius, uniformRadius };
		}

		inline SdRect ResolveLayerBounds(const SdEffectRenderLayer& layer) noexcept
		{
			if (layer.bounds.HasArea())
				return layer.bounds;
			return {
				0.0f,
				0.0f,
				static_cast<float>(layer.width),
				static_cast<float>(layer.height)
			};
		}

		inline Rhi::SdTextureFormat ResolveLayerFormat(const SdEffectRenderLayer& layer) noexcept
		{
			return layer.format == Rhi::SdTextureFormat::Unknown ? Rhi::SdTextureFormat::Rgba8Unorm : layer.format;
		}

		inline bool RunFullscreenBlurPass(
			const SdEffectApplyContext& context,
			SdEffectResourceCache& resources,
			Rhi::SdTextureHandle inputTexture,
			Rhi::SdTextureHandle outputTexture,
			Rhi::SdPipelineHandle pipeline,
			SdUInt32 inputWidth,
			SdUInt32 inputHeight,
			float radius,
			SdVec2 direction,
			SdRect maskBounds,
			SdRect textureBounds,
			SdCornerRadii cornerRadii,
			SdEffectResourceCache::ResourceSetSlot resourceSetSlot,
			SdVec4 tintColor,
			float saturation,
			float brightness,
			Rhi::SdRectI renderArea,
			Rhi::SdLoadOp loadOp,
			SdUtf8StringView debugName)
		{
			if (!inputTexture.IsValid() || !outputTexture.IsValid() || !pipeline.IsValid())
				return false;
			if (renderArea.Width() == 0 || renderArea.Height() == 0)
				return false;

			SdBlurParams params = {};
			params.texelSize =
			{
				inputWidth > 0 ? 1.0f / static_cast<float>(inputWidth) : 1.0f,
				inputHeight > 0 ? 1.0f / static_cast<float>(inputHeight) : 1.0f
			};
			params.direction = direction;
			params.clipMin = maskBounds.min;
			params.clipMax = maskBounds.max;
			params.cornerRadii = cornerRadii;
			params.radius = radius;
			params.textureMin = textureBounds.min;
			params.textureSize =
			{
				std::max(1.0f, textureBounds.Width()),
				std::max(1.0f, textureBounds.Height())
			};
			params.tintColor = tintColor;
			params.saturation = saturation;
			params.brightness = brightness;
			params.activeTextureSize =
			{
				static_cast<float>(inputWidth),
				static_cast<float>(inputHeight)
			};

			const Rhi::SdResourceSetHandle resourceSet = resources.GetOrCreateBlurParameterResourceSet(
				context.device,
				resourceSetSlot,
				context.packet.packetVersion,
				inputTexture,
				&params,
				sizeof(params),
				debugName);
			if (!resourceSet.IsValid())
				return false;

			const Rhi::SdRenderPassColorAttachment colorAttachment =
			{
				outputTexture,
				loadOp,
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
			context.encoder.SetPipeline(pipeline);
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

		inline bool ApplyBlur(
			const SdEffectApplyContext& context,
			SdEffectResourceCache& resources,
			const SdEffectRenderLayer& sourceLayer,
			const SdEffectRenderLayer& targetLayer,
			float radius,
			SdRect sourceBounds,
			SdRect expandedBounds,
			SdRect clipRect,
			SdCornerRadii cornerRadii,
			SdVec4 tintColor = {},
			float saturation = 1.0f,
			float brightness = 1.0f)
		{
			if (radius <= 0.0f || !sourceLayer.IsValid() || !targetLayer.IsValid())
				return false;
			const Rhi::SdTextureFormat sourceFormat = ResolveLayerFormat(sourceLayer);
			const Rhi::SdTextureFormat targetFormat = ResolveLayerFormat(targetLayer);
			if (!InitializeResources(context.device, resources, sourceFormat)
				|| !InitializeResources(context.device, resources, targetFormat))
			{
				return false;
			}
			if (!resources.GetBlurResourceSetLayout().IsValid()
				|| !resources.GetLinearClampSampler().IsValid()
				|| !resources.GetBlurHorizontalPipeline(sourceFormat).IsValid()
				|| !resources.GetBlurVerticalPipeline(sourceFormat).IsValid()
				|| !resources.GetCompositePipeline(targetFormat).IsValid())
			{
				return false;
			}

			const SdUInt32 textureWidth = std::max(1u, sourceLayer.width);
			const SdUInt32 textureHeight = std::max(1u, sourceLayer.height);
			const SdRect fallbackSourceBounds = ResolveLayerBounds(sourceLayer);
			if (!sourceBounds.HasArea())
				sourceBounds = fallbackSourceBounds;
			if (!expandedBounds.HasArea())
				expandedBounds = sourceBounds;

			SdRect renderBounds = sourceBounds;
			if (clipRect.HasArea())
				renderBounds = renderBounds.Intersection(clipRect);
			if (!renderBounds.HasArea() || !expandedBounds.HasArea())
				return false;

			const Rhi::SdTextureDesc textureDesc =
			{
				textureWidth,
				textureHeight,
				1,
				1,
				sourceFormat,
				Rhi::SdTextureUsage::RenderTarget | Rhi::SdTextureUsage::ShaderRead,
				1,
				false,
				SODIUM_STRING("Sodium.Effect.Blur.Temp")
			};
			const Rhi::SdTextureHandle tempA = resources.GetOrCreateTexture(context.device, SdEffectResourceCache::TextureSlot::BlurTempA, textureDesc);
			const Rhi::SdTextureHandle tempB = resources.GetOrCreateTexture(context.device, SdEffectResourceCache::TextureSlot::BlurTempB, textureDesc);
			if (!tempA.IsValid() || !tempB.IsValid())
				return false;

			const Rhi::SdRectI localRenderArea =
			{
				0,
				0,
				static_cast<SdInt32>(textureWidth),
				static_cast<SdInt32>(textureHeight)
			};
			const bool horizontal = RunFullscreenBlurPass(
				context,
				resources,
				sourceLayer.texture,
				tempA,
				resources.GetBlurHorizontalPipeline(sourceFormat),
				textureWidth,
				textureHeight,
				radius,
				{ 1.0f, 0.0f },
				sourceBounds,
				expandedBounds,
				cornerRadii,
				SdEffectResourceCache::ResourceSetSlot::BlurSource,
				{},
				1.0f,
				1.0f,
				localRenderArea,
				Rhi::SdLoadOp::Clear,
				SODIUM_STRING("Sodium.Effect.Blur.Horizontal"));
			const bool vertical = horizontal && RunFullscreenBlurPass(
				context,
				resources,
				tempA,
				tempB,
				resources.GetBlurVerticalPipeline(sourceFormat),
				textureWidth,
				textureHeight,
				radius,
				{ 0.0f, 1.0f },
				sourceBounds,
				expandedBounds,
				cornerRadii,
				SdEffectResourceCache::ResourceSetSlot::BlurTempA,
				{},
				1.0f,
				1.0f,
				localRenderArea,
				Rhi::SdLoadOp::Clear,
				SODIUM_STRING("Sodium.Effect.Blur.Vertical"));
			const bool composite = vertical && RunFullscreenBlurPass(
				context,
				resources,
				tempB,
				targetLayer.texture,
				resources.GetCompositePipeline(targetFormat),
				textureWidth,
				textureHeight,
				radius,
				{ 0.0f, 0.0f },
				sourceBounds,
				expandedBounds,
				cornerRadii,
				SdEffectResourceCache::ResourceSetSlot::BlurComposite,
				tintColor,
				saturation,
				brightness,
				Rhi::SdRectI::FromRect(renderBounds),
				Rhi::SdLoadOp::Load,
				SODIUM_STRING("Sodium.Effect.Blur.Composite"));

			return composite;
		}
	}

	struct SdBlurEffect final : ISdEffector
	{
		float radius = 8.0f;
		SdBlurQuality quality = SdBlurQuality::Balanced;
		bool clampToEdge = true;
		SdEffectResourceCache resources = {};

		SdEffectTypeId GetTypeId() const noexcept override { return SdBlurEffectTypeId; }

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
			float effectRadius = radius;
			if (const SdBlurEffectParameters* parameters = SdTryReadEffectParameters<SdBlurEffectParameters>(command.parameters))
				effectRadius = parameters->radius;

			SdEffectLayerRequirements requirements = {};
			requirements.requiresSource = true;
			requirements.requiresTarget = true;
			requirements.requiresIsolatedLayer = effectRadius > 0.0f;
			requirements.expandedBounds =
			{
				command.payload.sourceBounds.min.x - std::max(0.0f, effectRadius),
				command.payload.sourceBounds.min.y - std::max(0.0f, effectRadius),
				command.payload.sourceBounds.max.x + std::max(0.0f, effectRadius),
				command.payload.sourceBounds.max.y + std::max(0.0f, effectRadius)
			};
			return requirements;
		}

		bool Apply(const SdEffectApplyContext& context) override
		{
			if (!context.layers)
				return false;

			SdBlurEffectParameters parameters = {};
			parameters.radius = radius;
			parameters.quality = quality;
			parameters.clampToEdge = clampToEdge;
			if (const SdBlurEffectParameters* commandParameters = SdTryReadEffectParameters<SdBlurEffectParameters>(context.parameters))
				parameters = *commandParameters;

			const SdEffectRenderLayer sourceLayer = context.layers->FindRenderLayer(context.payload.sourceLayer);
			const SdEffectRenderLayer targetLayer = context.layers->FindRenderLayer(context.payload.targetLayer);
			return BlurEffectDetail::ApplyBlur(
				context,
				resources,
				sourceLayer,
				targetLayer,
				parameters.radius,
				context.payload.sourceBounds,
				context.payload.expandedBounds,
				context.payload.clipRect,
				BlurEffectDetail::NormalizeCornerRadii(parameters.cornerRadii, parameters.cornerRadius));
		}
	};
}
