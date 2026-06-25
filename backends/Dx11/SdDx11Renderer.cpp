#include "SdDx11Renderer.h"

#include "Effects/SdBlurEffect.hpp"

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
		Rhi::SdTextureFormat MapDxgiFormatToRhi(DXGI_FORMAT format) noexcept
		{
			switch (format)
			{
			case DXGI_FORMAT_R8G8B8A8_UNORM: return Rhi::SdTextureFormat::Rgba8Unorm;
			case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return Rhi::SdTextureFormat::Rgba8UnormSrgb;
			case DXGI_FORMAT_B8G8R8A8_UNORM: return Rhi::SdTextureFormat::Bgra8Unorm;
			case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: return Rhi::SdTextureFormat::Bgra8UnormSrgb;
			case DXGI_FORMAT_R16G16B16A16_FLOAT: return Rhi::SdTextureFormat::Rgba16Float;
			default: return Rhi::SdTextureFormat::Bgra8Unorm;
			}
		}

		SdRect IntersectRect(const SdRect& a, const SdRect& b) noexcept
		{
			return {
				std::max(a.min.x, b.min.x),
				std::max(a.min.y, b.min.y),
				std::min(a.max.x, b.max.x),
				std::min(a.max.y, b.max.y)
			};
		}

		SdRect ExpandRect(const SdRect& rect, float amount) noexcept
		{
			return {
				rect.min.x - amount,
				rect.min.y - amount,
				rect.max.x + amount,
				rect.max.y + amount
			};
		}

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
		if (!rhiDevice.Initialize({ config.device, config.deviceContext }))
			return false;
		if (!CreatePipelineState())
			return false;
		transientTexturePool = std::make_unique<Rhi::SdTransientTexturePool>(rhiDevice);
		if (!SdDx11InitializeBlurEffectResources(rhiDevice, effectResources))
			return false;
		return true;
	}

	void SdDx11Renderer::Shutdown()
	{
		transientTexturePool.reset();
		for (TextureEntry& texture : textures)
		{
			if (texture.resourceSet.IsValid())
				rhiDevice.DestroyResourceSet(texture.resourceSet);
			if (texture.texture.IsValid())
				rhiDevice.DestroyTexture(texture.texture);
		}
		textures.clear();
		if (pipeline.IsValid())
			rhiDevice.DestroyPipeline(pipeline);
		if (resourceSetLayout.IsValid())
			rhiDevice.DestroyResourceSetLayout(resourceSetLayout);
		if (sampler.IsValid())
			rhiDevice.DestroySampler(sampler);
		if (vertexLayout.IsValid())
			rhiDevice.DestroyVertexLayout(vertexLayout);
		if (pixelShader.IsValid())
			rhiDevice.DestroyShader(pixelShader);
		if (vertexShader.IsValid())
			rhiDevice.DestroyShader(vertexShader);
		if (frameConstantBuffer.IsValid())
			rhiDevice.DestroyBuffer(frameConstantBuffer);
		if (indexBuffer.IsValid())
			rhiDevice.DestroyBuffer(indexBuffer);
		if (vertexBuffer.IsValid())
			rhiDevice.DestroyBuffer(vertexBuffer);
		rhiDevice.Shutdown();
		pipeline = {};
		resourceSetLayout = {};
		sampler = {};
		vertexLayout = {};
		pixelShader = {};
		vertexShader = {};
		frameConstantBuffer = {};
		indexBuffer = {};
		vertexBuffer = {};
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

		const Rhi::SdShaderDesc vertexShaderDesc =
		{
			Rhi::SdShaderStage::Vertex,
			SdSpan<const Rhi::SdByte>(reinterpret_cast<const Rhi::SdByte*>(vertexShaderBytecode), vertexShaderBytecodeSize),
			"main",
			"Sodium.Gui.Default.VS"
		};
		const Rhi::SdShaderDesc pixelShaderDesc =
		{
			Rhi::SdShaderStage::Pixel,
			SdSpan<const Rhi::SdByte>(reinterpret_cast<const Rhi::SdByte*>(pixelShaderBytecode), pixelShaderBytecodeSize),
			"main",
			"Sodium.Gui.Default.PS"
		};

		vertexShader = rhiDevice.CreateShader(vertexShaderDesc);
		pixelShader = rhiDevice.CreateShader(pixelShaderDesc);
		if (!vertexShader.IsValid() || !pixelShader.IsValid())
			return false;

		const Rhi::SdVertexAttributeDesc attributes[] =
		{
			{ "POSITION", 0, Rhi::SdVertexFormat::Float2, static_cast<SdUInt32>(offsetof(SdVertex, position)), 0 },
			{ "TEXCOORD", 0, Rhi::SdVertexFormat::Float2, static_cast<SdUInt32>(offsetof(SdVertex, uv)), 0 },
			{ "COLOR", 0, Rhi::SdVertexFormat::UByte4Norm, static_cast<SdUInt32>(offsetof(SdVertex, color)), 0 }
		};
		vertexLayout = rhiDevice.CreateVertexLayout({ attributes, "Sodium.Gui.Default.VertexLayout" });
		if (!vertexLayout.IsValid())
			return false;

		const Rhi::SdShaderBindingDesc bindings[] =
		{
			{ "FrameConstants", Rhi::SdShaderResourceType::UniformBuffer, 0, 0, static_cast<Rhi::SdShaderStageFlags>(Rhi::SdShaderStageFlag::Vertex) },
			{ "texture0", Rhi::SdShaderResourceType::Texture2D, 0, 0, static_cast<Rhi::SdShaderStageFlags>(Rhi::SdShaderStageFlag::Pixel) },
			{ "sampler0", Rhi::SdShaderResourceType::Sampler, 0, 0, static_cast<Rhi::SdShaderStageFlags>(Rhi::SdShaderStageFlag::Pixel) }
		};
		resourceSetLayout = rhiDevice.CreateResourceSetLayout({ bindings, "Sodium.Gui.Default.ResourceLayout" });
		if (!resourceSetLayout.IsValid())
			return false;

		sampler = rhiDevice.CreateSampler({ Rhi::SdFilterMode::Linear, Rhi::SdFilterMode::Linear, Rhi::SdFilterMode::Linear, Rhi::SdAddressMode::Clamp, Rhi::SdAddressMode::Clamp, Rhi::SdAddressMode::Clamp, "Sodium.Gui.Default.Sampler" });
		if (!sampler.IsValid())
			return false;

		const Rhi::SdBufferDesc constantDesc =
		{
			sizeof(FrameConstants),
			static_cast<Rhi::SdBufferUsageFlags>(Rhi::SdBufferUsage::Uniform),
			Rhi::SdMemoryUsage::CpuToGpu,
			"Sodium.Gui.FrameConstants"
		};
		frameConstantBuffer = rhiDevice.CreateBuffer(constantDesc, nullptr);
		if (!frameConstantBuffer.IsValid())
			return false;

		Rhi::SdGraphicsPipelineDesc pipelineDesc = {};
		pipelineDesc.vertexShader = vertexShader;
		pipelineDesc.pixelShader = pixelShader;
		pipelineDesc.vertexLayout = vertexLayout;
		pipelineDesc.resourceSetLayout = resourceSetLayout;
		pipelineDesc.topology = Rhi::SdPrimitiveTopology::TriangleList;
		pipelineDesc.colorFormat = Rhi::SdTextureFormat::Bgra8Unorm;
		pipelineDesc.debugName = "Sodium.Gui.Default.Pipeline";
		pipeline = rhiDevice.CreateGraphicsPipeline(pipelineDesc);
		return pipeline.IsValid();
	}

	bool SdDx11Renderer::EnsureBuffers(SdUInt32 vertexCount, SdUInt32 indexCount)
	{
		if (vertexCount > vertexCapacity)
		{
			if (vertexBuffer.IsValid())
				rhiDevice.DestroyBuffer(vertexBuffer);
			vertexCapacity = std::max(vertexCount, vertexCapacity == 0 ? 1024u : vertexCapacity * 2u);
			const Rhi::SdBufferDesc desc =
			{
				static_cast<SdUInt64>(vertexCapacity) * sizeof(SdVertex),
				static_cast<Rhi::SdBufferUsageFlags>(Rhi::SdBufferUsage::Vertex),
				Rhi::SdMemoryUsage::CpuToGpu,
				"Sodium.Gui.VertexBuffer"
			};
			vertexBuffer = rhiDevice.CreateBuffer(desc, nullptr);
			if (!vertexBuffer.IsValid())
				return false;
		}

		if (indexCount > indexCapacity)
		{
			if (indexBuffer.IsValid())
				rhiDevice.DestroyBuffer(indexBuffer);
			indexCapacity = std::max(indexCount, indexCapacity == 0 ? 2048u : indexCapacity * 2u);
			const Rhi::SdBufferDesc desc =
			{
				static_cast<SdUInt64>(indexCapacity) * sizeof(SdUInt32),
				static_cast<Rhi::SdBufferUsageFlags>(Rhi::SdBufferUsage::Index),
				Rhi::SdMemoryUsage::CpuToGpu,
				"Sodium.Gui.IndexBuffer"
			};
			indexBuffer = rhiDevice.CreateBuffer(desc, nullptr);
			if (!indexBuffer.IsValid())
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

		if (!rhiDevice.UpdateBuffer(vertexBuffer, packet.vertices.data(), packet.vertices.size() * sizeof(SdVertex), 0))
			return false;
		if (!rhiDevice.UpdateBuffer(indexBuffer, packet.indices.data(), packet.indices.size() * sizeof(SdUInt32), 0))
			return false;
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

	bool SdDx11Renderer::RebuildTextureResourceSet(TextureEntry& entry)
	{
		if (!entry.texture.IsValid() || !sampler.IsValid() || !frameConstantBuffer.IsValid() || !resourceSetLayout.IsValid())
			return false;

		if (entry.resourceSet.IsValid())
			rhiDevice.DestroyResourceSet(entry.resourceSet);

		const Rhi::SdBoundTexture textures[] =
		{
			{ 0, entry.texture }
		};
		const Rhi::SdBoundSampler samplers[] =
		{
			{ 0, sampler }
		};
		const Rhi::SdBoundBuffer buffers[] =
		{
			{ 0, frameConstantBuffer, 0, sizeof(FrameConstants) }
		};
		const Rhi::SdResourceSetDesc desc =
		{
			resourceSetLayout,
			textures,
			samplers,
			buffers,
			"Sodium.Gui.TextureResourceSet"
		};
		entry.resourceSet = rhiDevice.CreateResourceSet(desc);
		return entry.resourceSet.IsValid();
	}

	bool SdDx11Renderer::UploadTexture(const SdUploadRequest& request)
	{
		if (!request.texture.IsValid() || request.width == 0 || request.height == 0 || request.pixels.empty())
			return false;

		TextureEntry& entry = EnsureTextureEntry(request.texture);
		if (!entry.texture.IsValid() || entry.width != request.width || entry.height != request.height || entry.generation != request.texture.generation)
		{
			if (entry.resourceSet.IsValid())
			{
				rhiDevice.DestroyResourceSet(entry.resourceSet);
				entry.resourceSet = {};
			}
			if (entry.texture.IsValid())
				rhiDevice.DestroyTexture(entry.texture);

			const Rhi::SdTextureDesc desc =
			{
				request.width,
				request.height,
				1,
				1,
				Rhi::SdTextureFormat::Rgba8Unorm,
				static_cast<Rhi::SdTextureUsageFlags>(Rhi::SdTextureUsage::ShaderRead),
				1,
				false,
				"Sodium.Gui.UploadedTexture"
			};
			entry.texture = rhiDevice.CreateTexture(desc);
			if (!entry.texture.IsValid())
				return false;
			if (!RebuildTextureResourceSet(entry))
			{
				rhiDevice.DestroyTexture(entry.texture);
				entry.texture = {};
				return false;
			}
			entry.width = request.width;
			entry.height = request.height;
			entry.generation = request.texture.generation;
			entry.occupied = true;
		}

		return rhiDevice.UpdateTexture(entry.texture, request.pixels.data(), request.width * sizeof(SdUInt32));
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

		rhiDevice.UpdateBuffer(frameConstantBuffer, &constants, sizeof(constants), 0);

		Rhi::ISdCommandEncoder& encoder = rhiDevice.GetImmediateEncoder();
		encoder.SetPipeline(pipeline);
		encoder.SetVertexBuffer(0, vertexBuffer, sizeof(SdVertex), 0);
		encoder.SetIndexBuffer(indexBuffer, Rhi::SdIndexFormat::UInt32, 0);
		encoder.SetViewport({ 0.0f, 0.0f, displaySize.x, displaySize.y, 0.0f, 1.0f });
	}

	bool SdDx11Renderer::RenderBackdropBlur(const SdBackdropBlurDraw& blur, const SdRendererFrameInfo& frameInfo)
	{
		if (!transientTexturePool || blur.radius <= 0.0f)
			return false;

		const SdRect frameRect = { 0.0f, 0.0f, frameInfo.displaySize.x, frameInfo.displaySize.y };
		const SdRect clippedRect = IntersectRect(IntersectRect(blur.rect, blur.clipRect), frameRect);
		if (clippedRect.Width() <= 0.0f || clippedRect.Height() <= 0.0f)
			return false;

		const float capturePadding = std::ceil(std::max(0.0f, blur.radius));
		const SdRect captureRect = IntersectRect(ExpandRect(clippedRect, capturePadding), frameRect);
		const Rhi::SdRectI sourceRect =
		{
			static_cast<SdInt32>(std::floor(captureRect.min.x)),
			static_cast<SdInt32>(std::floor(captureRect.min.y)),
			static_cast<SdInt32>(std::ceil(captureRect.max.x)),
			static_cast<SdInt32>(std::ceil(captureRect.max.y))
		};
		const SdUInt32 captureWidth = std::max(1u, sourceRect.Width());
		const SdUInt32 captureHeight = std::max(1u, sourceRect.Height());
		const SdRect captureBounds =
		{
			static_cast<float>(sourceRect.left),
			static_cast<float>(sourceRect.top),
			static_cast<float>(sourceRect.right),
			static_cast<float>(sourceRect.bottom)
		};

		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> currentRtv = {};
		deviceContext->OMGetRenderTargets(1, currentRtv.GetAddressOf(), nullptr);
		if (!currentRtv)
			return false;

		Microsoft::WRL::ComPtr<ID3D11Resource> targetResource = {};
		currentRtv->GetResource(targetResource.GetAddressOf());
		Microsoft::WRL::ComPtr<ID3D11Texture2D> targetTexture = {};
		if (!targetResource || FAILED(targetResource.As(&targetTexture)))
			return false;

		D3D11_TEXTURE2D_DESC targetDesc = {};
		targetTexture->GetDesc(&targetDesc);

		const Rhi::SdTextureDesc captureDesc =
		{
			captureWidth,
			captureHeight,
			1,
			1,
			MapDxgiFormatToRhi(targetDesc.Format),
			static_cast<Rhi::SdTextureUsageFlags>(Rhi::SdTextureUsage::ShaderRead),
			1,
			false,
			"Sodium.Window.BackdropCapture"
		};
		Rhi::SdTextureHandle captureTexture = rhiDevice.CreateTexture(captureDesc);
		if (!captureTexture.IsValid())
			return false;
		if (!rhiDevice.CopyCurrentRenderTargetToTexture(captureTexture, sourceRect))
		{
			rhiDevice.DestroyTexture(captureTexture);
			return false;
		}

		const Rhi::SdTextureDesc targetImportDesc =
		{
			targetDesc.Width,
			targetDesc.Height,
			1,
			1,
			MapDxgiFormatToRhi(targetDesc.Format),
			static_cast<Rhi::SdTextureUsageFlags>(Rhi::SdTextureUsage::RenderTarget),
			targetDesc.SampleDesc.Count,
			false,
			"Sodium.Window.CurrentRenderTarget"
		};
		Rhi::SdTextureHandle targetHandle = rhiDevice.ImportTexture2D(targetTexture.Get(), targetImportDesc);
		if (!targetHandle.IsValid())
		{
			rhiDevice.DestroyTexture(captureTexture);
			return false;
		}

		transientTexturePool->BeginFrame();
		Rhi::SdRenderGraph graph(*transientTexturePool);
		SdEffectResourceCache resources = effectResources;
		SdBlurEffect blurEffect = {};
		blurEffect.radius = blur.radius;
		Rhi::SdRenderGraphTexture source = graph.ImportTexture(captureTexture, "Sodium.Window.BackdropSource");
		Rhi::SdRenderGraphTexture target = graph.ImportTexture(targetHandle, "Sodium.Window.BackdropTarget");
		SdEffectBuildContext effectContext =
		{
			graph,
			source,
			{},
			target,
			{},
			blur.rect,
			{
				captureBounds.min.x,
				captureBounds.min.y,
				captureBounds.max.x,
				captureBounds.max.y
			},
			IntersectRect(blur.clipRect, frameRect),
			captureWidth,
			captureHeight,
			resources,
			*transientTexturePool,
			rhiDevice.GetCaps(),
			&rhiDevice,
			blur.cornerRadius
		};
		effectContext.cornerRadii = blur.cornerRadii;
		blurEffect.BuildGraph(effectContext);
		const bool compiled = graph.Compile();
		if (compiled)
			graph.Execute(rhiDevice.GetImmediateEncoder());
		graph.Reset();
		transientTexturePool->EndFrame();

		rhiDevice.DestroyTexture(targetHandle);
		rhiDevice.DestroyTexture(captureTexture);
		return compiled;
	}

	bool SdDx11Renderer::RenderDropShadow(const SdDropShadowDraw& shadow, const SdRendererFrameInfo& frameInfo)
	{
		if (shadow.radius <= 0.0f || shadow.color.a == 0)
			return false;
		if (!effectResources.GetShadowResourceSetLayout().IsValid()
			|| !effectResources.GetShadowParamsBuffer().IsValid())
		{
			return false;
		}

		const SdRect frameRect = { 0.0f, 0.0f, frameInfo.displaySize.x, frameInfo.displaySize.y };
		const float spread = std::max(0.0f, shadow.spread);
		const SdRect shadowRect =
		{
			shadow.rect.min.x + shadow.offset.x - spread,
			shadow.rect.min.y + shadow.offset.y - spread,
			shadow.rect.max.x + shadow.offset.x + spread,
			shadow.rect.max.y + shadow.offset.y + spread
		};
		const SdRect drawRect = ExpandRect(shadowRect, std::ceil(std::max(0.0f, shadow.radius)));
		const SdRect clippedRect = IntersectRect(IntersectRect(drawRect, shadow.clipRect), frameRect);
		if (clippedRect.Width() <= 0.0f || clippedRect.Height() <= 0.0f)
			return false;

		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> currentRtv = {};
		deviceContext->OMGetRenderTargets(1, currentRtv.GetAddressOf(), nullptr);
		if (!currentRtv)
			return false;

		Microsoft::WRL::ComPtr<ID3D11Resource> targetResource = {};
		currentRtv->GetResource(targetResource.GetAddressOf());
		Microsoft::WRL::ComPtr<ID3D11Texture2D> targetTexture = {};
		if (!targetResource || FAILED(targetResource.As(&targetTexture)))
			return false;

		D3D11_TEXTURE2D_DESC targetDesc = {};
		targetTexture->GetDesc(&targetDesc);
		const Rhi::SdTextureFormat targetFormat = MapDxgiFormatToRhi(targetDesc.Format);
		const Rhi::SdPipelineHandle shadowPipeline = effectResources.GetShadowPipeline(targetFormat);
		if (!shadowPipeline.IsValid())
			return false;

		const Rhi::SdTextureDesc targetImportDesc =
		{
			targetDesc.Width,
			targetDesc.Height,
			1,
			1,
			targetFormat,
			static_cast<Rhi::SdTextureUsageFlags>(Rhi::SdTextureUsage::RenderTarget),
			targetDesc.SampleDesc.Count,
			false,
			"Sodium.Window.ShadowTarget"
		};
		Rhi::SdTextureHandle targetHandle = rhiDevice.ImportTexture2D(targetTexture.Get(), targetImportDesc);
		if (!targetHandle.IsValid())
			return false;

		const SdColorLinear color = shadow.color.ToLinear();
		SdShadowParams params = {};
		params.rectMin = shadow.rect.min;
		params.rectMax = shadow.rect.max;
		params.offset = shadow.offset;
		params.radius = std::max(
			std::max(shadow.cornerRadii.topLeft, shadow.cornerRadii.topRight),
			std::max(shadow.cornerRadii.bottomRight, shadow.cornerRadii.bottomLeft));
		params.blurRadius = shadow.radius;
		params.spread = shadow.spread;
		params.outerOnly = 1.0f;
		params.shadowMode = 0.0f;
		params.color = { color.r, color.g, color.b, color.a };
		params.cornerRadii = {
			shadow.cornerRadii.topLeft,
			shadow.cornerRadii.topRight,
			shadow.cornerRadii.bottomRight,
			shadow.cornerRadii.bottomLeft
		};
		rhiDevice.UpdateBuffer(effectResources.GetShadowParamsBuffer(), &params, sizeof(params), 0);

		const Rhi::SdBoundBuffer buffers[] =
		{
			{ 0, effectResources.GetShadowParamsBuffer(), 0, sizeof(SdShadowParams) }
		};
		const Rhi::SdResourceSetDesc resourceSetDesc =
		{
			effectResources.GetShadowResourceSetLayout(),
			{},
			{},
			buffers,
			"Sodium.Shadow.ResourceSet"
		};
		const Rhi::SdResourceSetHandle resourceSet = rhiDevice.CreateResourceSet(resourceSetDesc);
		if (!resourceSet.IsValid())
		{
			rhiDevice.DestroyTexture(targetHandle);
			return false;
		}

		const Rhi::SdRectI renderArea =
		{
			static_cast<SdInt32>(std::floor(clippedRect.min.x)),
			static_cast<SdInt32>(std::floor(clippedRect.min.y)),
			static_cast<SdInt32>(std::ceil(clippedRect.max.x)),
			static_cast<SdInt32>(std::ceil(clippedRect.max.y))
		};
		const Rhi::SdRenderPassColorAttachment colorAttachment =
		{
			targetHandle,
			Rhi::SdLoadOp::Load,
			Rhi::SdStoreOp::Store,
			{}
		};
		const std::array<Rhi::SdRenderPassColorAttachment, 1> colorAttachments = { colorAttachment };
		const Rhi::SdRenderPassDesc renderPass =
		{
			colorAttachments,
			{},
			renderArea,
			"Sodium.Shadow.RenderPass"
		};

		Rhi::ISdCommandEncoder& encoder = rhiDevice.GetImmediateEncoder();
		encoder.BeginRenderPass(renderPass);
		encoder.SetPipeline(shadowPipeline);
		encoder.SetResourceSet(0, resourceSet);
		encoder.SetViewport({
			static_cast<float>(renderArea.left),
			static_cast<float>(renderArea.top),
			static_cast<float>(renderArea.Width()),
			static_cast<float>(renderArea.Height()),
			0.0f,
			1.0f
		});
		encoder.SetScissorRect(renderArea);
		encoder.Draw(3, 1, 0, 0);
		encoder.EndRenderPass();

		rhiDevice.DestroyResourceSet(resourceSet);
		rhiDevice.DestroyTexture(targetHandle);
		return true;
	}

	bool SdDx11Renderer::RenderInnerShadow(const SdInnerShadowDraw& shadow, const SdRendererFrameInfo& frameInfo)
	{
		if (shadow.radius <= 0.0f || shadow.color.a == 0)
			return false;
		if (!effectResources.GetShadowResourceSetLayout().IsValid()
			|| !effectResources.GetShadowParamsBuffer().IsValid())
		{
			return false;
		}

		const SdRect frameRect = { 0.0f, 0.0f, frameInfo.displaySize.x, frameInfo.displaySize.y };
		const float spread = std::max(0.0f, shadow.spread);
		const SdRect shadowRect =
		{
			shadow.rect.min.x + shadow.offset.x - spread,
			shadow.rect.min.y + shadow.offset.y - spread,
			shadow.rect.max.x + shadow.offset.x + spread,
			shadow.rect.max.y + shadow.offset.y + spread
		};
		const SdRect drawRect = ExpandRect(shadowRect, std::ceil(std::max(0.0f, shadow.radius)));
		const SdRect clippedRect = IntersectRect(IntersectRect(drawRect, shadow.clipRect), frameRect);
		if (clippedRect.Width() <= 0.0f || clippedRect.Height() <= 0.0f)
			return false;

		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> currentRtv = {};
		deviceContext->OMGetRenderTargets(1, currentRtv.GetAddressOf(), nullptr);
		if (!currentRtv)
			return false;

		Microsoft::WRL::ComPtr<ID3D11Resource> targetResource = {};
		currentRtv->GetResource(targetResource.GetAddressOf());
		Microsoft::WRL::ComPtr<ID3D11Texture2D> targetTexture = {};
		if (!targetResource || FAILED(targetResource.As(&targetTexture)))
			return false;

		D3D11_TEXTURE2D_DESC targetDesc = {};
		targetTexture->GetDesc(&targetDesc);
		const Rhi::SdTextureFormat targetFormat = MapDxgiFormatToRhi(targetDesc.Format);
		const Rhi::SdPipelineHandle shadowPipeline = effectResources.GetShadowPipeline(targetFormat);
		if (!shadowPipeline.IsValid())
			return false;

		const Rhi::SdTextureDesc targetImportDesc =
		{
			targetDesc.Width,
			targetDesc.Height,
			1,
			1,
			targetFormat,
			static_cast<Rhi::SdTextureUsageFlags>(Rhi::SdTextureUsage::RenderTarget),
			targetDesc.SampleDesc.Count,
			false,
			"Sodium.Window.InnerShadowTarget"
		};
		Rhi::SdTextureHandle targetHandle = rhiDevice.ImportTexture2D(targetTexture.Get(), targetImportDesc);
		if (!targetHandle.IsValid())
			return false;

		const SdColorLinear color = shadow.color.ToLinear();
		SdShadowParams params = {};
		params.rectMin = shadow.rect.min;
		params.rectMax = shadow.rect.max;
		params.offset = shadow.offset;
		params.radius = std::max(
			std::max(shadow.cornerRadii.topLeft, shadow.cornerRadii.topRight),
			std::max(shadow.cornerRadii.bottomRight, shadow.cornerRadii.bottomLeft));
		params.blurRadius = shadow.radius;
		params.spread = shadow.spread;
		params.outerOnly = 1.0f;
		params.shadowMode = 1.0f;
		params.color = { color.r, color.g, color.b, color.a };
		params.cornerRadii = {
			shadow.cornerRadii.topLeft,
			shadow.cornerRadii.topRight,
			shadow.cornerRadii.bottomRight,
			shadow.cornerRadii.bottomLeft
		};
		rhiDevice.UpdateBuffer(effectResources.GetShadowParamsBuffer(), &params, sizeof(params), 0);

		const Rhi::SdBoundBuffer buffers[] =
		{
			{ 0, effectResources.GetShadowParamsBuffer(), 0, sizeof(SdShadowParams) }
		};
		const Rhi::SdResourceSetDesc resourceSetDesc =
		{
			effectResources.GetShadowResourceSetLayout(),
			{},
			{},
			buffers,
			"Sodium.InnerShadow.ResourceSet"
		};
		const Rhi::SdResourceSetHandle resourceSet = rhiDevice.CreateResourceSet(resourceSetDesc);
		if (!resourceSet.IsValid())
		{
			rhiDevice.DestroyTexture(targetHandle);
			return false;
		}

		const Rhi::SdRectI renderArea =
		{
			static_cast<SdInt32>(std::floor(clippedRect.min.x)),
			static_cast<SdInt32>(std::floor(clippedRect.min.y)),
			static_cast<SdInt32>(std::ceil(clippedRect.max.x)),
			static_cast<SdInt32>(std::ceil(clippedRect.max.y))
		};
		const Rhi::SdRenderPassColorAttachment colorAttachment =
		{
			targetHandle,
			Rhi::SdLoadOp::Load,
			Rhi::SdStoreOp::Store,
			{}
		};
		const std::array<Rhi::SdRenderPassColorAttachment, 1> colorAttachments = { colorAttachment };
		const Rhi::SdRenderPassDesc renderPass =
		{
			colorAttachments,
			{},
			renderArea,
			"Sodium.InnerShadow.RenderPass"
		};

		Rhi::ISdCommandEncoder& encoder = rhiDevice.GetImmediateEncoder();
		encoder.BeginRenderPass(renderPass);
		encoder.SetPipeline(shadowPipeline);
		encoder.SetResourceSet(0, resourceSet);
		encoder.SetViewport({
			static_cast<float>(renderArea.left),
			static_cast<float>(renderArea.top),
			static_cast<float>(renderArea.Width()),
			static_cast<float>(renderArea.Height()),
			0.0f,
			1.0f
		});
		encoder.SetScissorRect(renderArea);
		encoder.Draw(3, 1, 0, 0);
		encoder.EndRenderPass();

		rhiDevice.DestroyResourceSet(resourceSet);
		rhiDevice.DestroyTexture(targetHandle);
		return true;
	}

	SdTextureHandle SdDx11Renderer::CreateTexture(SdUInt32 width, SdUInt32 height, const void* rgbaPixels)
	{
		if (width == 0 || height == 0 || !rgbaPixels)
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
		const Rhi::SdTextureDesc desc =
		{
			width,
			height,
			1,
			1,
			Rhi::SdTextureFormat::Rgba8Unorm,
			static_cast<Rhi::SdTextureUsageFlags>(Rhi::SdTextureUsage::ShaderRead),
			1,
			false,
			""
		};
		entry.texture = rhiDevice.CreateTexture(desc);
		if (!entry.texture.IsValid())
			return {};
		if (!rhiDevice.UpdateTexture(entry.texture, rgbaPixels, width * sizeof(SdUInt32)))
		{
			rhiDevice.DestroyTexture(entry.texture);
			entry.texture = {};
			return {};
		}
		entry.width = width;
		entry.height = height;
		entry.generation = entry.generation == 0 ? 1 : entry.generation + 1;
		entry.occupied = true;
		if (!RebuildTextureResourceSet(entry))
		{
			rhiDevice.DestroyTexture(entry.texture);
			entry.texture = {};
			entry.occupied = false;
			return {};
		}
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
		if (entry->resourceSet.IsValid())
			rhiDevice.DestroyResourceSet(entry->resourceSet);
		if (entry->texture.IsValid())
			rhiDevice.DestroyTexture(entry->texture);
		entry->resourceSet = {};
		entry->texture = {};
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
		if (!entry.occupied || entry.generation != texture.generation || !entry.resourceSet.IsValid())
			return nullptr;
		return &entry;
	}

	void SdDx11Renderer::Render(const SdRendererFrameInfo& frameInfo, const SdDrawPacket& packet)
	{
		if (!deviceContext)
			return;
		for (const SdUploadRequest& upload : packet.resourceUpdates)
			UploadTexture(upload);
		const bool hasGeometry = !packet.vertices.empty() && !packet.indices.empty();
		if (hasGeometry && !UploadBuffers(packet))
			return;

		Dx11PipelineStateSnapshot pipelineState(deviceContext.Get());
		if (hasGeometry)
			BindPipeline(frameInfo.displaySize);
		Rhi::ISdCommandEncoder& encoder = rhiDevice.GetImmediateEncoder();
		for (const SdDrawCommand& command : packet.commands)
		{
			if (command.kind == SdDrawCommandKind::BackdropBlur)
			{
				if (command.index < packet.backdropBlurs.size())
					RenderBackdropBlur(packet.backdropBlurs[command.index], frameInfo);
				if (hasGeometry)
					BindPipeline(frameInfo.displaySize);
				continue;
			}

			if (command.kind == SdDrawCommandKind::DropShadow)
			{
				if (command.index < packet.dropShadows.size())
					RenderDropShadow(packet.dropShadows[command.index], frameInfo);
				if (hasGeometry)
					BindPipeline(frameInfo.displaySize);
				continue;
			}

			if (command.kind == SdDrawCommandKind::InnerShadow)
			{
				if (command.index < packet.innerShadows.size())
					RenderInnerShadow(packet.innerShadows[command.index], frameInfo);
				if (hasGeometry)
					BindPipeline(frameInfo.displaySize);
				continue;
			}

			if (!hasGeometry)
				continue;

			if (command.kind != SdDrawCommandKind::OwnedBatch || command.index >= packet.batches.size())
				continue;
			const SdRenderBatch& batch = packet.batches[command.index];
			if (batch.indexCount == 0)
				continue;

			TextureEntry* texture = TryGetTexture(batch.texture);
			if (!texture)
				continue;

			Rhi::SdRectI scissor = {};
			scissor.left = static_cast<SdInt32>(std::floor(std::max(0.0f, batch.clipRect.min.x)));
			scissor.top = static_cast<SdInt32>(std::floor(std::max(0.0f, batch.clipRect.min.y)));
			scissor.right = static_cast<SdInt32>(std::ceil(std::min(frameInfo.displaySize.x, batch.clipRect.max.x)));
			scissor.bottom = static_cast<SdInt32>(std::ceil(std::min(frameInfo.displaySize.y, batch.clipRect.max.y)));
			if (scissor.left >= scissor.right || scissor.top >= scissor.bottom)
				continue;
			encoder.SetResourceSet(0, texture->resourceSet);
			encoder.SetScissorRect(scissor);
			encoder.DrawIndexed(batch.indexCount, 1, batch.indexOffset, 0, 0);
		}
	}
}
