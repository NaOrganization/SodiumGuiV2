#pragma once

#include "Rhi/SdRhi.h"

#include <Windows.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

namespace Sodium::Backends
{
	class SdDx11GpuDevice;

	struct SdDx11GpuDeviceConfig final
	{
		ID3D11Device* device = nullptr;
		ID3D11DeviceContext* deviceContext = nullptr;
	};

	class SdDx11CommandEncoder final : public Rhi::ISdCommandEncoder
	{
	private:
		SdDx11GpuDevice* device = nullptr;
		ID3D11DeviceContext* context = nullptr;

	public:
		SdDx11CommandEncoder() = default;
		SdDx11CommandEncoder(SdDx11GpuDevice& gpuDevice, ID3D11DeviceContext* deviceContext)
			: device(&gpuDevice), context(deviceContext) {}

		void Reset(SdDx11GpuDevice& gpuDevice, ID3D11DeviceContext* deviceContext) noexcept
		{
			device = &gpuDevice;
			context = deviceContext;
		}

		void BeginRenderPass(const Rhi::SdRenderPassDesc& desc) override;
		void EndRenderPass() override;
		void SetPipeline(Rhi::SdPipelineHandle pipeline) override;
		void SetResourceSet(SdUInt32 setIndex, Rhi::SdResourceSetHandle resourceSet) override;
		void SetVertexBuffer(SdUInt32 slot, Rhi::SdBufferHandle buffer, SdUInt32 stride, SdUInt64 offset) override;
		void SetIndexBuffer(Rhi::SdBufferHandle buffer, Rhi::SdIndexFormat format, SdUInt64 offset) override;
		void SetViewport(const Rhi::SdViewport& viewport) override;
		void SetScissorRect(const Rhi::SdRectI& rect) override;
		void Draw(SdUInt32 vertexCount, SdUInt32 instanceCount, SdUInt32 firstVertex, SdUInt32 firstInstance) override;
		void DrawIndexed(SdUInt32 indexCount, SdUInt32 instanceCount, SdUInt32 firstIndex, SdInt32 vertexOffset, SdUInt32 firstInstance) override;
	};

	class SdDx11GpuDevice final : public Rhi::ISdGpuDevice
	{
	private:
		struct TextureEntry final
		{
			Microsoft::WRL::ComPtr<ID3D11Texture2D> resource = {};
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv = {};
			Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv = {};
			Microsoft::WRL::ComPtr<ID3D11DepthStencilView> dsv = {};
			Rhi::SdTextureDesc desc = {};
			SdUInt32 generation = 0;
			bool occupied = false;
			bool imported = false;
		};

		struct BufferEntry final
		{
			Microsoft::WRL::ComPtr<ID3D11Buffer> buffer = {};
			Rhi::SdBufferDesc desc = {};
			SdUInt32 generation = 0;
			bool occupied = false;
		};

		struct ShaderEntry final
		{
			Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader = {};
			Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader = {};
			std::vector<std::byte> bytecode = {};
			Rhi::SdShaderStage stage = Rhi::SdShaderStage::Vertex;
			SdUInt32 generation = 0;
			bool occupied = false;
		};

		struct SamplerEntry final
		{
			Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler = {};
			SdUInt32 generation = 0;
			bool occupied = false;
		};

		struct VertexLayoutEntry final
		{
			std::vector<Rhi::SdVertexAttributeDesc> attributes = {};
			std::vector<std::string> semanticNames = {};
			SdUInt32 generation = 0;
			bool occupied = false;
		};

		struct ResourceSetLayoutEntry final
		{
			std::vector<Rhi::SdShaderBindingDesc> bindings = {};
			SdUInt32 generation = 0;
			bool occupied = false;
		};

		struct ResourceSetEntry final
		{
			Rhi::SdResourceSetLayoutHandle layout = {};
			std::vector<Rhi::SdBoundTexture> textures = {};
			std::vector<Rhi::SdBoundSampler> samplers = {};
			std::vector<Rhi::SdBoundBuffer> buffers = {};
			SdUInt32 generation = 0;
			bool occupied = false;
		};

		struct PipelineEntry final
		{
			Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout = {};
			Microsoft::WRL::ComPtr<ID3D11BlendState> blendState = {};
			Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizerState = {};
			Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthStencilState = {};
			Rhi::SdShaderHandle vertexShader = {};
			Rhi::SdShaderHandle pixelShader = {};
			Rhi::SdResourceSetLayoutHandle resourceSetLayout = {};
			Rhi::SdPrimitiveTopology topology = Rhi::SdPrimitiveTopology::TriangleList;
			SdUInt32 generation = 0;
			bool occupied = false;
		};

		Microsoft::WRL::ComPtr<ID3D11Device> nativeDevice = {};
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> nativeContext = {};
		Rhi::SdGpuCaps caps = {};
		std::vector<TextureEntry> textures = {};
		std::vector<BufferEntry> buffers = {};
		std::vector<ShaderEntry> shaders = {};
		std::vector<SamplerEntry> samplers = {};
		std::vector<VertexLayoutEntry> vertexLayouts = {};
		std::vector<ResourceSetLayoutEntry> resourceSetLayouts = {};
		std::vector<ResourceSetEntry> resourceSets = {};
		std::vector<PipelineEntry> pipelines = {};
		SdDx11CommandEncoder immediateEncoder = {};

