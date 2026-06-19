#pragma once

#include "SdDrawData.h"
#include "SdRenderStats.h"
#include "SdDrawList.h"
#include "SdBackendContracts.h"

namespace Sodium
{
	class SdRenderSystem final
	{
	public:
		bool Render(ISdRendererBackend* renderer, const SdRendererFrameInfo& frameInfo, const SdDrawPacket& packet)
		{
			if (!renderer)
				return false;
			renderer->Render(frameInfo, packet);
			return true;
		}
	};
}
