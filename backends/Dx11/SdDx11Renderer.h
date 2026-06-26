#pragma once

#include "backends/Dx11/SdDx11Rhi.h"
#include "Render/SdRenderCore.h"

#include <Windows.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <memory>

namespace Sodium::Backends
{
	struct SdDx11RendererConfig final
	{
		ID3D11Device* device = nullptr;
		ID3D11DeviceContext* deviceContext = nullptr;
	};

	class SdDx11Renderer final : public ISdRendererBackend
	{
	private:
		struct TextureEntry final
		{
			Rhi::SdTextureHandle texture = {};
			Rhi::SdResourceSetHandle resourceSet = {};
			SdUInt32 width = 0;
			SdUInt32 height = 0;
			SdUInt32 generation = 0;
			bool occupied = false;
		};

		Microsoft::WRL::ComPtr<ID3D11Device> device = {};
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> deviceContext = {};
		SdDx11GpuDevice rhiDevice = {};
		Rhi::SdBufferHandle vertexBuffer = {};
		Rhi::SdBufferHandle indexBuffer = {};
		Rhi::SdBufferHandle frameConstantBuffer = {};
		Rhi::SdShaderHandle vertexShader = {};
		Rhi::SdShaderHandle pixelShader = {};
		Rhi::SdVertexLayoutHandle vertexLayout = {};
		Rhi::SdSamplerHandle sampler = {};
		Rhi::SdResourceSetLayoutHandle resourceSetLayout = {};
		Rhi::SdPipelineHandle pipeline = {};
		std::vector<TextureEntry> textures = {};
		SdUInt32 vertexCapacity = 0;
		SdUInt32 indexCapacity = 0;
		SdRendererFrameInfo currentFrameInfo = {};

		bool CreatePipelineState();
		bool EnsureBuffers(SdUInt32 vertexCount, SdUInt32 indexCount);
		bool UploadBuffers(const SdRenderPacket& packet);
		bool UploadTexture(const SdUploadRequest& request);
		bool RebuildTextureResourceSet(TextureEntry& entry);
		TextureEntry& EnsureTextureEntry(SdTextureHandle texture);
		void BindPipeline(SdVec2 displaySize);
		TextureEntry* TryGetTexture(SdTextureHandle texture) noexcept;

	public:
		using Config = SdDx11RendererConfig;

		bool Initialize(const Config& config);
		void Shutdown();
		bool IsInitialized() const noexcept override { return rhiDevice.IsInitialized() && pipeline.IsValid(); }
		SdDx11GpuDevice& GetRhiDevice() noexcept { return rhiDevice; }
		const SdDx11GpuDevice& GetRhiDevice() const noexcept { return rhiDevice; }
		Rhi::ISdGpuDevice* GetRhiDeviceInterface() noexcept override { return &rhiDevice; }
		const Rhi::ISdGpuDevice* GetRhiDeviceInterface() const noexcept override { return &rhiDevice; }
		SdTextureHandle CreateTexture(SdUInt32 width, SdUInt32 height, const void* rgbaPixels);
		SdTextureHandle CreateTexture(const SdTextureDesc& desc) override;
		void DestroyTexture(SdTextureHandle texture) override;

		void Render(const SdRendererFrameInfo& frameInfo, const SdRenderPacket& packet) override;
	};

	using SdRenderer = SdDx11Renderer;
}