		template<typename Entry>
		static Rhi::SdGpuHandle Allocate(std::vector<Entry>& entries)
		{
			SdUInt32 index = 1;
			for (; index < entries.size(); ++index)
			{
				if (!entries[index].occupied)
					break;
			}
			if (entries.size() <= index)
				entries.resize(index + 1);
			Entry& entry = entries[index];
			entry.generation = entry.generation == 0 ? 1 : entry.generation + 1;
			entry.occupied = true;
			return Rhi::SdGpuHandle(index, entry.generation);
		}

		template<typename Entry>
		static Entry* Find(std::vector<Entry>& entries, Rhi::SdGpuHandle handle) noexcept
		{
			if (!handle.IsValid() || handle.index >= entries.size())
				return nullptr;
			Entry& entry = entries[handle.index];
			if (!entry.occupied || entry.generation != handle.generation)
				return nullptr;
			return &entry;
		}

		template<typename Entry>
		static const Entry* Find(const std::vector<Entry>& entries, Rhi::SdGpuHandle handle) noexcept
		{
			if (!handle.IsValid() || handle.index >= entries.size())
				return nullptr;
			const Entry& entry = entries[handle.index];
			if (!entry.occupied || entry.generation != handle.generation)
				return nullptr;
			return &entry;
		}

		template<typename Entry>
		static void Release(std::vector<Entry>& entries, Rhi::SdGpuHandle handle)
		{
			Entry* entry = Find(entries, handle);
			if (!entry)
				return;
			*entry = {};
			entry->generation = handle.generation + 1;
		}

		static DXGI_FORMAT MapTextureFormat(Rhi::SdTextureFormat format) noexcept
		{
			switch (format)
			{
			case Rhi::SdTextureFormat::R8Unorm: return DXGI_FORMAT_R8_UNORM;
			case Rhi::SdTextureFormat::Rgba8Unorm: return DXGI_FORMAT_R8G8B8A8_UNORM;
			case Rhi::SdTextureFormat::Rgba8UnormSrgb: return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
			case Rhi::SdTextureFormat::Bgra8Unorm: return DXGI_FORMAT_B8G8R8A8_UNORM;
			case Rhi::SdTextureFormat::Bgra8UnormSrgb: return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
			case Rhi::SdTextureFormat::R16Float: return DXGI_FORMAT_R16_FLOAT;
			case Rhi::SdTextureFormat::Rgba16Float: return DXGI_FORMAT_R16G16B16A16_FLOAT;
			case Rhi::SdTextureFormat::Rgba32Float: return DXGI_FORMAT_R32G32B32A32_FLOAT;
			case Rhi::SdTextureFormat::Depth24Stencil8: return DXGI_FORMAT_D24_UNORM_S8_UINT;
			case Rhi::SdTextureFormat::Depth32Float: return DXGI_FORMAT_D32_FLOAT;
			default: return DXGI_FORMAT_UNKNOWN;
			}
		}

		static DXGI_FORMAT MapVertexFormat(Rhi::SdVertexFormat format) noexcept
		{
			switch (format)
			{
			case Rhi::SdVertexFormat::Float2: return DXGI_FORMAT_R32G32_FLOAT;
			case Rhi::SdVertexFormat::Float3: return DXGI_FORMAT_R32G32B32_FLOAT;
			case Rhi::SdVertexFormat::Float4: return DXGI_FORMAT_R32G32B32A32_FLOAT;
			case Rhi::SdVertexFormat::UByte4Norm: return DXGI_FORMAT_R8G8B8A8_UNORM;
			default: return DXGI_FORMAT_UNKNOWN;
			}
		}

		static D3D11_PRIMITIVE_TOPOLOGY MapTopology(Rhi::SdPrimitiveTopology topology) noexcept
		{
			switch (topology)
			{
			case Rhi::SdPrimitiveTopology::TriangleStrip: return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
			case Rhi::SdPrimitiveTopology::LineList: return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
			case Rhi::SdPrimitiveTopology::LineStrip: return D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
			case Rhi::SdPrimitiveTopology::TriangleList:
			default: return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			}
		}

		static D3D11_BLEND MapBlendFactor(Rhi::SdBlendFactor factor) noexcept
		{
			switch (factor)
			{
			case Rhi::SdBlendFactor::Zero: return D3D11_BLEND_ZERO;
			case Rhi::SdBlendFactor::One: return D3D11_BLEND_ONE;
			case Rhi::SdBlendFactor::SrcColor: return D3D11_BLEND_SRC_COLOR;
			case Rhi::SdBlendFactor::InvSrcColor: return D3D11_BLEND_INV_SRC_COLOR;
			case Rhi::SdBlendFactor::DstColor: return D3D11_BLEND_DEST_COLOR;
			case Rhi::SdBlendFactor::InvDstColor: return D3D11_BLEND_INV_DEST_COLOR;
			case Rhi::SdBlendFactor::DstAlpha: return D3D11_BLEND_DEST_ALPHA;
			case Rhi::SdBlendFactor::InvDstAlpha: return D3D11_BLEND_INV_DEST_ALPHA;
			case Rhi::SdBlendFactor::SrcAlpha: return D3D11_BLEND_SRC_ALPHA;
			case Rhi::SdBlendFactor::InvSrcAlpha:
			default: return D3D11_BLEND_INV_SRC_ALPHA;
			}
		}

