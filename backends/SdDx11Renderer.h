#pragma once

#include "../SdRenderCore.h"

#include <Windows.h>
#include <d3d11.h>
#include <wrl/client.h>

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
			Microsoft::WRL::ComPtr<ID3D11Texture2D> resource = {};
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view = {};
			SdUInt32 width = 0;
			SdUInt32 height = 0;
			SdUInt32 generation = 0;
			bool occupied = false;
		};

		Microsoft::WRL::ComPtr<ID3D11Device> device = {};
		Microsoft::WRL::ComPtr<ID3D11DeviceContext> deviceContext = {};
		Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer = {};
		Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer = {};
		Microsoft::WRL::ComPtr<ID3D11Buffer> frameConstantBuffer = {};
		Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader = {};
		Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader = {};
		Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout = {};
		Microsoft::WRL::ComPtr<ID3D11BlendState> blendState = {};
		Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizerState = {};
		Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthStencilState = {};
		Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerState = {};
		std::vector<TextureEntry> textures = {};
		SdUInt32 vertexCapacity = 0;
		SdUInt32 indexCapacity = 0;
		SdRendererFrameInfo currentFrameInfo = {};

		bool CreatePipelineState();
		bool EnsureBuffers(SdUInt32 vertexCount, SdUInt32 indexCount);
		bool UploadBuffers(const SdDrawData& data);
		bool UploadTexture(const SdUploadRequest& request);
		TextureEntry& EnsureTextureEntry(SdTextureHandle texture);
		void BindPipeline(SdVec2 displaySize);
		TextureEntry* TryGetTexture(SdTextureHandle texture) noexcept;

	public:
		using Config = SdDx11RendererConfig;

		bool Initialize(const Config& config);
		void Shutdown();
		bool IsInitialized() const noexcept override { return device.Get() && deviceContext.Get() && vertexShader.Get() && pixelShader.Get() && inputLayout.Get(); }
		SdTextureHandle CreateTexture(SdUInt32 width, SdUInt32 height, const void* rgbaPixels);
		SdTextureHandle CreateTexture(const SdTextureDesc& desc) override;
		void DestroyTexture(SdTextureHandle texture) override;

		// Legacy compatibility helpers. New callers should use Render(frameInfo, packet)
		// or SdInstance::Render so renderer submission stays a single call.
		void BeginFrame(const SdRendererFrameInfo& frameInfo);
		void Submit(const SdDrawPacket& packet);
		void EndFrame();

		void Render(const SdRendererFrameInfo& frameInfo, const SdDrawPacket& packet) override;
		void Render(const SdDrawData& data, SdVec2 displaySize);
	};

	using SdRenderer = SdDx11Renderer;
}
