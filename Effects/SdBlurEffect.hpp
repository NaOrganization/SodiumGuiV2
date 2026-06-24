#pragma once

#include "Effects/SdEffect.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace Sodium
{
	namespace BlurEffectDetail
	{
		inline bool HasArea(const SdRect& rect) noexcept
		{
			return rect.Width() > 0.0f && rect.Height() > 0.0f;
		}

		inline SdRect IntersectRect(const SdRect& a, const SdRect& b) noexcept
		{
			return {
				std::max(a.min.x, b.min.x),
				std::max(a.min.y, b.min.y),
				std::min(a.max.x, b.max.x),
				std::min(a.max.y, b.max.y)
			};
		}

		inline Rhi::SdRectI ToRenderArea(const SdRect& rect) noexcept
		{
			return {
				static_cast<SdInt32>(std::floor(rect.min.x)),
				static_cast<SdInt32>(std::floor(rect.min.y)),
				static_cast<SdInt32>(std::ceil(rect.max.x)),
				static_cast<SdInt32>(std::ceil(rect.max.y))
			};
		}
	}

	struct SdBlurEffect final : ISdEffect
	{
		float radius = 8.0f;
		SdBlurQuality quality = SdBlurQuality::Balanced;
		bool clampToEdge = true;

		SdEffectType GetType() const noexcept override { return SdEffectType::Blur; }
		bool RequiresIsolatedLayer() const noexcept override { return radius > 0.0f; }
		bool RequiresBackdropCapture() const noexcept override { return false; }

		SdRect ExpandBounds(const SdRect& sourceBounds) const noexcept override
		{
			const float expand = std::max(0.0f, radius);
			return {
				sourceBounds.min.x - expand,
				sourceBounds.min.y - expand,
				sourceBounds.max.x + expand,
				sourceBounds.max.y + expand
			};
		}

		void BuildGraph(SdEffectBuildContext& context) const override
		{
			if (radius <= 0.0f || !context.source.IsValid() || !context.target.IsValid() || !context.device)
				return;

			if (!context.resources.GetBlurResourceSetLayout().IsValid()
				|| !context.resources.GetBlurParamsBuffer().IsValid()
				|| !context.resources.GetLinearClampSampler().IsValid()
				|| !context.resources.GetBlurHorizontalPipeline(Rhi::SdTextureFormat::Rgba8Unorm).IsValid()
				|| !context.resources.GetBlurVerticalPipeline(Rhi::SdTextureFormat::Rgba8Unorm).IsValid()
				|| !context.resources.GetCompositePipeline(Rhi::SdTextureFormat::Rgba8Unorm).IsValid())
			{
				return;
			}

			const Rhi::SdTextureFormat format = Rhi::SdTextureFormat::Rgba8Unorm;
			const SdUInt32 textureWidth = std::max(1u, context.pixelWidth);
			const SdUInt32 textureHeight = std::max(1u, context.pixelHeight);
			const SdRect fullTextureBounds = {
				0.0f,
				0.0f,
				static_cast<float>(textureWidth),
				static_cast<float>(textureHeight)
			};
			const SdRect maskBounds = BlurEffectDetail::HasArea(context.sourceBounds)
				? context.sourceBounds
				: fullTextureBounds;
			const SdRect textureBounds = BlurEffectDetail::HasArea(context.expandedBounds)
				? context.expandedBounds
				: maskBounds;
			SdRect renderBounds = maskBounds;
			if (BlurEffectDetail::HasArea(context.clipRect))
				renderBounds = BlurEffectDetail::IntersectRect(renderBounds, context.clipRect);
			if (!BlurEffectDetail::HasArea(renderBounds) || !BlurEffectDetail::HasArea(textureBounds))
				return;

			Rhi::SdRenderGraphTexture tempA = context.graph.CreateTexture(SdMakeEffectTextureDesc(
				textureWidth,
				textureHeight,
				format,
				"Sodium.Blur.TempA"));
			Rhi::SdRenderGraphTexture tempB = context.graph.CreateTexture(SdMakeEffectTextureDesc(
				textureWidth,
				textureHeight,
				format,
				"Sodium.Blur.TempB"));

			const auto addFullscreenPass = [this, &context, textureWidth, textureHeight, maskBounds, textureBounds](
				Rhi::SdRenderGraphPassHandle pass,
				Rhi::SdRenderGraphTexture input,
				Rhi::SdRenderGraphTexture output,
				Rhi::SdPipelineHandle pipeline,
				SdVec2 direction,
				Rhi::SdRectI renderArea,
				Rhi::SdLoadOp loadOp)
			{
				context.graph.SetPassCallback(pass, [&graph = context.graph, device = context.device, &resources = context.resources, input, output, pipeline, direction, renderArea, loadOp, radiusValue = radius, pixelWidth = textureWidth, pixelHeight = textureHeight, maskBounds, textureBounds, cornerRadius = context.cornerRadius](Rhi::ISdCommandEncoder& encoder)
				{
					if (!device)
						return;

					const Rhi::SdTextureHandle inputTexture = graph.GetPhysicalTexture(input);
					const Rhi::SdTextureHandle outputTexture = graph.GetPhysicalTexture(output);
					if (!inputTexture.IsValid() || !outputTexture.IsValid())
						return;

					SdBlurParams params = {};
					params.texelSize = {
						pixelWidth > 0 ? 1.0f / static_cast<float>(pixelWidth) : 1.0f,
						pixelHeight > 0 ? 1.0f / static_cast<float>(pixelHeight) : 1.0f
					};
					params.direction = direction;
					params.clipMin = maskBounds.min;
					params.clipMax = maskBounds.max;
					params.radius = radiusValue;
					params.cornerRadius = cornerRadius;
					params.textureMin = textureBounds.min;
					params.textureSize = {
						std::max(1.0f, textureBounds.Width()),
						std::max(1.0f, textureBounds.Height())
					};
					device->UpdateBuffer(resources.GetBlurParamsBuffer(), &params, sizeof(params), 0);

					const Rhi::SdBoundTexture textures[] =
					{
						{ 0, inputTexture }
					};
					const Rhi::SdBoundSampler samplers[] =
					{
						{ 0, resources.GetLinearClampSampler() }
					};
					const Rhi::SdBoundBuffer buffers[] =
					{
						{ 0, resources.GetBlurParamsBuffer(), 0, sizeof(SdBlurParams) }
					};
					const Rhi::SdResourceSetDesc resourceSetDesc =
					{
						resources.GetBlurResourceSetLayout(),
						textures,
						samplers,
						buffers,
						"Sodium.Blur.PassResourceSet"
					};
					const Rhi::SdResourceSetHandle resourceSet = device->CreateResourceSet(resourceSetDesc);
					if (!resourceSet.IsValid())
						return;

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
						"Sodium.Blur.FullscreenPass"
					};

					encoder.BeginRenderPass(renderPass);
					encoder.SetPipeline(pipeline);
					encoder.SetResourceSet(0, resourceSet);
					encoder.SetViewport({
						static_cast<float>(renderArea.left),
						static_cast<float>(renderArea.top),
						static_cast<float>(renderArea.Width()),
						static_cast<float>(renderArea.Height()),
						0.0f,
						1.0f
					});
					encoder.SetScissorRect(renderArea);
					encoder.Draw(3, 1, 0, 0);
					encoder.EndRenderPass();

					device->DestroyResourceSet(resourceSet);
				});
			};

			Rhi::SdRenderGraphPassHandle horizontal = context.graph.AddPass({
				"Sodium.Blur.Horizontal",
				Rhi::SdRenderGraphPassType::Fullscreen
			});
			context.graph.ReadTexture(horizontal, context.source);
			context.graph.WriteTexture(horizontal, tempA);
			addFullscreenPass(
				horizontal,
				context.source,
				tempA,
				context.resources.GetBlurHorizontalPipeline(format),
				{ 1.0f, 0.0f },
				{ 0, 0, static_cast<SdInt32>(textureWidth), static_cast<SdInt32>(textureHeight) },
				Rhi::SdLoadOp::Clear);

			Rhi::SdRenderGraphPassHandle vertical = context.graph.AddPass({
				"Sodium.Blur.Vertical",
				Rhi::SdRenderGraphPassType::Fullscreen
			});
			context.graph.ReadTexture(vertical, tempA);
			context.graph.WriteTexture(vertical, tempB);
			addFullscreenPass(
				vertical,
				tempA,
				tempB,
				context.resources.GetBlurVerticalPipeline(format),
				{ 0.0f, 1.0f },
				{ 0, 0, static_cast<SdInt32>(textureWidth), static_cast<SdInt32>(textureHeight) },
				Rhi::SdLoadOp::Clear);

			Rhi::SdRenderGraphPassHandle composite = context.graph.AddPass({
				"Sodium.Blur.Composite",
				Rhi::SdRenderGraphPassType::Fullscreen
			});
			context.graph.ReadTexture(composite, tempB);
			context.graph.WriteTexture(composite, context.target);
			addFullscreenPass(
				composite,
				tempB,
				context.target,
				context.resources.GetCompositePipeline(format),
				{ 0.0f, 0.0f },
				BlurEffectDetail::ToRenderArea(renderBounds),
				Rhi::SdLoadOp::Load);
		}
	};
}