		static D3D11_BLEND_OP MapBlendOp(Rhi::SdBlendOp op) noexcept
		{
			switch (op)
			{
			case Rhi::SdBlendOp::Subtract: return D3D11_BLEND_OP_SUBTRACT;
			case Rhi::SdBlendOp::ReverseSubtract: return D3D11_BLEND_OP_REV_SUBTRACT;
			case Rhi::SdBlendOp::Min: return D3D11_BLEND_OP_MIN;
			case Rhi::SdBlendOp::Max: return D3D11_BLEND_OP_MAX;
			case Rhi::SdBlendOp::Add:
			default: return D3D11_BLEND_OP_ADD;
			}
		}

		static D3D11_COMPARISON_FUNC MapCompare(Rhi::SdCompareOp op) noexcept
		{
			switch (op)
			{
			case Rhi::SdCompareOp::Never: return D3D11_COMPARISON_NEVER;
			case Rhi::SdCompareOp::Less: return D3D11_COMPARISON_LESS;
			case Rhi::SdCompareOp::LessEqual: return D3D11_COMPARISON_LESS_EQUAL;
			case Rhi::SdCompareOp::Equal: return D3D11_COMPARISON_EQUAL;
			case Rhi::SdCompareOp::GreaterEqual: return D3D11_COMPARISON_GREATER_EQUAL;
			case Rhi::SdCompareOp::Greater: return D3D11_COMPARISON_GREATER;
			case Rhi::SdCompareOp::Always:
			default: return D3D11_COMPARISON_ALWAYS;
			}
		}

		static D3D11_FILTER MapFilter(const Rhi::SdSamplerDesc& desc) noexcept
		{
			const bool linear = desc.minFilter == Rhi::SdFilterMode::Linear
				&& desc.magFilter == Rhi::SdFilterMode::Linear
				&& desc.mipFilter == Rhi::SdFilterMode::Linear;
			return linear ? D3D11_FILTER_MIN_MAG_MIP_LINEAR : D3D11_FILTER_MIN_MAG_MIP_POINT;
		}

		static D3D11_TEXTURE_ADDRESS_MODE MapAddress(Rhi::SdAddressMode mode) noexcept
		{
			switch (mode)
			{
			case Rhi::SdAddressMode::Repeat: return D3D11_TEXTURE_ADDRESS_WRAP;
			case Rhi::SdAddressMode::Mirror: return D3D11_TEXTURE_ADDRESS_MIRROR;
			case Rhi::SdAddressMode::Clamp:
			default: return D3D11_TEXTURE_ADDRESS_CLAMP;
			}
		}

		void InitializeCaps()
		{
			caps.maxColorAttachments = D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT;
			caps.maxTextureSize = D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION;
			caps.uniformBufferOffsetAlignment = 16;
			caps.supportsComputeShader = false;
			caps.supportsStorageTexture = false;
			caps.supportsIndependentBlend = false;
			caps.supportsFramebufferFetch = false;
			caps.supportsMultisampleRenderTarget = true;
		}

	public:
		using Config = SdDx11GpuDeviceConfig;

		SdDx11GpuDevice() = default;
		~SdDx11GpuDevice() override { Shutdown(); }

		bool Initialize(const Config& config)
		{
			if (!config.device || !config.deviceContext)
				return false;

			nativeDevice = config.device;
			nativeContext = config.deviceContext;
			InitializeCaps();
			immediateEncoder.Reset(*this, nativeContext.Get());
			return true;
		}

		void Shutdown()
		{
			pipelines.clear();
			resourceSets.clear();
			resourceSetLayouts.clear();
			vertexLayouts.clear();
			samplers.clear();
			shaders.clear();
			buffers.clear();
			textures.clear();
			nativeContext.Reset();
			nativeDevice.Reset();
			caps = {};
		}

		bool IsInitialized() const noexcept { return nativeDevice.Get() && nativeContext.Get(); }
		Rhi::ISdCommandEncoder& GetImmediateEncoder() noexcept { return immediateEncoder; }
		ID3D11Device* GetNativeDevice() const noexcept { return nativeDevice.Get(); }
		ID3D11DeviceContext* GetNativeContext() const noexcept { return nativeContext.Get(); }

		const Rhi::SdGpuCaps& GetCaps() const noexcept override { return caps; }

		Rhi::SdTextureHandle ImportTexture2D(ID3D11Texture2D* texture, const Rhi::SdTextureDesc& desc)
		{
			if (!nativeDevice || !texture)
				return {};

			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv = {};
			if (Rhi::SdHasFlag(desc.usage, Rhi::SdTextureUsage::ShaderRead)
				&& FAILED(nativeDevice->CreateShaderResourceView(texture, nullptr, srv.GetAddressOf())))
			{
				return {};
			}

			Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv = {};
			if (Rhi::SdHasFlag(desc.usage, Rhi::SdTextureUsage::RenderTarget)
				&& FAILED(nativeDevice->CreateRenderTargetView(texture, nullptr, rtv.GetAddressOf())))
			{
				return {};
			}

			Microsoft::WRL::ComPtr<ID3D11DepthStencilView> dsv = {};
			if (Rhi::SdHasFlag(desc.usage, Rhi::SdTextureUsage::DepthStencil)
				&& FAILED(nativeDevice->CreateDepthStencilView(texture, nullptr, dsv.GetAddressOf())))
			{
				return {};
			}

			Rhi::SdTextureHandle handle = Allocate(textures);
			TextureEntry& entry = textures[handle.index];
			entry.resource = texture;
			entry.srv = srv;
			entry.rtv = rtv;
			entry.dsv = dsv;
			entry.desc = desc;
			entry.imported = true;
			return handle;
		}

