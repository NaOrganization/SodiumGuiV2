#pragma once

#include "Render/SdRenderCore.h"

#include <Windows.h>
#include <d3d12.h>
#include <dxgiformat.h>

#include <array>
#include <memory>

namespace Sodium::Backends
{
	struct SdDx12RendererConfig final
	{
		ID3D12Device* device = nullptr;
		ID3D12CommandQueue* commandQueue = nullptr;
		ID3D12Resource* const* renderTargets = nullptr;
		UINT renderTargetCount = 0;
		DXGI_FORMAT renderTargetFormat = DXGI_FORMAT_UNKNOWN;
	};

	class SdDx12Renderer final : public ISdRendererBackend
	{
	private:
		struct Impl;
		std::unique_ptr<Impl> impl;

	public:
		using Config = SdDx12RendererConfig;

		SdDx12Renderer();
		~SdDx12Renderer() override;

		SdDx12Renderer(const SdDx12Renderer&) = delete;
		SdDx12Renderer& operator=(const SdDx12Renderer&) = delete;
		SdDx12Renderer(SdDx12Renderer&&) = delete;
		SdDx12Renderer& operator=(SdDx12Renderer&&) = delete;

		bool Initialize(const Config& config);
		void Shutdown();
		bool SetRenderTargets(ID3D12Resource* const* renderTargets, UINT renderTargetCount, DXGI_FORMAT renderTargetFormat = DXGI_FORMAT_UNKNOWN);
		void ReleaseRenderTargets();
		void BeginFrame(UINT renderTargetIndex);
		void BeginFrame(UINT renderTargetIndex, const std::array<float, 4>& clearColor);

		bool IsInitialized() const noexcept override;
		Rhi::ISdGpuDevice* GetRhiDeviceInterface() noexcept override;
		const Rhi::ISdGpuDevice* GetRhiDeviceInterface() const noexcept override;
		SdTextureHandle CreateTexture(const SdTextureDesc& desc) override;
		void DestroyTexture(SdTextureHandle texture) override;
		void Render(const SdRendererFrameInfo& frameInfo, const SdRenderPacket& packet) override;
	};
}
