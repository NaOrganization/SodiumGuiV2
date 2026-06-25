#pragma once

#include "Core/SdCore.h"
#include "Rhi/SdFullscreenPass.hpp"
#include "Rhi/SdRenderGraph.hpp"
#include "Rhi/SdTransientTexturePool.hpp"

#include <vector>

namespace Sodium
{
	struct SdEffectTag final {};
	using SdEffectHandle = SdHandle<SdEffectTag>;

	enum class SdEffectType : SdUInt16
	{
		None,
		Blur,
		BackdropBlur,
		DropShadow,
		InnerShadow,
		Mask,
		Opacity,
		ColorMatrix,
		Custom
	};

	enum class SdBlurQuality : SdUInt8
	{
		Low,
		Balanced,
		High
	};

	enum class SdMaskType : SdUInt8
	{
		Rectangle,
		RoundedRectangle,
		Polygon,
		TextureAlpha,
		TextAlpha
	};

	enum class SdEffectPassType : SdUInt8
	{
		Fullscreen,
		Raster,
		Copy,
		Compute
	};

	struct SdMat4 final
	{
		float m[4][4] =
		{
			{ 1.0f, 0.0f, 0.0f, 0.0f },
			{ 0.0f, 1.0f, 0.0f, 0.0f },
			{ 0.0f, 0.0f, 1.0f, 0.0f },
			{ 0.0f, 0.0f, 0.0f, 1.0f }
		};
	};

	class SdEffectResourceCache final
	{
	private:
		Rhi::SdResourceSetLayoutHandle blurResourceSetLayout = {};
		Rhi::SdBufferHandle blurParamsBuffer = {};
		Rhi::SdPipelineHandle blurHorizontalPipeline = {};
		Rhi::SdPipelineHandle blurVerticalPipeline = {};
		Rhi::SdPipelineHandle compositePipeline = {};
		Rhi::SdResourceSetLayoutHandle shadowResourceSetLayout = {};
		Rhi::SdBufferHandle shadowParamsBuffer = {};
		Rhi::SdPipelineHandle shadowPipeline = {};
		Rhi::SdPipelineHandle maskPipeline = {};
		Rhi::SdSamplerHandle linearClampSampler = {};
		Rhi::SdSamplerHandle nearestClampSampler = {};

	public:
		Rhi::SdResourceSetLayoutHandle GetBlurResourceSetLayout() const noexcept { return blurResourceSetLayout; }
		Rhi::SdBufferHandle GetBlurParamsBuffer() const noexcept { return blurParamsBuffer; }
		Rhi::SdPipelineHandle GetBlurHorizontalPipeline(Rhi::SdTextureFormat) const noexcept { return blurHorizontalPipeline; }
		Rhi::SdPipelineHandle GetBlurVerticalPipeline(Rhi::SdTextureFormat) const noexcept { return blurVerticalPipeline; }
		Rhi::SdPipelineHandle GetCompositePipeline(Rhi::SdTextureFormat) const noexcept { return compositePipeline; }
		Rhi::SdResourceSetLayoutHandle GetShadowResourceSetLayout() const noexcept { return shadowResourceSetLayout; }
		Rhi::SdBufferHandle GetShadowParamsBuffer() const noexcept { return shadowParamsBuffer; }
		Rhi::SdPipelineHandle GetShadowPipeline(Rhi::SdTextureFormat) const noexcept { return shadowPipeline; }
		Rhi::SdPipelineHandle GetMaskPipeline(SdMaskType) const noexcept { return maskPipeline; }
		Rhi::SdSamplerHandle GetLinearClampSampler() const noexcept { return linearClampSampler; }
		Rhi::SdSamplerHandle GetNearestClampSampler() const noexcept { return nearestClampSampler; }

