#pragma once

#include "Render/SdDrawData.h"

#include <concepts>

namespace Sodium
{
	class SdInputSystem;

	class ISdPlatformBackend
	{
	public:
		virtual ~ISdPlatformBackend() = default;

		virtual bool IsInitialized() const noexcept = 0;

		// StartFrame transfers host/platform events into SdInputSystem.
		// It must not render, present, or mutate renderer state.
		virtual void StartFrame(SdInputSystem& input) = 0;
	};

	class ISdRendererBackend
	{
	public:
		virtual ~ISdRendererBackend() = default;

		virtual bool IsInitialized() const noexcept = 0;

		// Render submits one complete UI frame.
		//
		// Contract:
		// - Does not clear or present the host swap chain.
		// - Saves and restores any host graphics pipeline state it mutates.
		// - Keeps GPU buffers persistent and grows them only when required.
		// - Applies resource updates carried by the draw packet before drawing.
		// - Treats SdTextureHandle as backend-owned; handles become invalid
		//   after DestroyTexture or when their generation no longer matches.
		virtual void Render(const SdRendererFrameInfo& frameInfo, const SdDrawPacket& packet) = 0;

		// Creates a backend-owned texture handle. The caller owns the source
		// pixel memory; the backend owns GPU resource lifetime after creation.
		virtual SdTextureHandle CreateTexture(const SdTextureDesc& desc) = 0;

		// Destroys the backend resource for a texture handle. Stale handles
		// must be ignored by Render rather than dereferenced.
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
