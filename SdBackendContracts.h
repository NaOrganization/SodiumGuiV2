#pragma once

#include "SdDrawData.h"

#include <concepts>

namespace Sodium
{
	class SdInputSystem;

	class ISdPlatformBackend
	{
	public:
		virtual ~ISdPlatformBackend() = default;
		virtual void StartFrame(SdInputSystem& input) = 0;
	};

	class ISdRendererBackend
	{
	public:
		virtual ~ISdRendererBackend() = default;

		// Render submits one complete UI frame. The backend does not present,
		// must preserve host pipeline state, and should keep GPU buffers
		// persistent, growing them only when packet capacity requires it.
		virtual void Render(const SdRendererFrameInfo& frameInfo, const SdDrawPacket& packet) = 0;
		virtual SdTextureHandle CreateTexture(const SdTextureDesc& desc) = 0;
		virtual void DestroyTexture(SdTextureHandle texture) = 0;
	};

	template<class T>
	concept SdRendererBackend = requires(T renderer, SdRendererFrameInfo frameInfo, SdDrawPacket packet, SdTextureDesc textureDesc, SdTextureHandle texture)
	{
		{ renderer.Render(frameInfo, packet) };
		{ renderer.CreateTexture(textureDesc) } -> std::same_as<SdTextureHandle>;
		{ renderer.DestroyTexture(texture) };
	};
}
