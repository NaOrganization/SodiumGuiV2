#pragma once

#include "SdCore.h"

namespace Sodium
{
	struct SdRenderStats final
	{
		SdUInt32 drawCallCount = 0;
		SdUInt32 commandCount = 0;
		SdUInt32 vertexCount = 0;
		SdUInt32 indexCount = 0;
		SdUInt32 uploadCount = 0;
		SdUInt32 textLayoutBuildCount = 0;
		SdUInt32 glyphRasterizedCount = 0;
		SdUInt32 atlasUploadCount = 0;
		SdUInt32 renderListGrowCount = 0;
		SdUInt32 aaScratchGrowCount = 0;
		SdUInt32 vertexBufferGrowCount = 0;
		SdUInt32 indexBufferGrowCount = 0;
		SdUInt32 maxVertexCapacity = 0;
		SdUInt32 maxIndexCapacity = 0;
		float submitCpuMilliseconds = 0.0f;
		float uploadCpuMilliseconds = 0.0f;
		float bufferUploadCpuMilliseconds = 0.0f;
		float drawSubmitCpuMilliseconds = 0.0f;

		void Reset()
		{
			drawCallCount = 0;
			commandCount = 0;
			vertexCount = 0;
			indexCount = 0;
			uploadCount = 0;
			textLayoutBuildCount = 0;
			glyphRasterizedCount = 0;
			atlasUploadCount = 0;
			renderListGrowCount = 0;
			aaScratchGrowCount = 0;
			vertexBufferGrowCount = 0;
			indexBufferGrowCount = 0;
			maxVertexCapacity = 0;
			maxIndexCapacity = 0;
			submitCpuMilliseconds = 0.0f;
			uploadCpuMilliseconds = 0.0f;
			bufferUploadCpuMilliseconds = 0.0f;
			drawSubmitCpuMilliseconds = 0.0f;
		}
	};
}
