#pragma once

#include "Render/SdRenderData.h"
#include "Render/SdRenderStats.h"
#include "Render/SdRenderList.h"
#include "Core/SdBackendContracts.h"

namespace Sodium
{
	class SdRenderSystem final
	{
	public:
		bool Render(ISdRendererBackend* renderer, const SdRendererFrameInfo& frameInfo, const SdRenderPacket& packet)
		{
			if (!renderer)
				return false;
			renderer->Render(frameInfo, packet);
			return true;
		}
	};
}