		Rhi::SdTextureHandle CreateTexture(const Rhi::SdTextureDesc& desc) override
		{
			if (!nativeDevice || desc.width == 0 || desc.height == 0)
				return {};

			const DXGI_FORMAT format = MapTextureFormat(desc.format);
			if (format == DXGI_FORMAT_UNKNOWN)
				return {};

			D3D11_TEXTURE2D_DESC nativeDesc = {};
			nativeDesc.Width = desc.width;
			nativeDesc.Height = desc.height;
			nativeDesc.MipLevels = desc.mipLevels;
			nativeDesc.ArraySize = desc.arraySize;
			nativeDesc.Format = format;
			nativeDesc.SampleDesc.Count = desc.sampleCount;
			nativeDesc.Usage = D3D11_USAGE_DEFAULT;
			nativeDesc.BindFlags = 0;
			if (Rhi::SdHasFlag(desc.usage, Rhi::SdTextureUsage::ShaderRead))
				nativeDesc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
			if (Rhi::SdHasFlag(desc.usage, Rhi::SdTextureUsage::RenderTarget))
				nativeDesc.BindFlags |= D3D11_BIND_RENDER_TARGET;
			if (Rhi::SdHasFlag(desc.usage, Rhi::SdTextureUsage::DepthStencil))
				nativeDesc.BindFlags |= D3D11_BIND_DEPTH_STENCIL;

			Microsoft::WRL::ComPtr<ID3D11Texture2D> texture = {};
			if (FAILED(nativeDevice->CreateTexture2D(&nativeDesc, nullptr, texture.GetAddressOf())))
				return {};

			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv = {};
			if ((nativeDesc.BindFlags & D3D11_BIND_SHADER_RESOURCE) != 0
				&& FAILED(nativeDevice->CreateShaderResourceView(texture.Get(), nullptr, srv.GetAddressOf())))
			{
				return {};
			}

			Microsoft::WRL::ComPtr<ID3D11RenderTargetView> rtv = {};
			if ((nativeDesc.BindFlags & D3D11_BIND_RENDER_TARGET) != 0
				&& FAILED(nativeDevice->CreateRenderTargetView(texture.Get(), nullptr, rtv.GetAddressOf())))
			{
				return {};
			}

			Microsoft::WRL::ComPtr<ID3D11DepthStencilView> dsv = {};
			if ((nativeDesc.BindFlags & D3D11_BIND_DEPTH_STENCIL) != 0
				&& FAILED(nativeDevice->CreateDepthStencilView(texture.Get(), nullptr, dsv.GetAddressOf())))
			{
				return {};
			}

			Rhi::SdTextureHandle handle = Allocate(textures);
			TextureEntry& entry = textures[handle.index];
			entry.resource = texture;
			entry.srv = srv;
			entry.rtv = rtv;
			entry.dsv = dsv;
			entry.desc = desc;
			return handle;
		}

		void DestroyTexture(Rhi::SdTextureHandle texture) override { Release(textures, texture); }

		bool CopyCurrentRenderTargetToTexture(Rhi::SdTextureHandle destination, const Rhi::SdRectI& sourceRect)
		{
			if (!nativeContext || sourceRect.Width() == 0 || sourceRect.Height() == 0)
				return false;

			TextureEntry* destinationEntry = Find(textures, destination);
			if (!destinationEntry || !destinationEntry->resource)
				return false;

			Microsoft::WRL::ComPtr<ID3D11RenderTargetView> currentRtv = {};
			nativeContext->OMGetRenderTargets(1, currentRtv.GetAddressOf(), nullptr);
			if (!currentRtv)
				return false;

			Microsoft::WRL::ComPtr<ID3D11Resource> sourceResource = {};
			currentRtv->GetResource(sourceResource.GetAddressOf());
			if (!sourceResource)
				return false;

			D3D11_BOX box = {};
			box.left = static_cast<UINT>(std::max(0, sourceRect.left));
			box.top = static_cast<UINT>(std::max(0, sourceRect.top));
			box.right = static_cast<UINT>(std::max(sourceRect.left, sourceRect.right));
			box.bottom = static_cast<UINT>(std::max(sourceRect.top, sourceRect.bottom));
			box.front = 0;
			box.back = 1;
			nativeContext->CopySubresourceRegion(destinationEntry->resource.Get(), 0, 0, 0, 0, sourceResource.Get(), 0, &box);
			return true;
		}

		bool UpdateTexture(Rhi::SdTextureHandle texture, const void* pixels, SdUInt32 rowPitch) override
		{
			if (!nativeContext || !pixels || rowPitch == 0)
				return false;

			TextureEntry* entry = Find(textures, texture);
			if (!entry || !entry->resource)
				return false;

			nativeContext->UpdateSubresource(entry->resource.Get(), 0, nullptr, pixels, rowPitch, 0);
			return true;
		}

