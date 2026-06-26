#pragma once

#include "Core/SdCore.h"

#include <cstddef>
#include <string_view>

namespace Sodium::Rhi
{
	using SdByte = std::byte;
	using SdShaderStageFlags = SdUInt32;
	using SdTextureUsageFlags = SdUInt32;
	using SdBufferUsageFlags = SdUInt32;
	using SdColorWriteFlags = SdUInt32;
	using SdTextureFormatFlags = SdUInt32;
	using SdShaderLanguageFlags = SdUInt32;

	enum class SdTextureFormat : SdUInt16
	{
		Unknown,
		R8Unorm,
		Rgba8Unorm,
		Rgba8UnormSrgb,
		Bgra8Unorm,
		Bgra8UnormSrgb,
		R16Float,
		Rgba16Float,
		Rgba32Float,
		Depth24Stencil8,
		Depth32Float
	};

	enum class SdTextureUsage : SdUInt32
	{
		None = 0,
		ShaderRead = 1 << 0,
		RenderTarget = 1 << 1,
		DepthStencil = 1 << 2,
		CopySrc = 1 << 3,
		CopyDst = 1 << 4,
		Storage = 1 << 5
	};

	constexpr SdTextureUsageFlags operator|(SdTextureUsage lhs, SdTextureUsage rhs) noexcept
	{
		return static_cast<SdTextureUsageFlags>(lhs) | static_cast<SdTextureUsageFlags>(rhs);
	}

	constexpr bool SdHasFlag(SdTextureUsageFlags flags, SdTextureUsage value) noexcept
	{
		return (flags & static_cast<SdTextureUsageFlags>(value)) != 0;
	}

	enum class SdBufferUsage : SdUInt32
	{
		None = 0,
		Vertex = 1 << 0,
		Index = 1 << 1,
		Uniform = 1 << 2,
		Storage = 1 << 3,
		CopySrc = 1 << 4,
		CopyDst = 1 << 5
	};

	constexpr SdBufferUsageFlags operator|(SdBufferUsage lhs, SdBufferUsage rhs) noexcept
	{
		return static_cast<SdBufferUsageFlags>(lhs) | static_cast<SdBufferUsageFlags>(rhs);
	}

	constexpr bool SdHasFlag(SdBufferUsageFlags flags, SdBufferUsage value) noexcept
	{
		return (flags & static_cast<SdBufferUsageFlags>(value)) != 0;
	}

	enum class SdMemoryUsage : SdUInt8
	{
		GpuOnly,
		CpuToGpu,
		GpuToCpu
	};

	enum class SdShaderStage : SdUInt8
	{
		Vertex,
		Pixel,
		Compute
	};

	enum class SdShaderLanguage : SdUInt8
	{
		Unknown,
		Hlsl,
		Spirv,
		Msl
	};

	enum class SdShaderStageFlag : SdUInt32
	{
		None = 0,
		Vertex = 1 << 0,
		Pixel = 1 << 1,
		Compute = 1 << 2,
		AllGraphics = 3
	};

	enum class SdShaderLanguageFlag : SdUInt32
	{
		None = 0,
		Hlsl = 1 << 0,
		Spirv = 1 << 1,
		Msl = 1 << 2
	};

	constexpr SdShaderStageFlags operator|(SdShaderStageFlag lhs, SdShaderStageFlag rhs) noexcept
	{
		return static_cast<SdShaderStageFlags>(lhs) | static_cast<SdShaderStageFlags>(rhs);
	}

	constexpr bool SdHasFlag(SdShaderStageFlags flags, SdShaderStageFlag value) noexcept
	{
		return (flags & static_cast<SdShaderStageFlags>(value)) != 0;
	}

	constexpr SdShaderLanguageFlags operator|(SdShaderLanguageFlag lhs, SdShaderLanguageFlag rhs) noexcept
	{
		return static_cast<SdShaderLanguageFlags>(lhs) | static_cast<SdShaderLanguageFlags>(rhs);
	}

	constexpr bool SdHasFlag(SdShaderLanguageFlags flags, SdShaderLanguageFlag value) noexcept
	{
		return (flags & static_cast<SdShaderLanguageFlags>(value)) != 0;
	}

	enum class SdFilterMode : SdUInt8
	{
		Nearest,
		Linear
	};

	enum class SdAddressMode : SdUInt8
	{
		Clamp,
		Repeat,
		Mirror
	};

