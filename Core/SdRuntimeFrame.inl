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
		portalStack.clear();
	}

	inline SdUi::SdPortalFrame SdUi::CurrentPortalFrame() const noexcept
	{
		return portalStack.empty() ? SdPortalFrame{} : portalStack.back();
	}

	inline void SdUi::BeginPortal(SdPortalRoot root, SdWidgetId ownerWidgetId, SdWidgetId anchorWidgetId)
	{
		portalStack.push_back({ root, ownerWidgetId, anchorWidgetId });
	}

	inline void SdUi::EndPortal()
	{
		assert(!portalStack.empty());
		if (!portalStack.empty())
			portalStack.pop_back();
	}

	inline SdInstance::SdInstance()
		: renderList(&context.renderStats, &context.renderSharedData), uiObject(*this), ui(uiObject)
	{
		context.renderSharedData.builtInEffects.blur = context.effectRegistry.Register<SdBlurEffect>();
		context.renderSharedData.builtInEffects.backdropBlur = context.effectRegistry.Register<SdBackdropBlurEffect>();
		context.renderSharedData.builtInEffects.dropShadow = context.effectRegistry.Register<SdDropShadowEffect>();
		context.renderSharedData.builtInEffects.innerShadow = context.effectRegistry.Register<SdInnerShadowEffect>();
		context.renderSharedData.builtInEffects.mask = context.effectRegistry.Register<SdMaskEffect>();
	}

	inline bool SdInstance::Initialize(ISdPlatformBackend* platform, ISdRendererBackend* renderer, ISdFontBackend* fontBackend) noexcept
	{
		SetPlatformBackend(platform);
		SetRendererBackend(renderer);
		SetFontBackend(fontBackend);
		const bool initialized = context.platform && context.platform->IsInitialized()
			&& context.renderer && context.renderer->IsInitialized()
			&& context.fontBackend && context.fontBackend->IsInitialized();
		if (initialized)
		{
			if (Rhi::ISdGpuDevice* device = context.renderer->GetRhiDeviceInterface())
				return context.effectRegistry.Initialize(*device);
		}
		return initialized;
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

	inline void SdInstance::FinishInputAndBeginUiFrame()
	{
		context.frame.displaySize = GetInput().display.size;
		context.input.FinalizeFrame();
		context.interactionSystem.Update(context.layerSystem, context.input.GetSnapshot(), context.frame.frameIndex);
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
		context.effectRegistry.Shutdown();
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
