#pragma once

#include "Rhi/SdRenderGraph.hpp"

#include <array>

namespace Sodium::Rhi
{
	struct SdFullscreenPassDesc final
	{
		SdUtf8StringView name = {};
		SdPipelineHandle pipeline = {};
		SdResourceSetHandle resourceSet = {};
		SdRenderGraphTexture output = {};
		SdRectI viewport = {};
		SdRectI scissor = {};
		SdLoadOp loadOp = SdLoadOp::Load;
		SdColorLinear clearColor = {};
	};

	inline SdRenderGraphPassHandle AddFullscreenPass(SdRenderGraph& graph, const SdFullscreenPassDesc& desc)
	{
		SdRenderGraphPassHandle pass = graph.AddPass({ desc.name, SdRenderGraphPassType::Fullscreen });
		graph.WriteTexture(pass, desc.output);
		graph.SetPassCallback(pass, [&graph, desc](ISdCommandEncoder& encoder)
		{
			const SdTextureHandle target = graph.GetPhysicalTexture(desc.output);
			if (!target.IsValid())
				return;

			const SdRenderPassColorAttachment attachment = { target, desc.loadOp, SdStoreOp::Store, desc.clearColor };
			const std::array<SdRenderPassColorAttachment, 1> attachments = { attachment };
			SdRenderPassDesc renderPass = {};
			renderPass.colorAttachments = attachments;
			renderPass.renderArea = desc.viewport;
			renderPass.debugName = desc.name;

			encoder.BeginRenderPass(renderPass);
			encoder.SetPipeline(desc.pipeline);
			encoder.SetResourceSet(0, desc.resourceSet);
			encoder.SetViewport({
				static_cast<float>(desc.viewport.left),
				static_cast<float>(desc.viewport.top),
				static_cast<float>(desc.viewport.Width()),
				static_cast<float>(desc.viewport.Height()),
				0.0f,
				1.0f
			});
			encoder.SetScissorRect(desc.scissor.Width() > 0 && desc.scissor.Height() > 0 ? desc.scissor : desc.viewport);
			encoder.Draw(3, 1, 0, 0);
			encoder.EndRenderPass();
		});
		return pass;
	}
}
