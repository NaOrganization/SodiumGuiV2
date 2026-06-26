#pragma once

#include "Effects/SdEffectTypes.hpp"
#include "Core/SdCore.h"
#include "Render/SdRenderData.h"

#include <array>
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
			SdColorRgba tintColor = { 255, 255, 255, 0 };
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
	public:
		enum class TextureSlot : SdUInt8
		{
			BackdropCapture,
			BlurTempA,
			BlurTempB,
			Count
		};

		enum class ResourceSetSlot : SdUInt8
		{
			BlurSource,
			BlurTempA,
			BlurComposite,
			Count
		};

	private:
		static constexpr SdSize kPipelineFormatCount = static_cast<SdSize>(Rhi::SdTextureFormat::Depth32Float) + 1;
		static constexpr SdSize kTextureSlotCount = static_cast<SdSize>(TextureSlot::Count);
		static constexpr SdSize kResourceSetSlotCount = static_cast<SdSize>(ResourceSetSlot::Count);
		static constexpr SdUInt32 kTextureCacheExtentGranularity = 64;

		using PipelineByFormat = std::array<Rhi::SdPipelineHandle, kPipelineFormatCount>;

		struct CachedTexture final
		{
			Rhi::SdTextureHandle texture = {};
			Rhi::SdTextureDesc desc = {};

			bool Matches(const Rhi::SdTextureDesc& value) const noexcept
			{
				return texture.IsValid()
					&& desc.width >= value.width
					&& desc.height >= value.height
					&& desc.mipLevels == value.mipLevels
					&& desc.arraySize == value.arraySize
					&& desc.format == value.format
					&& desc.usage == value.usage
					&& desc.sampleCount == value.sampleCount
					&& desc.isTransient == value.isTransient;
			}
		};

		struct CachedParameterSet final
		{
			Rhi::SdBufferHandle paramsBuffer = {};
			Rhi::SdResourceSetHandle resourceSet = {};
			Rhi::SdTextureHandle inputTexture = {};
		};

		struct CachedParameterSetPool final
		{
			std::vector<CachedParameterSet> entries = {};
			SdUInt32 packetVersion = 0;
			SdUInt32 nextEntry = 0;
			bool hasPacketVersion = false;
		};

		Rhi::SdResourceSetLayoutHandle blurResourceSetLayout = {};
		PipelineByFormat blurHorizontalPipelines = {};
		PipelineByFormat blurVerticalPipelines = {};
		PipelineByFormat compositePipelines = {};
		Rhi::SdResourceSetLayoutHandle shadowResourceSetLayout = {};
		PipelineByFormat shadowPipelines = {};
		Rhi::SdPipelineHandle maskPipeline = {};
		Rhi::SdSamplerHandle linearClampSampler = {};
		Rhi::SdSamplerHandle nearestClampSampler = {};
		Rhi::SdShaderHandle fullscreenVertexShader = {};
		Rhi::SdShaderHandle blurPixelShader = {};
		Rhi::SdShaderHandle compositePixelShader = {};
		Rhi::SdShaderHandle shadowPixelShader = {};
		std::array<CachedTexture, kTextureSlotCount> cachedTextures = {};
		std::array<CachedParameterSetPool, kResourceSetSlotCount> cachedBlurParameterSets = {};
		CachedParameterSetPool cachedShadowParameterSets = {};

		static Rhi::SdPipelineHandle GetPipeline(const PipelineByFormat& pipelines, Rhi::SdTextureFormat format) noexcept
		{
			const SdSize index = static_cast<SdSize>(format);
			return index < pipelines.size() ? pipelines[index] : Rhi::SdPipelineHandle{};
		}

		static void SetPipeline(PipelineByFormat& pipelines, Rhi::SdTextureFormat format, Rhi::SdPipelineHandle pipeline) noexcept
		{
			const SdSize index = static_cast<SdSize>(format);
			if (index < pipelines.size())
				pipelines[index] = pipeline;
		}

		static void DestroyPipelines(Rhi::ISdGpuDevice& device, PipelineByFormat& pipelines) noexcept
		{
			for (Rhi::SdPipelineHandle& pipeline : pipelines)
			{
				if (pipeline.IsValid())
					device.DestroyPipeline(pipeline);
				pipeline = {};
			}
		}

		static bool IsValidSlot(TextureSlot slot) noexcept
		{
			return static_cast<SdSize>(slot) < kTextureSlotCount;
		}

		static bool IsValidSlot(ResourceSetSlot slot) noexcept
		{
			return static_cast<SdSize>(slot) < kResourceSetSlotCount;
		}

		static SdUInt32 RoundTextureCacheExtent(SdUInt32 value) noexcept
		{
			return ((value + kTextureCacheExtentGranularity - 1) / kTextureCacheExtentGranularity) * kTextureCacheExtentGranularity;
		}

		static void BeginParameterSetFrame(CachedParameterSetPool& pool, SdUInt32 packetVersion) noexcept
		{
			if (!pool.hasPacketVersion || pool.packetVersion != packetVersion)
			{
				pool.packetVersion = packetVersion;
				pool.nextEntry = 0;
				pool.hasPacketVersion = true;
			}
		}

		static CachedParameterSet* AcquireParameterSet(
			Rhi::ISdGpuDevice& device,
			CachedParameterSetPool& pool,
			SdUInt32 packetVersion,
			SdUInt64 paramsSize,
			SdUtf8StringView debugName)
		{
			BeginParameterSetFrame(pool, packetVersion);

			const SdSize entryIndex = static_cast<SdSize>(pool.nextEntry++);
			if (pool.entries.size() <= entryIndex)
				pool.entries.resize(entryIndex + 1);

			CachedParameterSet& entry = pool.entries[entryIndex];
			if (!entry.paramsBuffer.IsValid())
			{
				entry.paramsBuffer = device.CreateBuffer({
					paramsSize,
					static_cast<Rhi::SdBufferUsageFlags>(Rhi::SdBufferUsage::Uniform),
					Rhi::SdMemoryUsage::CpuToGpu,
					debugName
					}, nullptr);
				if (!entry.paramsBuffer.IsValid())
					return nullptr;
			}
			return &entry;
		}

		static void DestroyParameterSetPool(Rhi::ISdGpuDevice& device, CachedParameterSetPool& pool) noexcept
		{
			for (CachedParameterSet& entry : pool.entries)
			{
				if (entry.resourceSet.IsValid())
					device.DestroyResourceSet(entry.resourceSet);
				if (entry.paramsBuffer.IsValid())
					device.DestroyBuffer(entry.paramsBuffer);
				entry = {};
			}
			pool = {};
		}

	public:
		Rhi::SdResourceSetLayoutHandle GetBlurResourceSetLayout() const noexcept { return blurResourceSetLayout; }
		Rhi::SdPipelineHandle GetBlurHorizontalPipeline(Rhi::SdTextureFormat format) const noexcept { return GetPipeline(blurHorizontalPipelines, format); }
		Rhi::SdPipelineHandle GetBlurVerticalPipeline(Rhi::SdTextureFormat format) const noexcept { return GetPipeline(blurVerticalPipelines, format); }
		Rhi::SdPipelineHandle GetCompositePipeline(Rhi::SdTextureFormat format) const noexcept { return GetPipeline(compositePipelines, format); }
		Rhi::SdResourceSetLayoutHandle GetShadowResourceSetLayout() const noexcept { return shadowResourceSetLayout; }
		Rhi::SdPipelineHandle GetShadowPipeline(Rhi::SdTextureFormat format) const noexcept { return GetPipeline(shadowPipelines, format); }
		Rhi::SdPipelineHandle GetMaskPipeline(SdMaskType) const noexcept { return maskPipeline; }
		Rhi::SdSamplerHandle GetLinearClampSampler() const noexcept { return linearClampSampler; }
		Rhi::SdSamplerHandle GetNearestClampSampler() const noexcept { return nearestClampSampler; }
		Rhi::SdShaderHandle GetFullscreenVertexShader() const noexcept { return fullscreenVertexShader; }
		Rhi::SdShaderHandle GetBlurPixelShader() const noexcept { return blurPixelShader; }
		Rhi::SdShaderHandle GetCompositePixelShader() const noexcept { return compositePixelShader; }
		Rhi::SdShaderHandle GetShadowPixelShader() const noexcept { return shadowPixelShader; }

		void SetBlurResourceSetLayout(Rhi::SdResourceSetLayoutHandle layout) noexcept { blurResourceSetLayout = layout; }
		void SetBlurHorizontalPipeline(Rhi::SdTextureFormat format, Rhi::SdPipelineHandle pipeline) noexcept { SetPipeline(blurHorizontalPipelines, format, pipeline); }
		void SetBlurVerticalPipeline(Rhi::SdTextureFormat format, Rhi::SdPipelineHandle pipeline) noexcept { SetPipeline(blurVerticalPipelines, format, pipeline); }
		void SetCompositePipeline(Rhi::SdTextureFormat format, Rhi::SdPipelineHandle pipeline) noexcept { SetPipeline(compositePipelines, format, pipeline); }
		void SetShadowResourceSetLayout(Rhi::SdResourceSetLayoutHandle layout) noexcept { shadowResourceSetLayout = layout; }
		void SetShadowPipeline(Rhi::SdTextureFormat format, Rhi::SdPipelineHandle pipeline) noexcept { SetPipeline(shadowPipelines, format, pipeline); }
		void SetMaskPipeline(Rhi::SdPipelineHandle pipeline) noexcept { maskPipeline = pipeline; }
		void SetLinearClampSampler(Rhi::SdSamplerHandle sampler) noexcept { linearClampSampler = sampler; }
		void SetNearestClampSampler(Rhi::SdSamplerHandle sampler) noexcept { nearestClampSampler = sampler; }
		void SetFullscreenVertexShader(Rhi::SdShaderHandle shader) noexcept { fullscreenVertexShader = shader; }
		void SetBlurPixelShader(Rhi::SdShaderHandle shader) noexcept { blurPixelShader = shader; }
		void SetCompositePixelShader(Rhi::SdShaderHandle shader) noexcept { compositePixelShader = shader; }
		void SetShadowPixelShader(Rhi::SdShaderHandle shader) noexcept { shadowPixelShader = shader; }

		Rhi::SdTextureHandle GetOrCreateTexture(Rhi::ISdGpuDevice& device, TextureSlot slot, const Rhi::SdTextureDesc& desc)
		{
			if (!IsValidSlot(slot) || desc.width == 0 || desc.height == 0)
				return {};

			CachedTexture& cached = cachedTextures[static_cast<SdSize>(slot)];
			if (cached.Matches(desc))
				return cached.texture;

			if (cached.texture.IsValid())
				device.DestroyTexture(cached.texture);
			cached = {};
			Rhi::SdTextureDesc textureDesc = desc;
			textureDesc.width = RoundTextureCacheExtent(desc.width);
			textureDesc.height = RoundTextureCacheExtent(desc.height);
			cached.texture = device.CreateTexture(textureDesc);
			if (cached.texture.IsValid())
				cached.desc = textureDesc;
			return cached.texture;
		}

		Rhi::SdResourceSetHandle GetOrCreateBlurParameterResourceSet(
			Rhi::ISdGpuDevice& device,
			ResourceSetSlot slot,
			SdUInt32 packetVersion,
			Rhi::SdTextureHandle inputTexture,
			const void* params,
			SdUInt64 paramsSize,
			SdUtf8StringView debugName)
		{
			if (!IsValidSlot(slot)
				|| !inputTexture.IsValid()
				|| !params
				|| paramsSize == 0
				|| !blurResourceSetLayout.IsValid()
				|| !linearClampSampler.IsValid())
			{
				return {};
			}

			CachedParameterSet* entry = AcquireParameterSet(
				device,
				cachedBlurParameterSets[static_cast<SdSize>(slot)],
				packetVersion,
				paramsSize,
				debugName);
			if (!entry || !entry->paramsBuffer.IsValid())
				return {};
			if (!device.UpdateBuffer(entry->paramsBuffer, params, paramsSize, 0))
				return {};

			if (entry->resourceSet.IsValid() && entry->inputTexture == inputTexture)
				return entry->resourceSet;

			if (entry->resourceSet.IsValid())
				device.DestroyResourceSet(entry->resourceSet);
			entry->resourceSet = {};
			entry->inputTexture = {};

			const Rhi::SdBoundTexture textures[] =
			{
				{ 0, inputTexture }
			};
			const Rhi::SdBoundSampler samplers[] =
			{
				{ 0, linearClampSampler }
			};
			const Rhi::SdBoundBuffer buffers[] =
			{
				{ 0, entry->paramsBuffer, 0, paramsSize }
			};
			entry->resourceSet = device.CreateResourceSet({
				blurResourceSetLayout,
				textures,
				samplers,
				buffers,
				debugName
				});
			if (entry->resourceSet.IsValid())
				entry->inputTexture = inputTexture;
			return entry->resourceSet;
		}

		Rhi::SdResourceSetHandle GetOrCreateShadowParameterResourceSet(
			Rhi::ISdGpuDevice& device,
			SdUInt32 packetVersion,
			const void* params,
			SdUInt64 paramsSize,
			SdUtf8StringView debugName)
		{
			if (!params || paramsSize == 0 || !shadowResourceSetLayout.IsValid())
				return {};

			CachedParameterSet* entry = AcquireParameterSet(
				device,
				cachedShadowParameterSets,
				packetVersion,
				paramsSize,
				debugName);
			if (!entry || !entry->paramsBuffer.IsValid())
				return {};
			if (!device.UpdateBuffer(entry->paramsBuffer, params, paramsSize, 0))
				return {};

			if (entry->resourceSet.IsValid())
				return entry->resourceSet;

			const Rhi::SdBoundBuffer buffers[] =
			{
				{ 0, entry->paramsBuffer, 0, paramsSize }
			};
			entry->resourceSet = device.CreateResourceSet({
				shadowResourceSetLayout,
				{},
				{},
				buffers,
				debugName
				});
			return entry->resourceSet;
		}

		void Shutdown(Rhi::ISdGpuDevice& device) noexcept
		{
			DestroyParameterSetPool(device, cachedShadowParameterSets);
			for (CachedParameterSetPool& pool : cachedBlurParameterSets)
				DestroyParameterSetPool(device, pool);
			for (CachedTexture& cached : cachedTextures)
			{
				if (cached.texture.IsValid())
					device.DestroyTexture(cached.texture);
				cached = {};
			}
			if (maskPipeline.IsValid())
				device.DestroyPipeline(maskPipeline);
			DestroyPipelines(device, shadowPipelines);
			DestroyPipelines(device, compositePipelines);
			DestroyPipelines(device, blurVerticalPipelines);
			DestroyPipelines(device, blurHorizontalPipelines);
			if (shadowResourceSetLayout.IsValid())
				device.DestroyResourceSetLayout(shadowResourceSetLayout);
			if (blurResourceSetLayout.IsValid())
				device.DestroyResourceSetLayout(blurResourceSetLayout);
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
		SdVec4 tintColor = {};
		float saturation = 1.0f;
		float brightness = 1.0f;
		SdVec2 activeTextureSize = { 1.0f, 1.0f };
	};

	static_assert(sizeof(SdBlurParams) == 112);

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
