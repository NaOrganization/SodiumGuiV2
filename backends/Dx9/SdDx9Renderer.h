#pragma once

#include "Render/SdRenderCore.h"

#include <Windows.h>
#include <d3d9.h>
#include <wrl/client.h>

#include <array>
#include <vector>

namespace Sodium::Backends
{
	struct SdDx9RendererConfig final
	{
		IDirect3DDevice9* device = nullptr;
		Rhi::SdSwapchainDesc swapchain = {};
		D3DPRESENT_PARAMETERS presentationParameters = {};
		bool usePresentationParameters = false;
	};

	class SdDx9CommandEncoder final : public Rhi::ISdCommandEncoder
	{
	public:
		void BeginRenderPass(const Rhi::SdRenderPassDesc&) override {}
		void EndRenderPass() override {}
		void SetPipeline(Rhi::SdPipelineHandle) override {}
		void SetResourceSet(SdUInt32, Rhi::SdResourceSetHandle) override {}
		void SetVertexBuffer(SdUInt32, Rhi::SdBufferHandle, SdUInt32, SdUInt64) override {}
		void SetIndexBuffer(Rhi::SdBufferHandle, Rhi::SdIndexFormat, SdUInt64) override {}
		void SetViewport(const Rhi::SdViewport&) override {}
		void SetScissorRect(const Rhi::SdRectI&) override {}
		void Draw(SdUInt32, SdUInt32, SdUInt32, SdUInt32) override {}
		void DrawIndexed(SdUInt32, SdUInt32, SdUInt32, SdInt32, SdUInt32) override {}
	};

	class SdDx9GpuDevice final : public Rhi::ISdGpuDevice
	{
	private:
		struct ResourceEntry final
		{
			SdUInt32 generation = 0;
			bool occupied = false;
		};

		Rhi::SdGpuCaps caps = {};
		Rhi::SdRhiStatus lastStatus = Rhi::SdRhiStatus::Ok;
		SdDx9CommandEncoder immediateEncoder = {};
		std::vector<ResourceEntry> textures = {};
		std::vector<ResourceEntry> buffers = {};
		std::vector<ResourceEntry> shaders = {};
		std::vector<ResourceEntry> samplers = {};
		std::vector<ResourceEntry> vertexLayouts = {};
		std::vector<ResourceEntry> resourceSetLayouts = {};
		std::vector<ResourceEntry> resourceSets = {};
		std::vector<ResourceEntry> pipelines = {};
		bool initialized = false;

		template<typename THandle>
		static THandle Allocate(std::vector<ResourceEntry>& entries)
		{
			SdUInt32 index = 1;
			for (; index < entries.size(); ++index)
			{
				if (!entries[index].occupied)
					break;
			}
			if (entries.size() <= index)
				entries.resize(index + 1);
			ResourceEntry& entry = entries[index];
			entry.generation = entry.generation == 0 ? 1 : entry.generation + 1;
			entry.occupied = true;
			return THandle(index, entry.generation);
		}

		template<typename THandle>
		static bool IsValid(const std::vector<ResourceEntry>& entries, THandle handle) noexcept
		{
			if (!handle.IsValid() || handle.index >= entries.size())
				return false;
			const ResourceEntry& entry = entries[handle.index];
			return entry.occupied && entry.generation == handle.generation;
		}

		template<typename THandle>
		static void Release(std::vector<ResourceEntry>& entries, THandle handle) noexcept
		{
			if (!IsValid(entries, handle))
				return;
			ResourceEntry& entry = entries[handle.index];
			entry.occupied = false;
			entry.generation = handle.generation + 1;
		}

	public:
		bool Initialize();
		void Shutdown();
		bool IsInitialized() const noexcept { return initialized; }
		Rhi::ISdCommandEncoder& GetImmediateEncoder() noexcept { return immediateEncoder; }

