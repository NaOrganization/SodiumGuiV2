#pragma once

#include "SdDrawData.h"

#include <concepts>

namespace Sodium
{
	template<class T>
	concept SdRendererBackend = requires(T renderer, SdRendererFrameInfo frameInfo, SdDrawPacket packet, SdTextureDesc textureDesc, SdTextureHandle texture)
	{
		{ renderer.BeginFrame(frameInfo) };
		{ renderer.Submit(packet) };
		{ renderer.EndFrame() };
		{ renderer.CreateTexture(textureDesc) } -> std::same_as<SdTextureHandle>;
		{ renderer.DestroyTexture(texture) };
	};
}