	enum class SdVertexFormat : SdUInt8
	{
		Float2,
		Float3,
		Float4,
		UByte4Norm
	};

	enum class SdPrimitiveTopology : SdUInt8
	{
		TriangleList,
		TriangleStrip,
		LineList,
		LineStrip
	};

	enum class SdBlendFactor : SdUInt8
	{
		Zero,
		One,
		SrcAlpha,
		InvSrcAlpha,
		SrcColor,
		InvSrcColor,
		DstAlpha,
		InvDstAlpha,
		DstColor,
		InvDstColor
	};

	enum class SdBlendOp : SdUInt8
	{
		Add,
		Subtract,
		ReverseSubtract,
		Min,
		Max
	};

	enum class SdColorWrite : SdUInt32
	{
		None = 0,
		Red = 1 << 0,
		Green = 1 << 1,
		Blue = 1 << 2,
		Alpha = 1 << 3,
		All = 15
	};

	enum class SdCullMode : SdUInt8
	{
		None,
		Front,
		Back
	};

	enum class SdFillMode : SdUInt8
	{
		Solid,
		Wireframe
	};

	enum class SdCompareOp : SdUInt8
	{
		Always,
		Never,
		Less,
		LessEqual,
		Equal,
		GreaterEqual,
		Greater
	};

	enum class SdIndexFormat : SdUInt8
	{
		UInt16,
		UInt32
	};

	enum class SdShaderResourceType : SdUInt8
	{
		Texture2D,
		Sampler,
		UniformBuffer,
		StorageBuffer,
		StorageTexture
	};

	enum class SdLoadOp : SdUInt8
	{
		Load,
		Clear,
		DontCare
	};

	enum class SdStoreOp : SdUInt8
	{
		Store,
		DontCare
	};

	struct SdRectI final
	{
		SdInt32 left = 0;
		SdInt32 top = 0;
		SdInt32 right = 0;
		SdInt32 bottom = 0;

		constexpr SdUInt32 Width() const noexcept { return right > left ? static_cast<SdUInt32>(right - left) : 0; }
		constexpr SdUInt32 Height() const noexcept { return bottom > top ? static_cast<SdUInt32>(bottom - top) : 0; }
	};

	struct SdViewport final
	{
		float x = 0.0f;
		float y = 0.0f;
		float width = 0.0f;
		float height = 0.0f;
		float minDepth = 0.0f;
		float maxDepth = 1.0f;
	};

	struct SdGpuCaps final
	{
		bool supportsComputeShader = false;
		bool supportsStorageTexture = false;
		bool supportsIndependentBlend = false;
		bool supportsFramebufferFetch = false;
		bool supportsMultisampleRenderTarget = false;

		SdUInt32 maxTextureSize = 0;
		SdUInt32 maxColorAttachments = 1;
		SdUInt32 uniformBufferOffsetAlignment = 256;

		SdTextureFormatFlags supportedTextureFormats = 0;
		SdShaderLanguageFlags supportedShaderLanguages = 0;
	};

	struct SdTextureDesc final
	{
		SdUInt32 width = 1;
		SdUInt32 height = 1;
		SdUInt16 mipLevels = 1;
		SdUInt16 arraySize = 1;
		SdTextureFormat format = SdTextureFormat::Rgba8Unorm;
		SdTextureUsageFlags usage = static_cast<SdTextureUsageFlags>(SdTextureUsage::ShaderRead);
		SdUInt32 sampleCount = 1;
		bool isTransient = false;
		SdUtf8StringView debugName = {};
	};

	struct SdBufferDesc final
	{
		SdUInt64 size = 0;
		SdBufferUsageFlags usage = 0;
		SdMemoryUsage memoryUsage = SdMemoryUsage::GpuOnly;
		SdUtf8StringView debugName = {};
	};

	struct SdShaderDesc final
	{
		SdShaderStage stage = SdShaderStage::Vertex;
		SdSpan<const SdByte> bytecode = {};
		SdUtf8StringView entryPoint = {};
		SdUtf8StringView debugName = {};
	};

	struct SdShaderSourceDesc final
	{
		SdShaderStage stage = SdShaderStage::Vertex;
		SdShaderLanguage language = SdShaderLanguage::Hlsl;
		SdUtf8StringView source = {};
		SdUtf8StringView entryPoint = SODIUM_STRING("main");
		SdUtf8StringView targetProfile = {};
		SdUtf8StringView debugName = {};
	};