		const Rhi::SdGpuCaps& GetCaps() const noexcept override { return caps; }
		Rhi::SdRhiStatus GetLastStatus() const noexcept override { return lastStatus; }
		void WaitIdle() override {}
		Rhi::SdTextureHandle CreateTexture(const Rhi::SdTextureDesc& desc) override;
		void DestroyTexture(Rhi::SdTextureHandle texture) override;
		bool UpdateTexture(Rhi::SdTextureHandle texture, const void* pixels, SdUInt32 rowPitch) override;
		Rhi::SdBufferHandle CreateBuffer(const Rhi::SdBufferDesc& desc, const void* initialData) override;
		void DestroyBuffer(Rhi::SdBufferHandle buffer) override;
		bool UpdateBuffer(Rhi::SdBufferHandle buffer, const void* data, SdUInt64 size, SdUInt64 offset) override;
		Rhi::SdShaderHandle CreateShader(const Rhi::SdShaderDesc& desc) override;
		Rhi::SdShaderHandle CreateShaderFromSource(const Rhi::SdShaderSourceDesc& desc) override;
		void DestroyShader(Rhi::SdShaderHandle shader) override;
		Rhi::SdSamplerHandle CreateSampler(const Rhi::SdSamplerDesc& desc) override;
		void DestroySampler(Rhi::SdSamplerHandle sampler) override;
		Rhi::SdVertexLayoutHandle CreateVertexLayout(const Rhi::SdVertexLayoutDesc& desc) override;
		void DestroyVertexLayout(Rhi::SdVertexLayoutHandle layout) override;
		Rhi::SdResourceSetLayoutHandle CreateResourceSetLayout(const Rhi::SdResourceSetLayoutDesc& desc) override;
		void DestroyResourceSetLayout(Rhi::SdResourceSetLayoutHandle layout) override;
		Rhi::SdResourceSetHandle CreateResourceSet(const Rhi::SdResourceSetDesc& desc) override;
		void DestroyResourceSet(Rhi::SdResourceSetHandle resourceSet) override;
		Rhi::SdPipelineHandle CreateGraphicsPipeline(const Rhi::SdGraphicsPipelineDesc& desc) override;
		void DestroyPipeline(Rhi::SdPipelineHandle pipeline) override;
	};

	class SdDx9Renderer final : public ISdRendererBackend
	{
	private:
		struct TextureEntry final
		{
			Microsoft::WRL::ComPtr<IDirect3DTexture9> texture = {};
			SdUInt32 width = 0;
			SdUInt32 height = 0;
			SdUInt32 generation = 0;
			bool occupied = false;
		};

		struct Dx9Vertex final
		{
			float x = 0.0f;
			float y = 0.0f;
			float z = 0.0f;
			float rhw = 1.0f;
			D3DCOLOR color = 0xFFFFFFFFu;
			float u = 0.0f;
			float v = 0.0f;
		};

		Microsoft::WRL::ComPtr<IDirect3D9> direct3d = {};
		Microsoft::WRL::ComPtr<IDirect3DDevice9> device = {};
		Microsoft::WRL::ComPtr<IDirect3DVertexBuffer9> vertexBuffer = {};
		Microsoft::WRL::ComPtr<IDirect3DIndexBuffer9> indexBuffer = {};
		D3DPRESENT_PARAMETERS presentationParameters = {};
		SdDx9GpuDevice rhiDevice = {};
		std::vector<TextureEntry> textures = {};
		std::vector<Dx9Vertex> convertedVertices = {};
		SdUInt32 vertexCapacity = 0;
		SdUInt32 indexCapacity = 0;
		SdUInt32 swapchainWidth = 0;
		SdUInt32 swapchainHeight = 0;
		bool ownsDeviceResources = false;
		bool sceneActive = false;
		bool deviceLost = false;

		bool CreateOwnedDeviceAndSwapchain(const SdDx9RendererConfig& config);
		bool ResetOwnedDevice();
		void BindPipeline(SdVec2 displaySize);
		bool EnsureBuffers(SdUInt32 vertexCount, SdUInt32 indexCount);
		bool UploadBuffers(const SdRenderPacket& packet);
		bool UploadTexture(const SdUploadRequest& request);
		TextureEntry& EnsureTextureEntry(SdTextureHandle texture);
		TextureEntry* TryGetTexture(SdTextureHandle texture) noexcept;
		static D3DCOLOR ConvertColor(SdUInt32 color) noexcept;
		static Rhi::SdRectI MakeScissor(const SdRect& rect, SdVec2 displaySize) noexcept;

	public:
		using Config = SdDx9RendererConfig;

		bool Initialize(const Config& config);
		void Shutdown();
		bool Resize(SdUInt32 width, SdUInt32 height);
		bool BeginFrame(const std::array<float, 4>& clearColor);
		bool Present();
		bool IsOccluded() const noexcept;
		void OnLostDevice();
		bool OnResetDevice();
		IDirect3DDevice9* GetNativeDevice() const noexcept { return device.Get(); }
		bool IsInitialized() const noexcept override { return device.Get() && rhiDevice.IsInitialized(); }
		SdDx9GpuDevice& GetRhiDevice() noexcept { return rhiDevice; }
		const SdDx9GpuDevice& GetRhiDevice() const noexcept { return rhiDevice; }
		Rhi::ISdGpuDevice& GetRhiDeviceInterface() noexcept override { return rhiDevice; }
		const Rhi::ISdGpuDevice& GetRhiDeviceInterface() const noexcept override { return rhiDevice; }
		SdTextureHandle CreateTexture(SdUInt32 width, SdUInt32 height, const void* rgbaPixels);
		SdTextureHandle CreateTexture(const SdTextureDesc& desc) override;
		void DestroyTexture(SdTextureHandle texture) override;
		void Render(const SdRendererFrameInfo& frameInfo, const SdRenderPacket& packet) override;
	};

	using SdRenderer = SdDx9Renderer;
}
