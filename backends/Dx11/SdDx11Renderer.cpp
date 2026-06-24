#include "SdDx11Renderer.h"

#if defined(SODIUM_DX11_USE_PRECOMPILED_SHADERS)
#include "SdDx11PrecompiledShaders.h"
#else
#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")
#endif

#include <algorithm>
#include <array>
#include <cstddef>
#include <cmath>
#include <cstring>

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

namespace Sodium::Backends
{
	namespace
	{
#if !defined(SODIUM_DX11_USE_PRECOMPILED_SHADERS)
		const char* const kVertexShader = SODIUM_STRING(R"(
			cbuffer FrameConstants : register(b0)
			{
				float4x4 ProjectionMatrix;
			};

			struct VSInput
			{
				float2 position : POSITION;
				float2 uv : TEXCOORD0;
				float4 color : COLOR0;
			};

			struct PSInput
			{
				float4 position : SV_POSITION;
				float2 uv : TEXCOORD0;
				float4 color : COLOR0;
			};

			PSInput main(VSInput input)
			{
				PSInput output;
				output.position = mul(ProjectionMatrix, float4(input.position, 0.0f, 1.0f));
				output.uv = input.uv;
				output.color = input.color;
				return output;
			})");

		const char* const kPixelShader = SODIUM_STRING(R"(
			Texture2D texture0 : register(t0);
			SamplerState sampler0 : register(s0);

			struct PSInput
			{
				float4 position : SV_POSITION;
				float2 uv : TEXCOORD0;
				float4 color : COLOR0;
			};

			float4 main(PSInput input) : SV_Target
			{
				return input.color * texture0.Sample(sampler0, input.uv);
			})");
#endif

		struct FrameConstants final
		{
			float projection[4][4] = {};
		};

		class Dx11PipelineStateSnapshot final
		{
		private:
			static constexpr UINT kShaderClassInstanceCapacity = D3D11_SHADER_MAX_INTERFACES;
			static constexpr UINT kViewportCapacity = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
			static constexpr UINT kScissorCapacity = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;

			ID3D11DeviceContext* context = nullptr;
			Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout = {};
			Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer = {};
			UINT vertexStride = 0;
			UINT vertexOffset = 0;
			Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer = {};
			DXGI_FORMAT indexFormat = DXGI_FORMAT_UNKNOWN;
			UINT indexOffset = 0;
			D3D11_PRIMITIVE_TOPOLOGY primitiveTopology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
			Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader = {};
			std::array<Microsoft::WRL::ComPtr<ID3D11ClassInstance>, kShaderClassInstanceCapacity> vertexShaderInstances = {};
			UINT vertexShaderInstanceCount = 0;
			Microsoft::WRL::ComPtr<ID3D11Buffer> vertexConstantBuffer = {};
			Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader = {};
			std::array<Microsoft::WRL::ComPtr<ID3D11ClassInstance>, kShaderClassInstanceCapacity> pixelShaderInstances = {};
			UINT pixelShaderInstanceCount = 0;
			Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> pixelShaderResource = {};
			Microsoft::WRL::ComPtr<ID3D11SamplerState> pixelSampler = {};
			Microsoft::WRL::ComPtr<ID3D11BlendState> blendState = {};
			FLOAT blendFactor[4] = {};
			UINT sampleMask = 0xFFFFFFFFu;
			Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthStencilState = {};
			UINT stencilRef = 0;
			Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizerState = {};
			std::array<D3D11_VIEWPORT, kViewportCapacity> viewports = {};
			UINT viewportCount = 0;
			std::array<D3D11_RECT, kScissorCapacity> scissors = {};
			UINT scissorCount = 0;

			template<SdSize Count>
			static void AttachClassInstances(
				std::array<Microsoft::WRL::ComPtr<ID3D11ClassInstance>, Count>& target,
				std::array<ID3D11ClassInstance*, Count>& source,
				UINT count)
			{
				for (UINT i = 0; i < count; ++i)
					target[i].Attach(source[i]);
			}