		Rhi::SdBufferHandle CreateBuffer(const Rhi::SdBufferDesc& desc, const void* initialData) override
		{
			if (!nativeDevice || desc.size == 0 || desc.size > std::numeric_limits<UINT>::max())
				return {};

			D3D11_BUFFER_DESC nativeDesc = {};
			nativeDesc.ByteWidth = static_cast<UINT>(desc.size);
			nativeDesc.BindFlags = 0;
			if (Rhi::SdHasFlag(desc.usage, Rhi::SdBufferUsage::Vertex))
				nativeDesc.BindFlags |= D3D11_BIND_VERTEX_BUFFER;
			if (Rhi::SdHasFlag(desc.usage, Rhi::SdBufferUsage::Index))
				nativeDesc.BindFlags |= D3D11_BIND_INDEX_BUFFER;
			if (Rhi::SdHasFlag(desc.usage, Rhi::SdBufferUsage::Uniform))
				nativeDesc.BindFlags |= D3D11_BIND_CONSTANT_BUFFER;
			if (Rhi::SdHasFlag(desc.usage, Rhi::SdBufferUsage::Storage))
				nativeDesc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
			nativeDesc.Usage = desc.memoryUsage == Rhi::SdMemoryUsage::CpuToGpu ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
			nativeDesc.CPUAccessFlags = desc.memoryUsage == Rhi::SdMemoryUsage::CpuToGpu ? D3D11_CPU_ACCESS_WRITE : 0;

			D3D11_SUBRESOURCE_DATA data = {};
			data.pSysMem = initialData;
			Microsoft::WRL::ComPtr<ID3D11Buffer> buffer = {};
			if (FAILED(nativeDevice->CreateBuffer(&nativeDesc, initialData ? &data : nullptr, buffer.GetAddressOf())))
				return {};

			Rhi::SdBufferHandle handle = Allocate(buffers);
			BufferEntry& entry = buffers[handle.index];
			entry.buffer = buffer;
			entry.desc = desc;
			return handle;
		}

		void DestroyBuffer(Rhi::SdBufferHandle buffer) override { Release(buffers, buffer); }