	struct SdSamplerDesc final
	{
		SdFilterMode minFilter = SdFilterMode::Linear;
		SdFilterMode magFilter = SdFilterMode::Linear;
		SdFilterMode mipFilter = SdFilterMode::Linear;
		SdAddressMode addressU = SdAddressMode::Clamp;
		SdAddressMode addressV = SdAddressMode::Clamp;
		SdAddressMode addressW = SdAddressMode::Clamp;
		SdUtf8StringView debugName = {};
	};

	struct SdVertexAttributeDesc final
	{
		SdUtf8StringView semanticName = {};
		SdUInt32 semanticIndex = 0;
		SdVertexFormat format = SdVertexFormat::Float2;
		SdUInt32 offset = 0;
		SdUInt32 bufferSlot = 0;
	};

	struct SdVertexLayoutDesc final
	{
		SdSpan<const SdVertexAttributeDesc> attributes = {};
		SdUtf8StringView debugName = {};
	};

	struct SdBlendAttachmentDesc final
	{
		bool blendEnabled = true;
		SdBlendFactor srcColor = SdBlendFactor::SrcAlpha;
		SdBlendFactor dstColor = SdBlendFactor::InvSrcAlpha;
		SdBlendOp colorOp = SdBlendOp::Add;
		SdBlendFactor srcAlpha = SdBlendFactor::One;
		SdBlendFactor dstAlpha = SdBlendFactor::InvSrcAlpha;
		SdBlendOp alphaOp = SdBlendOp::Add;
		SdColorWriteFlags writeMask = static_cast<SdColorWriteFlags>(SdColorWrite::All);
	};

	struct SdBlendStateDesc final
	{
		bool alphaToCoverage = false;
		SdBlendAttachmentDesc color = {};
	};

	struct SdRasterStateDesc final
	{
		SdFillMode fillMode = SdFillMode::Solid;
		SdCullMode cullMode = SdCullMode::None;
		bool scissorEnabled = true;
		bool depthClipEnabled = true;
	};

	struct SdDepthStencilStateDesc final
	{
		bool depthTestEnabled = false;
		bool depthWriteEnabled = false;
		SdCompareOp depthCompare = SdCompareOp::Always;
	};

	struct SdGraphicsPipelineDesc final
	{
		SdShaderHandle vertexShader = {};
		SdShaderHandle pixelShader = {};
		SdVertexLayoutHandle vertexLayout = {};
		SdResourceSetLayoutHandle resourceSetLayout = {};
		SdBlendStateDesc blendState = {};
		SdRasterStateDesc rasterState = {};
		SdDepthStencilStateDesc depthStencilState = {};
		SdPrimitiveTopology topology = SdPrimitiveTopology::TriangleList;
		SdTextureFormat colorFormat = SdTextureFormat::Bgra8Unorm;
		SdTextureFormat depthFormat = SdTextureFormat::Unknown;
		SdUInt32 sampleCount = 1;
		SdUtf8StringView debugName = {};
	};

	struct SdShaderBindingDesc final
	{
		SdUtf8StringView name = {};
		SdShaderResourceType type = SdShaderResourceType::Texture2D;
		SdUInt32 set = 0;
		SdUInt32 binding = 0;
		SdShaderStageFlags stages = static_cast<SdShaderStageFlags>(SdShaderStageFlag::Pixel);
	};

	struct SdResourceSetLayoutDesc final
	{
		SdSpan<const SdShaderBindingDesc> bindings = {};
		SdUtf8StringView debugName = {};
	};

	struct SdBoundTexture final
	{
		SdUInt32 binding = 0;
		SdTextureHandle texture = {};
	};

	struct SdBoundSampler final
	{
		SdUInt32 binding = 0;
		SdSamplerHandle sampler = {};
	};

	struct SdBoundBuffer final
	{
		SdUInt32 binding = 0;
		SdBufferHandle buffer = {};
		SdUInt64 offset = 0;
		SdUInt64 size = 0;
	};

	struct SdResourceSetDesc final
	{
		SdResourceSetLayoutHandle layout = {};
		SdSpan<const SdBoundTexture> textures = {};
		SdSpan<const SdBoundSampler> samplers = {};
		SdSpan<const SdBoundBuffer> buffers = {};
		SdUtf8StringView debugName = {};
	};