			template<SdSize Count>
			static void FillClassInstancePointers(
				const std::array<Microsoft::WRL::ComPtr<ID3D11ClassInstance>, Count>& source,
				std::array<ID3D11ClassInstance*, Count>& target,
				UINT count)
			{
				for (UINT i = 0; i < count; ++i)
					target[i] = source[i].Get();
			}

		public:
			explicit Dx11PipelineStateSnapshot(ID3D11DeviceContext* deviceContext)
				: context(deviceContext)
			{
				if (!context)
					return;

				context->IAGetInputLayout(inputLayout.GetAddressOf());
				context->IAGetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &vertexStride, &vertexOffset);
				context->IAGetIndexBuffer(indexBuffer.GetAddressOf(), &indexFormat, &indexOffset);
				context->IAGetPrimitiveTopology(&primitiveTopology);

				std::array<ID3D11ClassInstance*, kShaderClassInstanceCapacity> vertexInstances = {};
				vertexShaderInstanceCount = kShaderClassInstanceCapacity;
				context->VSGetShader(vertexShader.GetAddressOf(), vertexInstances.data(), &vertexShaderInstanceCount);
				AttachClassInstances(vertexShaderInstances, vertexInstances, vertexShaderInstanceCount);
				context->VSGetConstantBuffers(0, 1, vertexConstantBuffer.GetAddressOf());

				std::array<ID3D11ClassInstance*, kShaderClassInstanceCapacity> pixelInstances = {};
				pixelShaderInstanceCount = kShaderClassInstanceCapacity;
				context->PSGetShader(pixelShader.GetAddressOf(), pixelInstances.data(), &pixelShaderInstanceCount);
				AttachClassInstances(pixelShaderInstances, pixelInstances, pixelShaderInstanceCount);
				context->PSGetShaderResources(0, 1, pixelShaderResource.GetAddressOf());
				context->PSGetSamplers(0, 1, pixelSampler.GetAddressOf());

				context->OMGetBlendState(blendState.GetAddressOf(), blendFactor, &sampleMask);
				context->OMGetDepthStencilState(depthStencilState.GetAddressOf(), &stencilRef);

				context->RSGetState(rasterizerState.GetAddressOf());
				viewportCount = kViewportCapacity;
				context->RSGetViewports(&viewportCount, viewports.data());
				scissorCount = kScissorCapacity;
				context->RSGetScissorRects(&scissorCount, scissors.data());
			}

			~Dx11PipelineStateSnapshot()
			{
				Restore();
			}

			Dx11PipelineStateSnapshot(const Dx11PipelineStateSnapshot&) = delete;
			Dx11PipelineStateSnapshot& operator=(const Dx11PipelineStateSnapshot&) = delete;

