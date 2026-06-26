#include "SdDx9Renderer.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

#pragma comment(lib, "d3d9.lib")

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
		constexpr DWORD kVertexFvf = D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1;
		constexpr SdUInt32 kReservedFontAtlasTextureIndex = 64;

		SdUInt32 GetClientWidth(HWND windowHandle, SdUInt32 fallback) noexcept
		{
			RECT rect = {};
			if (windowHandle && ::GetClientRect(windowHandle, &rect) && rect.right > rect.left)
				return static_cast<SdUInt32>(rect.right - rect.left);
			return fallback;
		}

		SdUInt32 GetClientHeight(HWND windowHandle, SdUInt32 fallback) noexcept
		{
			RECT rect = {};
			if (windowHandle && ::GetClientRect(windowHandle, &rect) && rect.bottom > rect.top)
				return static_cast<SdUInt32>(rect.bottom - rect.top);
			return fallback;
		}

		D3DFORMAT MapBackBufferFormat(Rhi::SdTextureFormat format) noexcept
		{
			switch (format)
			{
			case Rhi::SdTextureFormat::Bgra8Unorm:
			case Rhi::SdTextureFormat::Bgra8UnormSrgb:
			case Rhi::SdTextureFormat::Rgba8Unorm:
			case Rhi::SdTextureFormat::Rgba8UnormSrgb:
				return D3DFMT_A8R8G8B8;
			default:
				return D3DFMT_UNKNOWN;
			}
		}

		SdUInt32 ConvertRgbaToA8R8G8B8(SdUInt32 color) noexcept
		{
			const SdUInt32 r = color & 0xFFu;
			const SdUInt32 g = (color >> 8) & 0xFFu;
			const SdUInt32 b = (color >> 16) & 0xFFu;
			const SdUInt32 a = (color >> 24) & 0xFFu;
			return (a << 24) | (r << 16) | (g << 8) | b;
		}

		template<typename T>
		bool FitsUIntByteCount(T count, T stride) noexcept
		{
			if (count == 0)
				return true;
			return count <= static_cast<T>(std::numeric_limits<UINT>::max()) / stride;
		}
	}

	bool SdDx9GpuDevice::Initialize()
	{
		caps = {};
		caps.maxColorAttachments = 1;
		caps.maxTextureSize = 4096;
		caps.uniformBufferOffsetAlignment = 16;
		caps.supportsComputeShader = false;
		caps.supportsStorageTexture = false;
		caps.supportsIndependentBlend = false;
		caps.supportsFramebufferFetch = false;
		caps.supportsMultisampleRenderTarget = false;
		caps.supportedShaderLanguages = static_cast<Rhi::SdShaderLanguageFlags>(Rhi::SdShaderLanguageFlag::Hlsl);
		lastStatus = Rhi::SdRhiStatus::Ok;
		initialized = true;
		return true;
	}

	void SdDx9GpuDevice::Shutdown()
	{
		pipelines.clear();
		resourceSets.clear();
		resourceSetLayouts.clear();
		vertexLayouts.clear();
		samplers.clear();
		shaders.clear();
		buffers.clear();
		textures.clear();
		caps = {};
		lastStatus = Rhi::SdRhiStatus::Ok;
		initialized = false;
	}

	Rhi::SdTextureHandle SdDx9GpuDevice::CreateTexture(const Rhi::SdTextureDesc& desc)
	{
		if (!initialized || desc.width == 0 || desc.height == 0)
		{
			lastStatus = Rhi::SdRhiStatus::InvalidArgument;
			return {};
		}

		lastStatus = Rhi::SdRhiStatus::Ok;
		return Allocate<Rhi::SdTextureHandle>(textures);
	}

	void SdDx9GpuDevice::DestroyTexture(Rhi::SdTextureHandle texture)
	{
		Release(textures, texture);
	}

	bool SdDx9GpuDevice::UpdateTexture(Rhi::SdTextureHandle texture, const void* pixels, SdUInt32 rowPitch)
	{
		return initialized && pixels && rowPitch != 0 && IsValid(textures, texture);
	}

	Rhi::SdBufferHandle SdDx9GpuDevice::CreateBuffer(const Rhi::SdBufferDesc& desc, const void* initialData)
	{
		(void)initialData;
		if (!initialized || desc.size == 0)
		{
			lastStatus = Rhi::SdRhiStatus::InvalidArgument;
			return {};
		}

		lastStatus = Rhi::SdRhiStatus::Ok;
		return Allocate<Rhi::SdBufferHandle>(buffers);
	}

	void SdDx9GpuDevice::DestroyBuffer(Rhi::SdBufferHandle buffer)
	{
		Release(buffers, buffer);
	}

	bool SdDx9GpuDevice::UpdateBuffer(Rhi::SdBufferHandle buffer, const void* data, SdUInt64 size, SdUInt64 offset)
	{
		(void)offset;
		return initialized && data && size != 0 && IsValid(buffers, buffer);
	}

	Rhi::SdShaderHandle SdDx9GpuDevice::CreateShader(const Rhi::SdShaderDesc& desc)
	{
		if (!initialized || desc.bytecode.empty())
		{
			lastStatus = Rhi::SdRhiStatus::InvalidArgument;
			return {};
		}

		lastStatus = Rhi::SdRhiStatus::Ok;
		return Allocate<Rhi::SdShaderHandle>(shaders);
	}

	Rhi::SdShaderHandle SdDx9GpuDevice::CreateShaderFromSource(const Rhi::SdShaderSourceDesc& desc)
	{
		if (!initialized || desc.source.empty() || desc.language != Rhi::SdShaderLanguage::Hlsl)
		{
			lastStatus = Rhi::SdRhiStatus::InvalidArgument;
			return {};
		}

		lastStatus = Rhi::SdRhiStatus::Ok;
		return Allocate<Rhi::SdShaderHandle>(shaders);
	}

	void SdDx9GpuDevice::DestroyShader(Rhi::SdShaderHandle shader)
	{
		Release(shaders, shader);
	}

	Rhi::SdSamplerHandle SdDx9GpuDevice::CreateSampler(const Rhi::SdSamplerDesc& desc)
	{
		(void)desc;
		if (!initialized)
			return {};

		lastStatus = Rhi::SdRhiStatus::Ok;
		return Allocate<Rhi::SdSamplerHandle>(samplers);
	}

	void SdDx9GpuDevice::DestroySampler(Rhi::SdSamplerHandle sampler)
	{
		Release(samplers, sampler);
	}

	Rhi::SdVertexLayoutHandle SdDx9GpuDevice::CreateVertexLayout(const Rhi::SdVertexLayoutDesc& desc)
	{
		(void)desc;
		if (!initialized)
			return {};

		lastStatus = Rhi::SdRhiStatus::Ok;
		return Allocate<Rhi::SdVertexLayoutHandle>(vertexLayouts);
	}

	void SdDx9GpuDevice::DestroyVertexLayout(Rhi::SdVertexLayoutHandle layout)
	{
		Release(vertexLayouts, layout);
	}

	Rhi::SdResourceSetLayoutHandle SdDx9GpuDevice::CreateResourceSetLayout(const Rhi::SdResourceSetLayoutDesc& desc)
	{
		(void)desc;
		if (!initialized)
			return {};

		lastStatus = Rhi::SdRhiStatus::Ok;
		return Allocate<Rhi::SdResourceSetLayoutHandle>(resourceSetLayouts);
	}

	void SdDx9GpuDevice::DestroyResourceSetLayout(Rhi::SdResourceSetLayoutHandle layout)
	{
		Release(resourceSetLayouts, layout);
	}

	Rhi::SdResourceSetHandle SdDx9GpuDevice::CreateResourceSet(const Rhi::SdResourceSetDesc& desc)
	{
		if (!initialized || !IsValid(resourceSetLayouts, desc.layout))
			return {};

		lastStatus = Rhi::SdRhiStatus::Ok;
		return Allocate<Rhi::SdResourceSetHandle>(resourceSets);
	}

	void SdDx9GpuDevice::DestroyResourceSet(Rhi::SdResourceSetHandle resourceSet)
	{
		Release(resourceSets, resourceSet);
	}

	Rhi::SdPipelineHandle SdDx9GpuDevice::CreateGraphicsPipeline(const Rhi::SdGraphicsPipelineDesc& desc)
	{
		if (!initialized || !IsValid(shaders, desc.vertexShader) || !IsValid(shaders, desc.pixelShader))
			return {};

		lastStatus = Rhi::SdRhiStatus::Ok;
		return Allocate<Rhi::SdPipelineHandle>(pipelines);
	}

	void SdDx9GpuDevice::DestroyPipeline(Rhi::SdPipelineHandle pipeline)
	{
		Release(pipelines, pipeline);
	}

	bool SdDx9Renderer::Initialize(const Config& config)
	{
		Shutdown();

		if (config.device)
		{
			device = config.device;
			ownsDeviceResources = false;
			D3DVIEWPORT9 viewport = {};
			if (SUCCEEDED(device->GetViewport(&viewport)))
			{
				swapchainWidth = viewport.Width;
				swapchainHeight = viewport.Height;
			}
		}
		else if (!CreateOwnedDeviceAndSwapchain(config))
		{
			return false;
		}

		deviceLost = false;
		return rhiDevice.Initialize();
	}

	void SdDx9Renderer::Shutdown()
	{
		if (sceneActive && device)
			device->EndScene();
		sceneActive = false;
		OnLostDevice();
		for (TextureEntry& texture : textures)
			texture = {};
		textures.clear();
		convertedVertices.clear();
		rhiDevice.Shutdown();
		device.Reset();
		direct3d.Reset();
		presentationParameters = {};
		swapchainWidth = 0;
		swapchainHeight = 0;
		ownsDeviceResources = false;
		deviceLost = false;
	}

	bool SdDx9Renderer::CreateOwnedDeviceAndSwapchain(const SdDx9RendererConfig& config)
	{
		HWND windowHandle = static_cast<HWND>(config.swapchain.nativeWindow);
		if (config.usePresentationParameters && config.presentationParameters.hDeviceWindow)
			windowHandle = config.presentationParameters.hDeviceWindow;
		if (!windowHandle)
			return false;

		direct3d.Attach(::Direct3DCreate9(D3D_SDK_VERSION));
		if (!direct3d)
			return false;

		presentationParameters = config.usePresentationParameters ? config.presentationParameters : D3DPRESENT_PARAMETERS{};
		presentationParameters.Windowed = TRUE;
		presentationParameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
		presentationParameters.hDeviceWindow = windowHandle;
		presentationParameters.EnableAutoDepthStencil = FALSE;
		presentationParameters.BackBufferCount = std::max<SdUInt32>(1, config.swapchain.bufferCount);
		presentationParameters.BackBufferWidth = config.swapchain.width != 0
			? config.swapchain.width
			: GetClientWidth(windowHandle, 1);
		presentationParameters.BackBufferHeight = config.swapchain.height != 0
			? config.swapchain.height
			: GetClientHeight(windowHandle, 1);
		presentationParameters.BackBufferFormat = MapBackBufferFormat(config.swapchain.colorFormat);
		if (presentationParameters.BackBufferFormat == D3DFMT_UNKNOWN)
			presentationParameters.BackBufferFormat = D3DFMT_A8R8G8B8;
		presentationParameters.PresentationInterval = config.swapchain.presentMode == Rhi::SdPresentMode::Fifo
			? D3DPRESENT_INTERVAL_ONE
			: D3DPRESENT_INTERVAL_IMMEDIATE;

		const DWORD behaviorFlags[] =
		{
			D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED,
			D3DCREATE_MIXED_VERTEXPROCESSING | D3DCREATE_MULTITHREADED,
			D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_MULTITHREADED
		};

		for (DWORD flags : behaviorFlags)
		{
			if (SUCCEEDED(direct3d->CreateDevice(
				D3DADAPTER_DEFAULT,
				D3DDEVTYPE_HAL,
				windowHandle,
				flags,
				&presentationParameters,
				device.ReleaseAndGetAddressOf())))
			{
				ownsDeviceResources = true;
				swapchainWidth = presentationParameters.BackBufferWidth;
				swapchainHeight = presentationParameters.BackBufferHeight;
				return true;
			}
		}

		return false;
	}

	bool SdDx9Renderer::ResetOwnedDevice()
	{
		if (!ownsDeviceResources || !device)
			return false;

		OnLostDevice();
		const HRESULT result = device->Reset(&presentationParameters);
		if (FAILED(result))
		{
			deviceLost = result == D3DERR_DEVICELOST || result == D3DERR_DEVICENOTRESET;
			return false;
		}

		return OnResetDevice();
	}

	bool SdDx9Renderer::Resize(SdUInt32 width, SdUInt32 height)
	{
		if (!ownsDeviceResources || !device || width == 0 || height == 0)
			return false;

		presentationParameters.BackBufferWidth = width;
		presentationParameters.BackBufferHeight = height;
		swapchainWidth = width;
		swapchainHeight = height;
		return ResetOwnedDevice();
	}

	bool SdDx9Renderer::BeginFrame(const std::array<float, 4>& clearColor)
	{
		if (!device)
			return false;

		if (ownsDeviceResources)
		{
			const HRESULT cooperativeLevel = device->TestCooperativeLevel();
			if (cooperativeLevel == D3DERR_DEVICELOST)
			{
				deviceLost = true;
				return false;
			}
			if (cooperativeLevel == D3DERR_DEVICENOTRESET && !ResetOwnedDevice())
				return false;

			const D3DCOLOR nativeClearColor = D3DCOLOR_COLORVALUE(clearColor[0], clearColor[1], clearColor[2], clearColor[3]);
			device->Clear(0, nullptr, D3DCLEAR_TARGET, nativeClearColor, 1.0f, 0);
			if (FAILED(device->BeginScene()))
				return false;
			sceneActive = true;
		}

		return true;
	}

	bool SdDx9Renderer::Present()
	{
		if (!device || !ownsDeviceResources)
			return false;

		if (sceneActive)
		{
			device->EndScene();
			sceneActive = false;
		}

		const HRESULT result = device->Present(nullptr, nullptr, nullptr, nullptr);
		if (result == D3DERR_DEVICELOST)
		{
			deviceLost = true;
			return false;
		}
		return SUCCEEDED(result);
	}

	bool SdDx9Renderer::IsOccluded() const noexcept
	{
		if (!device)
			return true;
		if (deviceLost)
			return true;
		return device->TestCooperativeLevel() == D3DERR_DEVICELOST;
	}

	void SdDx9Renderer::OnLostDevice()
	{
		vertexBuffer.Reset();
		indexBuffer.Reset();
		vertexCapacity = 0;
		indexCapacity = 0;
	}

	bool SdDx9Renderer::OnResetDevice()
	{
		deviceLost = false;
		return device.Get() != nullptr;
	}

	D3DCOLOR SdDx9Renderer::ConvertColor(SdUInt32 color) noexcept
	{
		return ConvertRgbaToA8R8G8B8(color);
	}

	Rhi::SdRectI SdDx9Renderer::MakeScissor(const SdRect& rect, SdVec2 displaySize) noexcept
	{
		Rhi::SdRectI scissor = {};
		scissor.left = static_cast<SdInt32>(std::floor(std::clamp(rect.min.x, 0.0f, displaySize.x)));
		scissor.top = static_cast<SdInt32>(std::floor(std::clamp(rect.min.y, 0.0f, displaySize.y)));
		scissor.right = static_cast<SdInt32>(std::ceil(std::clamp(rect.max.x, 0.0f, displaySize.x)));
		scissor.bottom = static_cast<SdInt32>(std::ceil(std::clamp(rect.max.y, 0.0f, displaySize.y)));
		if (scissor.right < scissor.left)
			scissor.right = scissor.left;
		if (scissor.bottom < scissor.top)
			scissor.bottom = scissor.top;
		return scissor;
	}

	void SdDx9Renderer::BindPipeline(SdVec2 displaySize)
	{
		if (!device)
			return;

		D3DVIEWPORT9 viewport = {};
		viewport.X = 0;
		viewport.Y = 0;
		viewport.Width = static_cast<DWORD>(std::max(1.0f, displaySize.x));
		viewport.Height = static_cast<DWORD>(std::max(1.0f, displaySize.y));
		viewport.MinZ = 0.0f;
		viewport.MaxZ = 1.0f;
		device->SetViewport(&viewport);

		device->SetVertexShader(nullptr);
		device->SetPixelShader(nullptr);
		device->SetFVF(kVertexFvf);
		device->SetRenderState(D3DRS_ZENABLE, FALSE);
		device->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
		device->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
		device->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
		device->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
		device->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
		device->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, TRUE);
		device->SetRenderState(D3DRS_BLENDOPALPHA, D3DBLENDOP_ADD);
		device->SetRenderState(D3DRS_SRCBLENDALPHA, D3DBLEND_ONE);
		device->SetRenderState(D3DRS_DESTBLENDALPHA, D3DBLEND_INVSRCALPHA);
		device->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
		device->SetRenderState(D3DRS_LIGHTING, FALSE);
		device->SetRenderState(D3DRS_FOGENABLE, FALSE);
		device->SetRenderState(D3DRS_SCISSORTESTENABLE, TRUE);
		device->SetRenderState(D3DRS_COLORWRITEENABLE, D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA);

		device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
		device->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
		device->SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
		device->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
		device->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
		device->SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
		device->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
		device->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);

		device->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
		device->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
		device->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
		device->SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
		device->SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
	}

	bool SdDx9Renderer::EnsureBuffers(SdUInt32 vertexCount, SdUInt32 indexCount)
	{
		if (!device)
			return false;
		if (!FitsUIntByteCount<SdUInt32>(vertexCount, static_cast<SdUInt32>(sizeof(Dx9Vertex)))
			|| !FitsUIntByteCount<SdUInt32>(indexCount, static_cast<SdUInt32>(sizeof(SdUInt32))))
		{
			return false;
		}

		if (vertexCount > vertexCapacity)
		{
			vertexCapacity = std::max<SdUInt32>(vertexCount, std::max<SdUInt32>(vertexCapacity * 2, 1024));
			vertexBuffer.Reset();
			if (FAILED(device->CreateVertexBuffer(
				vertexCapacity * static_cast<SdUInt32>(sizeof(Dx9Vertex)),
				D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY,
				kVertexFvf,
				D3DPOOL_DEFAULT,
				vertexBuffer.GetAddressOf(),
				nullptr)))
			{
				vertexCapacity = 0;
				return false;
			}
		}

		if (indexCount > indexCapacity)
		{
			indexCapacity = std::max<SdUInt32>(indexCount, std::max<SdUInt32>(indexCapacity * 2, 2048));
			indexBuffer.Reset();
			if (FAILED(device->CreateIndexBuffer(
				indexCapacity * static_cast<SdUInt32>(sizeof(SdUInt32)),
				D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY,
				D3DFMT_INDEX32,
				D3DPOOL_DEFAULT,
				indexBuffer.GetAddressOf(),
				nullptr)))
			{
				indexCapacity = 0;
				return false;
			}
		}

		return true;
	}

	bool SdDx9Renderer::UploadBuffers(const SdRenderPacket& packet)
	{
		if (!EnsureBuffers(static_cast<SdUInt32>(packet.vertices.size()), static_cast<SdUInt32>(packet.indices.size())))
			return false;

		convertedVertices.resize(packet.vertices.size());
		for (SdSize index = 0; index < packet.vertices.size(); ++index)
		{
			const SdVertex& source = packet.vertices[index];
			Dx9Vertex& destination = convertedVertices[index];
			destination.x = source.position.x - 0.5f;
			destination.y = source.position.y - 0.5f;
			destination.z = 0.0f;
			destination.rhw = 1.0f;
			destination.color = ConvertColor(source.color);
			destination.u = source.uv.x;
			destination.v = source.uv.y;
		}

		void* mappedVertices = nullptr;
		const UINT vertexBytes = static_cast<UINT>(convertedVertices.size() * sizeof(Dx9Vertex));
		if (FAILED(vertexBuffer->Lock(0, vertexBytes, &mappedVertices, D3DLOCK_DISCARD)))
			return false;
		std::memcpy(mappedVertices, convertedVertices.data(), vertexBytes);
		vertexBuffer->Unlock();

		void* mappedIndices = nullptr;
		const UINT indexBytes = static_cast<UINT>(packet.indices.size_bytes());
		if (FAILED(indexBuffer->Lock(0, indexBytes, &mappedIndices, D3DLOCK_DISCARD)))
			return false;
		std::memcpy(mappedIndices, packet.indices.data(), indexBytes);
		indexBuffer->Unlock();
		return true;
	}

	SdDx9Renderer::TextureEntry& SdDx9Renderer::EnsureTextureEntry(SdTextureHandle texture)
	{
		if (textures.size() <= texture.index)
			textures.resize(texture.index + 1);

		TextureEntry& entry = textures[texture.index];
		if (!entry.occupied || entry.generation != texture.generation)
		{
			entry = {};
			entry.generation = texture.generation;
			entry.occupied = true;
		}
		return entry;
	}

	SdDx9Renderer::TextureEntry* SdDx9Renderer::TryGetTexture(SdTextureHandle texture) noexcept
	{
		if (!texture.IsValid() || texture.index >= textures.size())
			return nullptr;
		TextureEntry& entry = textures[texture.index];
		if (!entry.occupied || entry.generation != texture.generation || !entry.texture)
			return nullptr;
		return &entry;
	}

	bool SdDx9Renderer::UploadTexture(const SdUploadRequest& request)
	{
		if (!device || !request.texture.IsValid() || request.width == 0 || request.height == 0)
			return false;
		const SdSize expectedSize = static_cast<SdSize>(request.width) * request.height * sizeof(SdUInt32);
		if (request.pixels.size() < expectedSize)
			return false;

		TextureEntry& entry = EnsureTextureEntry(request.texture);
		if (!entry.texture || entry.width != request.width || entry.height != request.height)
		{
			entry.texture.Reset();
			if (FAILED(device->CreateTexture(
				request.width,
				request.height,
				1,
				0,
				D3DFMT_A8R8G8B8,
				D3DPOOL_MANAGED,
				entry.texture.GetAddressOf(),
				nullptr)))
			{
				return false;
			}
			entry.width = request.width;
			entry.height = request.height;
		}

		D3DLOCKED_RECT locked = {};
		if (FAILED(entry.texture->LockRect(0, &locked, nullptr, 0)))
			return false;

		const SdUInt32* sourcePixels = reinterpret_cast<const SdUInt32*>(request.pixels.data());
		for (SdUInt32 y = 0; y < request.height; ++y)
		{
			SdUInt32* destinationPixels = reinterpret_cast<SdUInt32*>(static_cast<std::byte*>(locked.pBits) + static_cast<SdSize>(y) * locked.Pitch);
			for (SdUInt32 x = 0; x < request.width; ++x)
				destinationPixels[x] = ConvertRgbaToA8R8G8B8(sourcePixels[static_cast<SdSize>(y) * request.width + x]);
		}
		entry.texture->UnlockRect(0);
		return true;
	}

	SdTextureHandle SdDx9Renderer::CreateTexture(SdUInt32 width, SdUInt32 height, const void* rgbaPixels)
	{
		if (!device || width == 0 || height == 0)
			return {};

		SdUInt32 index = 1;
		for (; index < textures.size(); ++index)
		{
			if (index == kReservedFontAtlasTextureIndex)
				continue;
			if (!textures[index].occupied)
				break;
		}
		if (index == kReservedFontAtlasTextureIndex)
			++index;
		if (textures.size() <= index)
			textures.resize(index + 1);

		TextureEntry& entry = textures[index];
		const SdUInt32 previousGeneration = entry.generation;
		entry = {};
		entry.generation = previousGeneration == 0 ? 1 : previousGeneration + 1;
		entry.occupied = true;

		if (FAILED(device->CreateTexture(
			width,
			height,
			1,
			0,
			D3DFMT_A8R8G8B8,
			D3DPOOL_MANAGED,
			entry.texture.GetAddressOf(),
			nullptr)))
		{
			entry = {};
			return {};
		}

		entry.width = width;
		entry.height = height;
		const SdTextureHandle handle(index, entry.generation);
		if (rgbaPixels)
		{
			SdUploadRequest upload = {};
			upload.texture = handle;
			upload.width = width;
			upload.height = height;
			upload.pixels.resize(static_cast<SdSize>(width) * height * sizeof(SdUInt32));
			std::memcpy(upload.pixels.data(), rgbaPixels, upload.pixels.size());
			if (!UploadTexture(upload))
			{
				DestroyTexture(handle);
				return {};
			}
		}
		return handle;
	}

	SdTextureHandle SdDx9Renderer::CreateTexture(const SdTextureDesc& desc)
	{
		if (desc.width == 0 || desc.height == 0)
			return {};
		return CreateTexture(desc.width, desc.height, desc.pixels.empty() ? nullptr : desc.pixels.data());
	}

	void SdDx9Renderer::DestroyTexture(SdTextureHandle texture)
	{
		if (!texture.IsValid() || texture.index >= textures.size())
			return;

		TextureEntry& entry = textures[texture.index];
		if (!entry.occupied || entry.generation != texture.generation)
			return;

		entry = {};
		entry.generation = texture.generation + 1;
	}

	void SdDx9Renderer::Render(const SdRendererFrameInfo& frameInfo, const SdRenderPacket& packet)
	{
		if (!device)
			return;

		for (const SdUploadRequest& upload : packet.resourceUpdates)
			UploadTexture(upload);

		const bool hasGeometry = !packet.vertices.empty() && !packet.indices.empty();
		if (!hasGeometry || !UploadBuffers(packet))
			return;

		Microsoft::WRL::ComPtr<IDirect3DStateBlock9> stateBlock = {};
		if (SUCCEEDED(device->CreateStateBlock(D3DSBT_ALL, stateBlock.GetAddressOf())) && stateBlock)
			stateBlock->Capture();

		BindPipeline(frameInfo.displaySize);
		device->SetStreamSource(0, vertexBuffer.Get(), 0, sizeof(Dx9Vertex));
		device->SetIndices(indexBuffer.Get());

		const Rhi::SdRectI fullScissor =
		{
			0,
			0,
			static_cast<SdInt32>(frameInfo.displaySize.x),
			static_cast<SdInt32>(frameInfo.displaySize.y)
		};
		std::vector<Rhi::SdRectI> clipStack = {};
		Rhi::SdRectI currentScissor = fullScissor;
		const auto applyScissor = [this, &currentScissor](const Rhi::SdRectI& scissor)
		{
			currentScissor = scissor;
			RECT nativeRect =
			{
				scissor.left,
				scissor.top,
				scissor.right,
				scissor.bottom
			};
			device->SetScissorRect(&nativeRect);
		};
		applyScissor(fullScissor);

		for (const SdRenderCommand& command : packet.commandBuffer.GetCommands())
		{
			if (command.kind == SdRenderCommandKind::DrawBatch)
			{
				const SdDrawBatchPayload& payload = packet.commandBuffer.ReadPayload<SdDrawBatchPayload>(command);
				if (payload.batchIndex >= packet.batches.size())
					continue;
				const SdRenderBatch& batch = packet.batches[payload.batchIndex];
				if (batch.indexCount < 3 || currentScissor.Width() == 0 || currentScissor.Height() == 0)
					continue;
				TextureEntry* texture = TryGetTexture(batch.texture);
				if (!texture)
					continue;

				device->SetTexture(0, texture->texture.Get());
				device->DrawIndexedPrimitive(
					D3DPT_TRIANGLELIST,
					0,
					batch.vertexOffset,
					batch.vertexCount,
					batch.indexOffset,
					batch.indexCount / 3);
			}
			else if (command.kind == SdRenderCommandKind::PushClipRect)
			{
				const SdPushClipPayload& payload = packet.commandBuffer.ReadPayload<SdPushClipPayload>(command);
				if (payload.kind != SdClipKind::Rect)
					continue;
				const Rhi::SdRectI scissor = payload.rect.Width() > 0.0f && payload.rect.Height() > 0.0f
					? MakeScissor(payload.rect, frameInfo.displaySize)
					: Rhi::SdRectI{};
				clipStack.push_back(scissor);
				applyScissor(scissor);
			}
			else if (command.kind == SdRenderCommandKind::PopClip)
			{
				if (!clipStack.empty())
					clipStack.pop_back();
				applyScissor(clipStack.empty() ? fullScissor : clipStack.back());
			}
			else if (command.kind == SdRenderCommandKind::BeginRenderLayer
				|| command.kind == SdRenderCommandKind::EndRenderLayer
				|| command.kind == SdRenderCommandKind::ApplyEffect)
			{
				continue;
			}
		}

		device->SetTexture(0, nullptr);
		if (stateBlock)
			stateBlock->Apply();
	}
}