	struct SdRenderPassColorAttachment final
	{
		SdTextureHandle texture = {};
		SdLoadOp loadOp = SdLoadOp::Clear;
		SdStoreOp storeOp = SdStoreOp::Store;
		SdColorLinear clearColor = {};
	};

	struct SdRenderPassDesc final
	{
		SdSpan<const SdRenderPassColorAttachment> colorAttachments = {};
		SdTextureHandle depthStencil = {};
		SdRectI renderArea = {};
		SdUtf8StringView debugName = {};
	};

	class ISdCommandEncoder
	{
	public:
		virtual ~ISdCommandEncoder() = default;

		virtual void BeginRenderPass(const SdRenderPassDesc& desc) = 0;
		virtual void EndRenderPass() = 0;
		virtual void SetPipeline(SdPipelineHandle pipeline) = 0;
		virtual void SetResourceSet(SdUInt32 setIndex, SdResourceSetHandle resourceSet) = 0;
		virtual void SetVertexBuffer(SdUInt32 slot, SdBufferHandle buffer, SdUInt32 stride, SdUInt64 offset) = 0;
		virtual void SetIndexBuffer(SdBufferHandle buffer, SdIndexFormat format, SdUInt64 offset) = 0;
		virtual void SetViewport(const SdViewport& viewport) = 0;
		virtual void SetScissorRect(const SdRectI& rect) = 0;
		virtual void Draw(SdUInt32 vertexCount, SdUInt32 instanceCount, SdUInt32 firstVertex, SdUInt32 firstInstance) = 0;
		virtual void DrawIndexed(SdUInt32 indexCount, SdUInt32 instanceCount, SdUInt32 firstIndex, SdInt32 vertexOffset, SdUInt32 firstInstance) = 0;
	};

	class ISdGpuDevice
	{
	public:
		virtual ~ISdGpuDevice() = default;

		virtual const SdGpuCaps& GetCaps() const noexcept = 0;
		virtual SdTextureHandle CreateTexture(const SdTextureDesc& desc) = 0;
		virtual void DestroyTexture(SdTextureHandle texture) = 0;
		virtual bool UpdateTexture(SdTextureHandle texture, const void* pixels, SdUInt32 rowPitch) = 0;
		virtual bool CopyCurrentRenderTargetToTexture(SdTextureHandle, const SdRectI&) { return false; }
		virtual SdBufferHandle CreateBuffer(const SdBufferDesc& desc, const void* initialData) = 0;
		virtual void DestroyBuffer(SdBufferHandle buffer) = 0;
		virtual bool UpdateBuffer(SdBufferHandle buffer, const void* data, SdUInt64 size, SdUInt64 offset) = 0;
		virtual SdShaderHandle CreateShader(const SdShaderDesc& desc) = 0;
		virtual SdShaderHandle CreateShaderFromSource(const SdShaderSourceDesc&) { return {}; }
		virtual void DestroyShader(SdShaderHandle shader) = 0;
		virtual SdSamplerHandle CreateSampler(const SdSamplerDesc& desc) = 0;
		virtual void DestroySampler(SdSamplerHandle sampler) = 0;
		virtual SdVertexLayoutHandle CreateVertexLayout(const SdVertexLayoutDesc& desc) = 0;
		virtual void DestroyVertexLayout(SdVertexLayoutHandle layout) = 0;
		virtual SdResourceSetLayoutHandle CreateResourceSetLayout(const SdResourceSetLayoutDesc& desc) = 0;
		virtual void DestroyResourceSetLayout(SdResourceSetLayoutHandle layout) = 0;
		virtual SdResourceSetHandle CreateResourceSet(const SdResourceSetDesc& desc) = 0;
		virtual void DestroyResourceSet(SdResourceSetHandle resourceSet) = 0;
		virtual SdPipelineHandle CreateGraphicsPipeline(const SdGraphicsPipelineDesc& desc) = 0;
		virtual void DestroyPipeline(SdPipelineHandle pipeline) = 0;
	};
}

namespace Sodium
{
	using Rhi::SdTextureFormat;
	using Rhi::SdTextureUsage;
	using Rhi::SdMemoryUsage;
	using Rhi::SdShaderStage;
	using Rhi::SdShaderStageFlag;
	using Rhi::SdShaderLanguage;
	using Rhi::SdShaderLanguageFlag;
	using Rhi::SdFilterMode;
	using Rhi::SdAddressMode;
	using Rhi::SdVertexFormat;
}