		bool UpdateBuffer(Rhi::SdBufferHandle buffer, const void* data, SdUInt64 size, SdUInt64 offset) override
		{
			if (!nativeContext || !data || size == 0)
				return false;

			BufferEntry* entry = Find(buffers, buffer);
			if (!entry || !entry->buffer || offset > entry->desc.size || size > entry->desc.size - offset)
				return false;

			if (entry->desc.memoryUsage == Rhi::SdMemoryUsage::CpuToGpu)
			{
				D3D11_MAPPED_SUBRESOURCE mapped = {};
				if (FAILED(nativeContext->Map(entry->buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
					return false;
				std::memcpy(static_cast<std::byte*>(mapped.pData) + offset, data, static_cast<SdSize>(size));
				nativeContext->Unmap(entry->buffer.Get(), 0);
				return true;
			}

			D3D11_BOX box = {};
			box.left = static_cast<UINT>(offset);
			box.right = static_cast<UINT>(offset + size);
			box.bottom = 1;
			box.back = 1;
			nativeContext->UpdateSubresource(entry->buffer.Get(), 0, &box, data, 0, 0);
			return true;
		}

		Rhi::SdShaderHandle CreateShader(const Rhi::SdShaderDesc& desc) override
		{
			if (!nativeDevice || desc.bytecode.empty())
				return {};

			Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader = {};
			Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader = {};
			const void* bytecode = desc.bytecode.data();
			const SIZE_T bytecodeSize = desc.bytecode.size_bytes();

			if (desc.stage == Rhi::SdShaderStage::Vertex)
			{
				if (FAILED(nativeDevice->CreateVertexShader(bytecode, bytecodeSize, nullptr, vertexShader.GetAddressOf())))
					return {};
			}
			else if (desc.stage == Rhi::SdShaderStage::Pixel)
			{
				if (FAILED(nativeDevice->CreatePixelShader(bytecode, bytecodeSize, nullptr, pixelShader.GetAddressOf())))
					return {};
			}
			else
			{
				return {};
			}

			Rhi::SdShaderHandle handle = Allocate(shaders);
			ShaderEntry& entry = shaders[handle.index];
			entry.vertexShader = vertexShader;
			entry.pixelShader = pixelShader;
			entry.stage = desc.stage;
			entry.bytecode.assign(desc.bytecode.begin(), desc.bytecode.end());
			return handle;
		}

		void DestroyShader(Rhi::SdShaderHandle shader) override { Release(shaders, shader); }

		Rhi::SdSamplerHandle CreateSampler(const Rhi::SdSamplerDesc& desc) override
		{
			if (!nativeDevice)
				return {};

			D3D11_SAMPLER_DESC nativeDesc = {};
			nativeDesc.Filter = MapFilter(desc);
			nativeDesc.AddressU = MapAddress(desc.addressU);
			nativeDesc.AddressV = MapAddress(desc.addressV);
			nativeDesc.AddressW = MapAddress(desc.addressW);
			nativeDesc.MaxLOD = D3D11_FLOAT32_MAX;

			Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler = {};
			if (FAILED(nativeDevice->CreateSamplerState(&nativeDesc, sampler.GetAddressOf())))
				return {};

			Rhi::SdSamplerHandle handle = Allocate(samplers);
			samplers[handle.index].sampler = sampler;
			return handle;
		}

		void DestroySampler(Rhi::SdSamplerHandle sampler) override { Release(samplers, sampler); }

		Rhi::SdVertexLayoutHandle CreateVertexLayout(const Rhi::SdVertexLayoutDesc& desc) override
		{
			Rhi::SdVertexLayoutHandle handle = Allocate(vertexLayouts);
			VertexLayoutEntry& entry = vertexLayouts[handle.index];
			entry.attributes.assign(desc.attributes.begin(), desc.attributes.end());
			entry.semanticNames.clear();
			entry.semanticNames.reserve(desc.attributes.size());
			for (Rhi::SdVertexAttributeDesc& attribute : entry.attributes)
			{
				entry.semanticNames.emplace_back(attribute.semanticName);
				attribute.semanticName = entry.semanticNames.back();
			}
			return handle;
		}

		void DestroyVertexLayout(Rhi::SdVertexLayoutHandle layout) override { Release(vertexLayouts, layout); }

		Rhi::SdResourceSetLayoutHandle CreateResourceSetLayout(const Rhi::SdResourceSetLayoutDesc& desc) override
		{
			Rhi::SdResourceSetLayoutHandle handle = Allocate(resourceSetLayouts);
			ResourceSetLayoutEntry& entry = resourceSetLayouts[handle.index];
			entry.bindings.assign(desc.bindings.begin(), desc.bindings.end());
			return handle;
		}

		void DestroyResourceSetLayout(Rhi::SdResourceSetLayoutHandle layout) override { Release(resourceSetLayouts, layout); }

		Rhi::SdResourceSetHandle CreateResourceSet(const Rhi::SdResourceSetDesc& desc) override
		{
			if (!Find(resourceSetLayouts, desc.layout))
				return {};

			Rhi::SdResourceSetHandle handle = Allocate(resourceSets);
			ResourceSetEntry& entry = resourceSets[handle.index];
			entry.layout = desc.layout;
			entry.textures.assign(desc.textures.begin(), desc.textures.end());
			entry.samplers.assign(desc.samplers.begin(), desc.samplers.end());
			entry.buffers.assign(desc.buffers.begin(), desc.buffers.end());
			return handle;
		}

		void DestroyResourceSet(Rhi::SdResourceSetHandle resourceSet) override { Release(resourceSets, resourceSet); }

		Rhi::SdPipelineHandle CreateGraphicsPipeline(const Rhi::SdGraphicsPipelineDesc& desc) override
		{
			if (!nativeDevice)
				return {};

			const ShaderEntry* vs = Find(shaders, desc.vertexShader);
			const ShaderEntry* ps = Find(shaders, desc.pixelShader);
			if (!vs || !vs->vertexShader || !ps || !ps->pixelShader)
				return {};

			Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout = {};
			if (const VertexLayoutEntry* layout = Find(vertexLayouts, desc.vertexLayout))
			{
				std::vector<D3D11_INPUT_ELEMENT_DESC> elements;
				elements.reserve(layout->attributes.size());
				for (const Rhi::SdVertexAttributeDesc& attribute : layout->attributes)
				{
					elements.push_back({
						attribute.semanticName.data(),
						attribute.semanticIndex,
						MapVertexFormat(attribute.format),
						attribute.bufferSlot,
						attribute.offset,
						D3D11_INPUT_PER_VERTEX_DATA,
						0
					});
				}
				if (!elements.empty()
					&& FAILED(nativeDevice->CreateInputLayout(
						elements.data(),
						static_cast<UINT>(elements.size()),
						vs->bytecode.data(),
						vs->bytecode.size(),
						inputLayout.GetAddressOf())))
				{
					return {};
				}
			}

			D3D11_BLEND_DESC blendDesc = {};
			blendDesc.AlphaToCoverageEnable = desc.blendState.alphaToCoverage;
			const Rhi::SdBlendAttachmentDesc& colorBlend = desc.blendState.color;
			blendDesc.RenderTarget[0].BlendEnable = colorBlend.blendEnabled;
			blendDesc.RenderTarget[0].SrcBlend = MapBlendFactor(colorBlend.srcColor);
			blendDesc.RenderTarget[0].DestBlend = MapBlendFactor(colorBlend.dstColor);
			blendDesc.RenderTarget[0].BlendOp = MapBlendOp(colorBlend.colorOp);
			blendDesc.RenderTarget[0].SrcBlendAlpha = MapBlendFactor(colorBlend.srcAlpha);
			blendDesc.RenderTarget[0].DestBlendAlpha = MapBlendFactor(colorBlend.dstAlpha);
			blendDesc.RenderTarget[0].BlendOpAlpha = MapBlendOp(colorBlend.alphaOp);
			blendDesc.RenderTarget[0].RenderTargetWriteMask = static_cast<UINT8>(colorBlend.writeMask);

			Microsoft::WRL::ComPtr<ID3D11BlendState> blendState = {};
			if (FAILED(nativeDevice->CreateBlendState(&blendDesc, blendState.GetAddressOf())))
				return {};

			D3D11_RASTERIZER_DESC rasterDesc = {};
			rasterDesc.FillMode = desc.rasterState.fillMode == Rhi::SdFillMode::Wireframe ? D3D11_FILL_WIREFRAME : D3D11_FILL_SOLID;
			rasterDesc.CullMode = desc.rasterState.cullMode == Rhi::SdCullMode::Front ? D3D11_CULL_FRONT : (desc.rasterState.cullMode == Rhi::SdCullMode::Back ? D3D11_CULL_BACK : D3D11_CULL_NONE);
			rasterDesc.ScissorEnable = desc.rasterState.scissorEnabled;
			rasterDesc.DepthClipEnable = desc.rasterState.depthClipEnabled;

			Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizerState = {};
			if (FAILED(nativeDevice->CreateRasterizerState(&rasterDesc, rasterizerState.GetAddressOf())))
				return {};

			D3D11_DEPTH_STENCIL_DESC depthDesc = {};
			depthDesc.DepthEnable = desc.depthStencilState.depthTestEnabled;
			depthDesc.DepthWriteMask = desc.depthStencilState.depthWriteEnabled ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
			depthDesc.DepthFunc = MapCompare(desc.depthStencilState.depthCompare);

			Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthStencilState = {};
			if (FAILED(nativeDevice->CreateDepthStencilState(&depthDesc, depthStencilState.GetAddressOf())))
				return {};

			Rhi::SdPipelineHandle handle = Allocate(pipelines);
			PipelineEntry& entry = pipelines[handle.index];
			entry.inputLayout = inputLayout;
			entry.blendState = blendState;
			entry.rasterizerState = rasterizerState;
			entry.depthStencilState = depthStencilState;
			entry.vertexShader = desc.vertexShader;
			entry.pixelShader = desc.pixelShader;
			entry.resourceSetLayout = desc.resourceSetLayout;
			entry.topology = desc.topology;
			return handle;
		}

		void DestroyPipeline(Rhi::SdPipelineHandle pipeline) override { Release(pipelines, pipeline); }

	private:
		friend class SdDx11CommandEncoder;

		TextureEntry* FindTexture(Rhi::SdTextureHandle handle) noexcept { return Find(textures, handle); }
		BufferEntry* FindBuffer(Rhi::SdBufferHandle handle) noexcept { return Find(buffers, handle); }
		ShaderEntry* FindShader(Rhi::SdShaderHandle handle) noexcept { return Find(shaders, handle); }
		SamplerEntry* FindSampler(Rhi::SdSamplerHandle handle) noexcept { return Find(samplers, handle); }
		ResourceSetLayoutEntry* FindResourceSetLayout(Rhi::SdResourceSetLayoutHandle handle) noexcept { return Find(resourceSetLayouts, handle); }
		ResourceSetEntry* FindResourceSet(Rhi::SdResourceSetHandle handle) noexcept { return Find(resourceSets, handle); }
		PipelineEntry* FindPipeline(Rhi::SdPipelineHandle handle) noexcept { return Find(pipelines, handle); }
	};

	inline void SdDx11CommandEncoder::BeginRenderPass(const Rhi::SdRenderPassDesc& desc)
	{
		if (!device || !context)
			return;

		std::array<ID3D11ShaderResourceView*, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT> nullSrvs = {};
		context->PSSetShaderResources(0, static_cast<UINT>(nullSrvs.size()), nullSrvs.data());

		std::array<ID3D11RenderTargetView*, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT> rtvs = {};
		UINT rtvCount = 0;
		for (const Rhi::SdRenderPassColorAttachment& attachment : desc.colorAttachments)
		{
			if (rtvCount >= rtvs.size())
				break;
			SdDx11GpuDevice::TextureEntry* texture = device->FindTexture(attachment.texture);
			if (!texture || !texture->rtv)
				continue;

			rtvs[rtvCount++] = texture->rtv.Get();
			if (attachment.loadOp == Rhi::SdLoadOp::Clear)
			{
				const FLOAT clearColor[4] =
				{
					attachment.clearColor.r,
					attachment.clearColor.g,
					attachment.clearColor.b,
					attachment.clearColor.a
				};
				context->ClearRenderTargetView(texture->rtv.Get(), clearColor);
			}
		}

		ID3D11DepthStencilView* dsv = nullptr;
		if (desc.depthStencil.IsValid())
		{
			if (SdDx11GpuDevice::TextureEntry* depth = device->FindTexture(desc.depthStencil))
				dsv = depth->dsv.Get();
		}

		context->OMSetRenderTargets(rtvCount, rtvCount > 0 ? rtvs.data() : nullptr, dsv);
		if (desc.renderArea.Width() > 0 && desc.renderArea.Height() > 0)
		{
			SetViewport({
				static_cast<float>(desc.renderArea.left),
				static_cast<float>(desc.renderArea.top),
				static_cast<float>(desc.renderArea.Width()),
				static_cast<float>(desc.renderArea.Height()),
				0.0f,
				1.0f
			});
			SetScissorRect(desc.renderArea);
		}
	}

	inline void SdDx11CommandEncoder::EndRenderPass()
	{
	}

	inline void SdDx11CommandEncoder::SetPipeline(Rhi::SdPipelineHandle pipeline)
	{
		if (!device || !context)
			return;

		SdDx11GpuDevice::PipelineEntry* entry = device->FindPipeline(pipeline);
		if (!entry)
			return;

		SdDx11GpuDevice::ShaderEntry* vs = device->FindShader(entry->vertexShader);
		SdDx11GpuDevice::ShaderEntry* ps = device->FindShader(entry->pixelShader);
		context->IASetInputLayout(entry->inputLayout.Get());
		context->IASetPrimitiveTopology(SdDx11GpuDevice::MapTopology(entry->topology));
		context->VSSetShader(vs ? vs->vertexShader.Get() : nullptr, nullptr, 0);
		context->PSSetShader(ps ? ps->pixelShader.Get() : nullptr, nullptr, 0);
		context->OMSetBlendState(entry->blendState.Get(), nullptr, 0xFFFFFFFFu);
		context->OMSetDepthStencilState(entry->depthStencilState.Get(), 0);
		context->RSSetState(entry->rasterizerState.Get());
	}

	inline void SdDx11CommandEncoder::SetResourceSet(SdUInt32 setIndex, Rhi::SdResourceSetHandle resourceSet)
	{
		if (!device || !context)
			return;

		SdDx11GpuDevice::ResourceSetEntry* set = device->FindResourceSet(resourceSet);
		if (!set)
			return;
		SdDx11GpuDevice::ResourceSetLayoutEntry* layout = device->FindResourceSetLayout(set->layout);
		if (!layout)
			return;

		for (const Rhi::SdShaderBindingDesc& binding : layout->bindings)
		{
			if (binding.set != setIndex)
				continue;

			if (binding.type == Rhi::SdShaderResourceType::Texture2D)
			{
				ID3D11ShaderResourceView* srv = nullptr;
				for (const Rhi::SdBoundTexture& bound : set->textures)
				{
					if (bound.binding == binding.binding)
					{
						if (SdDx11GpuDevice::TextureEntry* texture = device->FindTexture(bound.texture))
							srv = texture->srv.Get();
						break;
					}
				}
				if (Rhi::SdHasFlag(binding.stages, Rhi::SdShaderStageFlag::Pixel))
					context->PSSetShaderResources(binding.binding, 1, &srv);
				if (Rhi::SdHasFlag(binding.stages, Rhi::SdShaderStageFlag::Vertex))
					context->VSSetShaderResources(binding.binding, 1, &srv);
			}
			else if (binding.type == Rhi::SdShaderResourceType::Sampler)
			{
				ID3D11SamplerState* sampler = nullptr;
				for (const Rhi::SdBoundSampler& bound : set->samplers)
				{
					if (bound.binding == binding.binding)
					{
						if (SdDx11GpuDevice::SamplerEntry* entry = device->FindSampler(bound.sampler))
							sampler = entry->sampler.Get();
						break;
					}
				}
				if (Rhi::SdHasFlag(binding.stages, Rhi::SdShaderStageFlag::Pixel))
					context->PSSetSamplers(binding.binding, 1, &sampler);
				if (Rhi::SdHasFlag(binding.stages, Rhi::SdShaderStageFlag::Vertex))
					context->VSSetSamplers(binding.binding, 1, &sampler);
			}
			else if (binding.type == Rhi::SdShaderResourceType::UniformBuffer)
			{
				ID3D11Buffer* buffer = nullptr;
				for (const Rhi::SdBoundBuffer& bound : set->buffers)
				{
					if (bound.binding == binding.binding)
					{
						if (SdDx11GpuDevice::BufferEntry* entry = device->FindBuffer(bound.buffer))
							buffer = entry->buffer.Get();
						break;
					}
				}
				if (Rhi::SdHasFlag(binding.stages, Rhi::SdShaderStageFlag::Pixel))
					context->PSSetConstantBuffers(binding.binding, 1, &buffer);
				if (Rhi::SdHasFlag(binding.stages, Rhi::SdShaderStageFlag::Vertex))
					context->VSSetConstantBuffers(binding.binding, 1, &buffer);
			}
		}
	}

	inline void SdDx11CommandEncoder::SetVertexBuffer(SdUInt32 slot, Rhi::SdBufferHandle buffer, SdUInt32 stride, SdUInt64 offset)
	{
		if (!device || !context || offset > std::numeric_limits<UINT>::max())
			return;
		SdDx11GpuDevice::BufferEntry* entry = device->FindBuffer(buffer);
		ID3D11Buffer* nativeBuffer = entry ? entry->buffer.Get() : nullptr;
		const UINT nativeStride = stride;
		const UINT nativeOffset = static_cast<UINT>(offset);
		context->IASetVertexBuffers(slot, 1, &nativeBuffer, &nativeStride, &nativeOffset);
	}

	inline void SdDx11CommandEncoder::SetIndexBuffer(Rhi::SdBufferHandle buffer, Rhi::SdIndexFormat format, SdUInt64 offset)
	{
		if (!device || !context || offset > std::numeric_limits<UINT>::max())
			return;
		SdDx11GpuDevice::BufferEntry* entry = device->FindBuffer(buffer);
		context->IASetIndexBuffer(
			entry ? entry->buffer.Get() : nullptr,
			format == Rhi::SdIndexFormat::UInt16 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT,
			static_cast<UINT>(offset));
	}

	inline void SdDx11CommandEncoder::SetViewport(const Rhi::SdViewport& viewport)
	{
		if (!context)
			return;
		D3D11_VIEWPORT nativeViewport = {};
		nativeViewport.TopLeftX = viewport.x;
		nativeViewport.TopLeftY = viewport.y;
		nativeViewport.Width = viewport.width;
		nativeViewport.Height = viewport.height;
		nativeViewport.MinDepth = viewport.minDepth;
		nativeViewport.MaxDepth = viewport.maxDepth;
		context->RSSetViewports(1, &nativeViewport);
	}

	inline void SdDx11CommandEncoder::SetScissorRect(const Rhi::SdRectI& rect)
	{
		if (!context)
			return;
		D3D11_RECT nativeRect = {};
		nativeRect.left = rect.left;
		nativeRect.top = rect.top;
		nativeRect.right = rect.right;
		nativeRect.bottom = rect.bottom;
		context->RSSetScissorRects(1, &nativeRect);
	}

	inline void SdDx11CommandEncoder::Draw(SdUInt32 vertexCount, SdUInt32 instanceCount, SdUInt32 firstVertex, SdUInt32 firstInstance)
	{
		if (!context)
			return;
		if (instanceCount <= 1 && firstInstance == 0)
			context->Draw(vertexCount, firstVertex);
		else
			context->DrawInstanced(vertexCount, instanceCount, firstVertex, firstInstance);
	}

	inline void SdDx11CommandEncoder::DrawIndexed(SdUInt32 indexCount, SdUInt32 instanceCount, SdUInt32 firstIndex, SdInt32 vertexOffset, SdUInt32 firstInstance)
	{
		if (!context)
			return;
		if (instanceCount <= 1 && firstInstance == 0)
			context->DrawIndexed(indexCount, firstIndex, vertexOffset);
		else
			context->DrawIndexedInstanced(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
	}
}
