#pragma once

namespace Sodium
{
	inline void SdInstance::RefreshDiagnostics()
	{
		SdFrameDiagnostics& diagnostics = context.frame.diagnostics;
		const auto& widgets = context.stateStorage.GetWidgetRecords();
		diagnostics.submittedWidgetCount = static_cast<SdUInt32>(context.frameOrder.size());
		diagnostics.liveWidgetCount = 0;
		diagnostics.enteringWidgetCount = 0;
		diagnostics.leavingWidgetCount = 0;
		diagnostics.deadWidgetCount = 0;

		for (const auto& [id, record] : widgets)
		{
			(void)id;
			switch (record.state.lifePhase)
			{
			case SdWidgetLifePhase::Entering:
				++diagnostics.enteringWidgetCount;
				++diagnostics.liveWidgetCount;
				break;
			case SdWidgetLifePhase::Alive:
				++diagnostics.liveWidgetCount;
				break;
			case SdWidgetLifePhase::Leaving:
				++diagnostics.leavingWidgetCount;
				++diagnostics.liveWidgetCount;
				break;
			case SdWidgetLifePhase::Dead:
				++diagnostics.deadWidgetCount;
				break;
			default:
				break;
			}
		}

		const SdDrawData& drawData = renderList.GetDrawData();
		diagnostics.layoutNodeCount = static_cast<SdUInt32>(context.layoutSystem.GetNodes().size());
		diagnostics.hitTestRecordCount = static_cast<SdUInt32>(context.layerSystem.GetHitTestRecords().size());
		diagnostics.layerDrawChannelCount = static_cast<SdUInt32>(context.layerSystem.GetDrawChannels().size());
		diagnostics.activeAnimationChannelCount = static_cast<SdUInt32>(
			context.animationSystem.GetActiveChannelCount()
			+ context.stateStorage.CountActiveTypedStyleAnimationChannels());
		diagnostics.activeStyleNodeAnimationChannelCount = context.styleAnimationChannels.CountActive();
		diagnostics.activeLayoutTransitionCount = context.styleAnimationChannels.CountActiveLayoutTransitions();
		diagnostics.drawCommandCount = static_cast<SdUInt32>(drawData.commands.size());
		diagnostics.drawVertexCount = static_cast<SdUInt32>(drawData.vertices.size());
		diagnostics.drawIndexCount = static_cast<SdUInt32>(drawData.indices.size());
		diagnostics.drawBatchCount = static_cast<SdUInt32>(drawData.batches.size());
		diagnostics.resourceUploadCount = static_cast<SdUInt32>(drawData.uploads.size());
		diagnostics.createdWidgetCount = context.stateStorage.GetStats().createdWidgetCount;
		diagnostics.reusedWidgetCount = context.stateStorage.GetStats().reusedWidgetCount;
		diagnostics.modelCount = context.stateStorage.GetStats().modelCount;
		diagnostics.styleNodeCount = context.stateStorage.CountLiveStyleNodes();
		diagnostics.liveObjectCount = context.stateStorage.GetStats().liveObjectCount;
	}
}