			void Restore()
			{
				if (!context)
					return;

				ID3D11Buffer* restoredVertexBuffer = vertexBuffer.Get();
				context->IASetInputLayout(inputLayout.Get());
				context->IASetVertexBuffers(0, 1, &restoredVertexBuffer, &vertexStride, &vertexOffset);
				context->IASetIndexBuffer(indexBuffer.Get(), indexFormat, indexOffset);
				context->IASetPrimitiveTopology(primitiveTopology);

				std::array<ID3D11ClassInstance*, kShaderClassInstanceCapacity> vertexInstances = {};
				FillClassInstancePointers(vertexShaderInstances, vertexInstances, vertexShaderInstanceCount);
				context->VSSetShader(vertexShader.Get(), vertexShaderInstanceCount > 0 ? vertexInstances.data() : nullptr, vertexShaderInstanceCount);
				ID3D11Buffer* restoredVertexConstantBuffer = vertexConstantBuffer.Get();
				context->VSSetConstantBuffers(0, 1, &restoredVertexConstantBuffer);

				std::array<ID3D11ClassInstance*, kShaderClassInstanceCapacity> pixelInstances = {};
				FillClassInstancePointers(pixelShaderInstances, pixelInstances, pixelShaderInstanceCount);
				context->PSSetShader(pixelShader.Get(), pixelShaderInstanceCount > 0 ? pixelInstances.data() : nullptr, pixelShaderInstanceCount);
				ID3D11ShaderResourceView* restoredPixelShaderResource = pixelShaderResource.Get();
				context->PSSetShaderResources(0, 1, &restoredPixelShaderResource);
				ID3D11SamplerState* restoredPixelSampler = pixelSampler.Get();
				context->PSSetSamplers(0, 1, &restoredPixelSampler);

				context->OMSetBlendState(blendState.Get(), blendFactor, sampleMask);
				context->OMSetDepthStencilState(depthStencilState.Get(), stencilRef);

				context->RSSetState(rasterizerState.Get());
				context->RSSetViewports(viewportCount, viewportCount > 0 ? viewports.data() : nullptr);
				context->RSSetScissorRects(scissorCount, scissorCount > 0 ? scissors.data() : nullptr);
				context = nullptr;
			}
		};
	}

	bool SdDx11Renderer::Initialize(const Config& config)
	{
		if (!config.device || !config.deviceContext)
			return false;

		device = config.device;
		deviceContext = config.deviceContext;
		if (!CreatePipelineState())
			return false;
		return true;
	}

	void SdDx11Renderer::Shutdown()
	{
		textures.clear();
		samplerState.Reset();
		depthStencilState.Reset();
		rasterizerState.Reset();
		blendState.Reset();
		inputLayout.Reset();
		pixelShader.Reset();
		vertexShader.Reset();
		frameConstantBuffer.Reset();
		indexBuffer.Reset();
		vertexBuffer.Reset();
		deviceContext.Reset();
		device.Reset();
		vertexCapacity = 0;
		indexCapacity = 0;
	}

	bool SdDx11Renderer::CreatePipelineState()
	{
#if defined(SODIUM_DX11_USE_PRECOMPILED_SHADERS)
		const void* vertexShaderBytecode = PrecompiledShaders::kVertexShader;
		const std::size_t vertexShaderBytecodeSize = PrecompiledShaders::kVertexShaderSize;
		const void* pixelShaderBytecode = PrecompiledShaders::kPixelShader;
		const std::size_t pixelShaderBytecodeSize = PrecompiledShaders::kPixelShaderSize;
#else
		Microsoft::WRL::ComPtr<ID3DBlob> vertexBlob = {};
		Microsoft::WRL::ComPtr<ID3DBlob> pixelBlob = {};
		Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = {};
		if (FAILED(::D3DCompile(kVertexShader, std::strlen(kVertexShader), nullptr, nullptr, nullptr, SODIUM_STRING("main"), SODIUM_STRING("vs_4_0"), 0, 0, vertexBlob.GetAddressOf(), errorBlob.GetAddressOf())))
			return false;
		if (FAILED(::D3DCompile(kPixelShader, std::strlen(kPixelShader), nullptr, nullptr, nullptr, SODIUM_STRING("main"), SODIUM_STRING("ps_4_0"), 0, 0, pixelBlob.GetAddressOf(), errorBlob.ReleaseAndGetAddressOf())))
			return false;

		const void* vertexShaderBytecode = vertexBlob->GetBufferPointer();
		const std::size_t vertexShaderBytecodeSize = vertexBlob->GetBufferSize();
		const void* pixelShaderBytecode = pixelBlob->GetBufferPointer();
		const std::size_t pixelShaderBytecodeSize = pixelBlob->GetBufferSize();
#endif

		if (FAILED(device->CreateVertexShader(vertexShaderBytecode, vertexShaderBytecodeSize, nullptr, vertexShader.ReleaseAndGetAddressOf())))
			return false;
		if (FAILED(device->CreatePixelShader(pixelShaderBytecode, pixelShaderBytecodeSize, nullptr, pixelShader.ReleaseAndGetAddressOf())))
			return false;

		const D3D11_INPUT_ELEMENT_DESC layout[] =
		{
			{ SODIUM_STRING("POSITION"), 0, DXGI_FORMAT_R32G32_FLOAT, 0, static_cast<UINT>(offsetof(SdVertex, position)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ SODIUM_STRING("TEXCOORD"), 0, DXGI_FORMAT_R32G32_FLOAT, 0, static_cast<UINT>(offsetof(SdVertex, uv)), D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ SODIUM_STRING("COLOR"), 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, static_cast<UINT>(offsetof(SdVertex, color)), D3D11_INPUT_PER_VERTEX_DATA, 0 }
		};
		if (FAILED(device->CreateInputLayout(layout, ARRAYSIZE(layout), vertexShaderBytecode, vertexShaderBytecodeSize, inputLayout.ReleaseAndGetAddressOf())))
			return false;

		D3D11_BLEND_DESC blendDesc = {};
		blendDesc.RenderTarget[0].BlendEnable = TRUE;
		blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
		blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		if (FAILED(device->CreateBlendState(&blendDesc, blendState.ReleaseAndGetAddressOf())))
			return false;

		D3D11_RASTERIZER_DESC rasterizerDesc = {};
		rasterizerDesc.FillMode = D3D11_FILL_SOLID;
		rasterizerDesc.CullMode = D3D11_CULL_NONE;
		rasterizerDesc.ScissorEnable = TRUE;
		rasterizerDesc.DepthClipEnable = TRUE;
		if (FAILED(device->CreateRasterizerState(&rasterizerDesc, rasterizerState.ReleaseAndGetAddressOf())))
			return false;

		D3D11_DEPTH_STENCIL_DESC depthDesc = {};
		depthDesc.DepthEnable = FALSE;
		depthDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
		if (FAILED(device->CreateDepthStencilState(&depthDesc, depthStencilState.ReleaseAndGetAddressOf())))
			return false;

		D3D11_SAMPLER_DESC samplerDesc = {};
		samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		if (FAILED(device->CreateSamplerState(&samplerDesc, samplerState.ReleaseAndGetAddressOf())))
			return false;

		D3D11_BUFFER_DESC constantDesc = {};
		constantDesc.ByteWidth = sizeof(FrameConstants);
		constantDesc.Usage = D3D11_USAGE_DYNAMIC;
		constantDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		constantDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		return SUCCEEDED(device->CreateBuffer(&constantDesc, nullptr, frameConstantBuffer.ReleaseAndGetAddressOf()));
	}

	bool SdDx11Renderer::EnsureBuffers(SdUInt32 vertexCount, SdUInt32 indexCount)
	{
		if (vertexCount > vertexCapacity)
		{
			vertexCapacity = std::max(vertexCount, vertexCapacity == 0 ? 1024u : vertexCapacity * 2u);
			D3D11_BUFFER_DESC desc = {};
			desc.ByteWidth = vertexCapacity * sizeof(SdVertex);
			desc.Usage = D3D11_USAGE_DYNAMIC;
			desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			if (FAILED(device->CreateBuffer(&desc, nullptr, vertexBuffer.ReleaseAndGetAddressOf())))
				return false;
		}

		if (indexCount > indexCapacity)
		{
			indexCapacity = std::max(indexCount, indexCapacity == 0 ? 2048u : indexCapacity * 2u);
			D3D11_BUFFER_DESC desc = {};
			desc.ByteWidth = indexCapacity * sizeof(SdUInt32);
			desc.Usage = D3D11_USAGE_DYNAMIC;
			desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			if (FAILED(device->CreateBuffer(&desc, nullptr, indexBuffer.ReleaseAndGetAddressOf())))
				return false;
		}
		return true;
	}

	bool SdDx11Renderer::UploadBuffers(const SdDrawPacket& packet)
	{
		if (!EnsureBuffers(static_cast<SdUInt32>(packet.vertices.size()), static_cast<SdUInt32>(packet.indices.size())))
			return false;
		if (packet.vertices.empty() || packet.indices.empty())
			return true;

		D3D11_MAPPED_SUBRESOURCE mapped = {};
		if (FAILED(deviceContext->Map(vertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
			return false;
		std::memcpy(mapped.pData, packet.vertices.data(), packet.vertices.size() * sizeof(SdVertex));
		deviceContext->Unmap(vertexBuffer.Get(), 0);

		if (FAILED(deviceContext->Map(indexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
			return false;
		std::memcpy(mapped.pData, packet.indices.data(), packet.indices.size() * sizeof(SdUInt32));
		deviceContext->Unmap(indexBuffer.Get(), 0);
		return true;
	}

	SdDx11Renderer::TextureEntry& SdDx11Renderer::EnsureTextureEntry(SdTextureHandle texture)
	{
		if (textures.size() <= texture.index)
			textures.resize(texture.index + 1);
		TextureEntry& entry = textures[texture.index];
		if (entry.generation == 0)
			entry.generation = texture.generation;
		entry.occupied = true;
		return entry;
	}

	bool SdDx11Renderer::UploadTexture(const SdUploadRequest& request)
	{
		if (!request.texture.IsValid() || request.width == 0 || request.height == 0 || request.pixels.empty())
			return false;

		TextureEntry& entry = EnsureTextureEntry(request.texture);
		if (!entry.resource || entry.width != request.width || entry.height != request.height || entry.generation != request.texture.generation)
		{
			D3D11_TEXTURE2D_DESC desc = {};
			desc.Width = request.width;
			desc.Height = request.height;
			desc.MipLevels = 1;
			desc.ArraySize = 1;
			desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
			desc.SampleDesc.Count = 1;
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			if (FAILED(device->CreateTexture2D(&desc, nullptr, entry.resource.ReleaseAndGetAddressOf())))
				return false;
			if (FAILED(device->CreateShaderResourceView(entry.resource.Get(), nullptr, entry.view.ReleaseAndGetAddressOf())))
				return false;
			entry.width = request.width;
			entry.height = request.height;
			entry.generation = request.texture.generation;
			entry.occupied = true;
		}

		deviceContext->UpdateSubresource(entry.resource.Get(), 0, nullptr, request.pixels.data(), request.width * sizeof(SdUInt32), 0);
		return true;
	}

	void SdDx11Renderer::BindPipeline(SdVec2 displaySize)
	{
		const float left = 0.0f;
		const float right = displaySize.x;
		const float top = 0.0f;
		const float bottom = displaySize.y;

		FrameConstants constants = {};
		constants.projection[0][0] = 2.0f / (right - left);
		constants.projection[1][1] = 2.0f / (top - bottom);
		constants.projection[2][2] = 0.5f;
		constants.projection[3][0] = (right + left) / (left - right);
		constants.projection[3][1] = (top + bottom) / (bottom - top);
		constants.projection[3][2] = 0.5f;
		constants.projection[3][3] = 1.0f;

		D3D11_MAPPED_SUBRESOURCE mapped = {};
		if (SUCCEEDED(deviceContext->Map(frameConstantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		{
			std::memcpy(mapped.pData, &constants, sizeof(constants));
			deviceContext->Unmap(frameConstantBuffer.Get(), 0);
		}

		const UINT stride = sizeof(SdVertex);
		const UINT offset = 0;
		ID3D11Buffer* vertexBuffers[] = { vertexBuffer.Get() };
		deviceContext->IASetInputLayout(inputLayout.Get());
		deviceContext->IASetVertexBuffers(0, 1, vertexBuffers, &stride, &offset);
		deviceContext->IASetIndexBuffer(indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
		deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		deviceContext->VSSetShader(vertexShader.Get(), nullptr, 0);
		deviceContext->VSSetConstantBuffers(0, 1, frameConstantBuffer.GetAddressOf());
		deviceContext->PSSetShader(pixelShader.Get(), nullptr, 0);
		deviceContext->PSSetSamplers(0, 1, samplerState.GetAddressOf());
		deviceContext->OMSetBlendState(blendState.Get(), nullptr, 0xFFFFFFFFu);
		deviceContext->OMSetDepthStencilState(depthStencilState.Get(), 0);
		deviceContext->RSSetState(rasterizerState.Get());

		D3D11_VIEWPORT viewport = {};
		viewport.TopLeftX = 0.0f;
		viewport.TopLeftY = 0.0f;
		viewport.Width = displaySize.x;
		viewport.Height = displaySize.y;
		viewport.MinDepth = 0.0f;
		viewport.MaxDepth = 1.0f;
		deviceContext->RSSetViewports(1, &viewport);
	}

	SdTextureHandle SdDx11Renderer::CreateTexture(SdUInt32 width, SdUInt32 height, const void* rgbaPixels)
	{
		if (width == 0 || height == 0 || !rgbaPixels)
			return {};

		D3D11_TEXTURE2D_DESC desc = {};
		desc.Width = width;
		desc.Height = height;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

		D3D11_SUBRESOURCE_DATA initialData = {};
		initialData.pSysMem = rgbaPixels;
		initialData.SysMemPitch = width * sizeof(SdUInt32);

		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view = {};
		Microsoft::WRL::ComPtr<ID3D11Texture2D> texture = {};
		if (FAILED(device->CreateTexture2D(&desc, &initialData, texture.GetAddressOf())))
			return {};
		if (FAILED(device->CreateShaderResourceView(texture.Get(), nullptr, view.GetAddressOf())))
			return {};

		SdUInt32 index = 1;
		for (; index < textures.size(); ++index)
		{
			if (!textures[index].occupied)
				break;
		}
		if (textures.size() <= index)
			textures.resize(index + 1);

		TextureEntry& entry = textures[index];
		entry.resource = texture;
		entry.view = view;
		entry.width = width;
		entry.height = height;
		entry.generation = entry.generation == 0 ? 1 : entry.generation + 1;
		entry.occupied = true;
		return SdTextureHandle(index, entry.generation);
	}

	SdTextureHandle SdDx11Renderer::CreateTexture(const SdTextureDesc& desc)
	{
		if (desc.width == 0 || desc.height == 0)
			return {};
		return CreateTexture(desc.width, desc.height, desc.pixels.empty() ? nullptr : desc.pixels.data());
	}

	void SdDx11Renderer::DestroyTexture(SdTextureHandle texture)
	{
		TextureEntry* entry = TryGetTexture(texture);
		if (!entry)
			return;
		entry->resource.Reset();
		entry->view.Reset();
		entry->width = 0;
		entry->height = 0;
		entry->occupied = false;
		++entry->generation;
	}

	SdDx11Renderer::TextureEntry* SdDx11Renderer::TryGetTexture(SdTextureHandle texture) noexcept
	{
		if (!texture.IsValid() || texture.index >= textures.size())
			return nullptr;
		TextureEntry& entry = textures[texture.index];
		if (!entry.occupied || entry.generation != texture.generation)
			return nullptr;
		return &entry;
	}

	void SdDx11Renderer::Render(const SdRendererFrameInfo& frameInfo, const SdDrawPacket& packet)
	{
		if (!deviceContext)
			return;
		for (const SdUploadRequest& upload : packet.resourceUpdates)
			UploadTexture(upload);
		if (packet.vertices.empty() || packet.indices.empty())
			return;
		if (!UploadBuffers(packet))
			return;

		Dx11PipelineStateSnapshot pipelineState(deviceContext.Get());
		BindPipeline(frameInfo.displaySize);
		for (const SdDrawCommand& command : packet.commands)
		{
			if (command.kind != SdDrawCommandKind::OwnedBatch || command.index >= packet.batches.size())
				continue;
			const SdRenderBatch& batch = packet.batches[command.index];
			if (batch.indexCount == 0)
				continue;

			TextureEntry* texture = TryGetTexture(batch.texture);
			if (!texture)
				continue;

			ID3D11ShaderResourceView* view = texture->view.Get();
			deviceContext->PSSetShaderResources(0, 1, &view);

			D3D11_RECT scissor = {};
			scissor.left = static_cast<LONG>(std::floor(std::max(0.0f, batch.clipRect.min.x)));
			scissor.top = static_cast<LONG>(std::floor(std::max(0.0f, batch.clipRect.min.y)));
			scissor.right = static_cast<LONG>(std::ceil(std::min(frameInfo.displaySize.x, batch.clipRect.max.x)));
			scissor.bottom = static_cast<LONG>(std::ceil(std::min(frameInfo.displaySize.y, batch.clipRect.max.y)));
			if (scissor.left >= scissor.right || scissor.top >= scissor.bottom)
				continue;
			deviceContext->RSSetScissorRects(1, &scissor);
			deviceContext->DrawIndexed(batch.indexCount, batch.indexOffset, 0);
		}
	}
}
