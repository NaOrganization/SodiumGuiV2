#pragma once

#include "Effects/SdEffectTypes.hpp"
#include "Core/SdCore.h"
#include "Render/SdRenderData.h"

#include <vector>

namespace Sodium
{
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

	struct SdBlurEffectParameters final
	{
		float radius = 8.0f;
		SdBlurQuality quality = SdBlurQuality::Balanced;
		bool clampToEdge = true;
		float cornerRadius = 0.0f;
		SdCornerRadii cornerRadii = {};
	};

	struct SdBackdropBlurEffectParameters final
	{
		float radius = 16.0f;
		SdColorRgba tintColor = { 255, 255, 255, 48 };
		float saturation = 1.2f;
		float brightness = 1.0f;
		float cornerRadius = 0.0f;
		SdCornerRadii cornerRadii = {};
	};

	struct SdDropShadowEffectParameters final
	{
		SdVec2 offset = { 0.0f, 4.0f };
		float radius = 12.0f;
		float spread = 0.0f;
		SdColorRgba color = { 0, 0, 0, 128 };
		SdCornerRadii cornerRadii = {};
	};

	struct SdInnerShadowEffectParameters final
	{
		SdVec2 offset = { 0.0f, 2.0f };
		float radius = 8.0f;
		float spread = 0.0f;
		SdColorRgba color = { 0, 0, 0, 96 };
		SdCornerRadii cornerRadii = {};
	};

	struct SdEffectRenderLayer final
	{
		SdRenderLayerId id = SdInvalidRenderLayerId;
		Rhi::SdTextureHandle texture = {};
		Rhi::SdTextureFormat format = Rhi::SdTextureFormat::Unknown;
		SdUInt32 width = 0;
		SdUInt32 height = 0;
		SdRect bounds = {};

		bool IsValid() const noexcept { return texture.IsValid(); }
	};

	class ISdRenderLayerProvider
	{
	public:
		virtual ~ISdRenderLayerProvider() = default;

		virtual SdEffectRenderLayer FindRenderLayer(SdRenderLayerId layerId) const noexcept = 0;
	};

	class SdEffectResourceCache;

	struct SdEffectLayerRequirements final
	{
		bool requiresSource = true;
		bool requiresTarget = true;
		bool requiresBackdrop = false;
		bool requiresMask = false;
		bool requiresIsolatedLayer = false;
		SdRect expandedBounds = {};
	};

	struct SdEffectInitContext final
	{
		Rhi::ISdGpuDevice& device;
		const Rhi::SdGpuCaps& caps;
	};

	struct SdEffectCommandView final
	{
		const SdApplyEffectPayload& payload;
		std::span<const std::byte> parameters = {};
		const SdRenderPacket& packet;
	};

	struct SdEffectApplyContext final
	{
		Rhi::ISdGpuDevice& device;
		Rhi::ISdCommandEncoder& encoder;
		const SdRendererFrameInfo& frameInfo;
		const SdApplyEffectPayload& payload;
		std::span<const std::byte> parameters = {};
		const SdRenderPacket& packet;
		const ISdRenderLayerProvider* layers = nullptr;
	};

	class ISdEffector
	{
	public:
		virtual ~ISdEffector() = default;

		virtual SdEffectTypeId GetTypeId() const noexcept = 0;
		virtual bool Initialize(const SdEffectInitContext& context) = 0;
		virtual void Shutdown(Rhi::ISdGpuDevice& device) noexcept = 0;
		virtual SdEffectLayerRequirements QueryLayerRequirements(const SdEffectCommandView& command) const noexcept = 0;
		virtual bool Apply(const SdEffectApplyContext& context) = 0;
	};

	template<class TParameters>
	const TParameters* SdTryReadEffectParameters(std::span<const std::byte> bytes) noexcept
	{
		static_assert(std::is_trivially_copyable_v<TParameters>, SODIUM_STRING("Effect command parameters must be trivially copyable."));
		if (bytes.size_bytes() != sizeof(TParameters))
			return nullptr;
		return std::launder(reinterpret_cast<const TParameters*>(bytes.data()));
	}

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
		Rhi::SdShaderHandle fullscreenVertexShader = {};
		Rhi::SdShaderHandle blurPixelShader = {};
		Rhi::SdShaderHandle compositePixelShader = {};
		Rhi::SdShaderHandle shadowPixelShader = {};

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
		Rhi::SdShaderHandle GetFullscreenVertexShader() const noexcept { return fullscreenVertexShader; }
		Rhi::SdShaderHandle GetBlurPixelShader() const noexcept { return blurPixelShader; }
		Rhi::SdShaderHandle GetCompositePixelShader() const noexcept { return compositePixelShader; }
		Rhi::SdShaderHandle GetShadowPixelShader() const noexcept { return shadowPixelShader; }

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
		void SetFullscreenVertexShader(Rhi::SdShaderHandle shader) noexcept { fullscreenVertexShader = shader; }
		void SetBlurPixelShader(Rhi::SdShaderHandle shader) noexcept { blurPixelShader = shader; }
		void SetCompositePixelShader(Rhi::SdShaderHandle shader) noexcept { compositePixelShader = shader; }
		void SetShadowPixelShader(Rhi::SdShaderHandle shader) noexcept { shadowPixelShader = shader; }

		void Shutdown(Rhi::ISdGpuDevice& device) noexcept
		{
			if (maskPipeline.IsValid())
				device.DestroyPipeline(maskPipeline);
			if (shadowPipeline.IsValid())
				device.DestroyPipeline(shadowPipeline);
			if (compositePipeline.IsValid())
				device.DestroyPipeline(compositePipeline);
			if (blurVerticalPipeline.IsValid())
				device.DestroyPipeline(blurVerticalPipeline);
			if (blurHorizontalPipeline.IsValid())
				device.DestroyPipeline(blurHorizontalPipeline);
			if (shadowResourceSetLayout.IsValid())
				device.DestroyResourceSetLayout(shadowResourceSetLayout);
			if (blurResourceSetLayout.IsValid())
				device.DestroyResourceSetLayout(blurResourceSetLayout);
			if (shadowParamsBuffer.IsValid())
				device.DestroyBuffer(shadowParamsBuffer);
			if (blurParamsBuffer.IsValid())
				device.DestroyBuffer(blurParamsBuffer);
			if (nearestClampSampler.IsValid())
				device.DestroySampler(nearestClampSampler);
			if (linearClampSampler.IsValid())
				device.DestroySampler(linearClampSampler);
			if (shadowPixelShader.IsValid())
				device.DestroyShader(shadowPixelShader);
			if (compositePixelShader.IsValid())
				device.DestroyShader(compositePixelShader);
			if (blurPixelShader.IsValid())
				device.DestroyShader(blurPixelShader);
			if (fullscreenVertexShader.IsValid())
				device.DestroyShader(fullscreenVertexShader);
			*this = {};
		}
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

}
