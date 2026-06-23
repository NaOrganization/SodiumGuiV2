#pragma once

namespace Sodium
{
	inline SdUi::SdUi(SdInstance& owner)
		: instance(owner)
	{
	}

	inline void SdUi::BeginDeclarationFrame()
	{
		idStack.BeginFrame();
	}

	inline SdInstance::SdInstance()
		: renderList(&context.renderStats, &context.renderSharedData), uiObject(*this), ui(uiObject)
	{
	}

	inline void SdInstance::BeginInputFrame()
	{
		++context.frame.frameIndex;
		const SdTimePoint now = std::chrono::time_point_cast<SdDuration>(std::chrono::steady_clock::now());
		if (lastFrameTime.time_since_epoch().count() != 0)
			context.frame.deltaTime = now - lastFrameTime;
		lastFrameTime = now;
		context.input.BeginFrame(context.frame.frameIndex);
	}

	inline void SdInstance::FinishInputAndBeginUiFrame(SdVec2 newDisplaySize)
	{
		if (newDisplaySize.x > 0.0f && newDisplaySize.y > 0.0f)
			context.frame.displaySize = newDisplaySize;
		context.input.FinalizeFrame();
		context.interactionSystem.Update(context.layerSystem, context.input.GetSnapshot());
		renderList.SetSharedData(&context.renderSharedData);
		renderList.SetStats(&context.renderStats);
		renderList.Reset();
		context.layerSystem.BeginFrame();
		context.frameOrder.clear();
		context.nextOrder = 0;
		context.stateStorage.BeginFrame();
		context.presentationChannels.ResetFrameStats();
		context.frame.diagnostics.ResetFrameTransient();
		uiObject.BeginDeclarationFrame();
	}

	inline void SdInstance::BeginFrame(SdVec2 newDisplaySize)
	{
		BeginInputFrame();
		FinishInputAndBeginUiFrame(newDisplaySize);
	}

	inline void SdInstance::EndFrame()
	{
		FinishWidgetFrame();
	}

	inline void SdInstance::Render()
	{
		context.renderSystem.Render(context.renderer, SdRendererFrameInfo{ context.frame.displaySize }, GetDrawPacket());
	}

	inline void SdInstance::Shutdown()
	{
		context.stateStorage.Clear();
		context.frameOrder.clear();
		renderList.Reset();
	}

	inline void SdInstance::SetPlatformBackend(ISdPlatformBackend* platform) noexcept
	{
		context.platform = platform;
	}

	inline void SdInstance::SetRendererBackend(ISdRendererBackend* renderer) noexcept
	{
		context.renderer = renderer;
	}

	inline void SdInstance::SetFontBackend(ISdFontBackend* fontBackend) noexcept
	{
		context.fontBackend = fontBackend;
		context.renderSharedData.fontBackend = fontBackend;
		if (fontBackend)
			fontBackend->ConfigureRenderSharedData(context.renderSharedData);
	}

	inline SdUInt32 SdInstance::AllocateActivationOrder() noexcept
	{
		++context.nextActivationOrder;
		if (context.nextActivationOrder == 0)
			context.nextActivationOrder = 1;
		return context.nextActivationOrder;
	}

	inline bool SdInstance::IsWidgetDescendantOf(SdWidgetId widgetId, SdWidgetId ancestorWidgetId) const noexcept
	{
		if (widgetId == 0 || ancestorWidgetId == 0)
			return false;

		SdWidgetId currentId = widgetId;
		for (SdUInt32 depth = 0; currentId != 0 && depth < 256; ++depth)
		{
			if (currentId == ancestorWidgetId)
				return true;

			const SdWidgetRecord* record = context.stateStorage.FindWidgetRecord(currentId);
			if (!record)
				return false;
			currentId = record->parentId;
		}
		return false;
	}
}