		void SetBlurResourceSetLayout(Rhi::SdResourceSetLayoutHandle layout) noexcept { blurResourceSetLayout = layout; }
		void SetBlurParamsBuffer(Rhi::SdBufferHandle buffer) noexcept { blurParamsBuffer = buffer; }
		void SetBlurHorizontalPipeline(Rhi::SdPipelineHandle pipeline) noexcept { blurHorizontalPipeline = pipeline; }
		void SetBlurVerticalPipeline(Rhi::SdPipelineHandle pipeline) noexcept { blurVerticalPipeline = pipeline; }
		void SetCompositePipeline(Rhi::SdPipelineHandle pipeline) noexcept { compositePipeline = pipeline; }
		void SetShadowResourceSetLayout(Rhi::SdResourceSetLayoutHandle layout) noexcept { shadowResourceSetLayout = layout; }
		void SetShadowParamsBuffer(Rhi::SdBufferHandle buffer) noexcept { shadowParamsBuffer = buffer; }
		void SetShadowPipeline(Rhi::SdPipelineHandle pipeline) noexcept { shadowPipeline = pipeline; }
		void SetMaskPipeline(Rhi::SdPipelineHandle pipeline) noexcept { maskPipeline = pipeline; }
		void SetLinearClampSampler(Rhi::SdSamplerHandle sampler) noexcept { linearClampSampler = sampler; }
		void SetNearestClampSampler(Rhi::SdSamplerHandle sampler) noexcept { nearestClampSampler = sampler; }
	};

	struct SdEffectBuildContext final
	{
		Rhi::SdRenderGraph& graph;
		Rhi::SdRenderGraphTexture source = {};
		Rhi::SdRenderGraphTexture backdrop = {};
		Rhi::SdRenderGraphTexture target = {};
		Rhi::SdRenderGraphTexture mask = {};
		SdRect sourceBounds = {};
		SdRect expandedBounds = {};
		SdRect clipRect = {};
		SdUInt32 pixelWidth = 0;
		SdUInt32 pixelHeight = 0;
		SdEffectResourceCache& resources;
		Rhi::SdTransientTexturePool& texturePool;
		const Rhi::SdGpuCaps& caps;
		Rhi::ISdGpuDevice* device = nullptr;
		float cornerRadius = 0.0f;
		SdCornerRadii cornerRadii = {};
	};

	class ISdEffect
	{
	public:
		virtual ~ISdEffect() = default;

		virtual SdEffectType GetType() const noexcept = 0;
		virtual bool RequiresIsolatedLayer() const noexcept = 0;
		virtual bool RequiresBackdropCapture() const noexcept = 0;
		virtual SdRect ExpandBounds(const SdRect& sourceBounds) const noexcept = 0;
		virtual void BuildGraph(SdEffectBuildContext& context) const = 0;
	};

	struct SdBlurParams final
	{
		SdVec2 texelSize = {};
		SdVec2 direction = { 1.0f, 0.0f };
		SdVec2 clipMin = {};
		SdVec2 clipMax = {};
		SdCornerRadii cornerRadii = {};
		float radius = 0.0f;
		float padding[3] = {};
		SdVec2 textureMin = {};
		SdVec2 textureSize = { 1.0f, 1.0f };
	};

	static_assert(sizeof(SdBlurParams) == 80);

	struct SdCompositeParams final
	{
		SdVec4 tintColor = { 1.0f, 1.0f, 1.0f, 1.0f };
		float opacity = 1.0f;
		float padding[3] = {};
	};

	struct SdShadowParams final
	{
		SdVec2 rectMin = {};
		SdVec2 rectMax = {};
		SdVec2 offset = {};
		float radius = 0.0f;
		float blurRadius = 0.0f;
		float spread = 0.0f;
		float outerOnly = 1.0f;
		float shadowMode = 0.0f;
		float padding[1] = {};
		SdVec4 color = {};
		SdVec4 cornerRadii = {};
	};

	static_assert(sizeof(SdShadowParams) == 80);

	inline Rhi::SdRenderGraphTextureDesc SdMakeEffectTextureDesc(
		SdUInt32 width,
		SdUInt32 height,
		Rhi::SdTextureFormat format,
		SdUtf8StringView debugName)
	{
		return {
			width,
			height,
			format,
			Rhi::SdTextureUsage::RenderTarget | Rhi::SdTextureUsage::ShaderRead,
			1,
			true,
			debugName
		};
	}
}
