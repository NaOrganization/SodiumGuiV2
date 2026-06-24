#pragma once

#include "Render/SdDrawData.h"
#include "Render/SdRenderStats.h"
#include "Render/SdDrawList.h"
#include "Core/SdBackendContracts.h"

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
