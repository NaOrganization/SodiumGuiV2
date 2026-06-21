#pragma once

namespace Sodium
{
	inline SdWidgetRecord& SdInstance::GetOrCreateWidgetRecord(SdWidgetId id)
	{
		SdWidgetRecord& record = context.stateStorage.GetOrCreateWidgetRecord(id);
		if (record.state.id == 0)
		{
			record.state.id = id;
			context.stateStorage.EnsureRootStyleNode(record, id);
			record.state.lifePhase = SdWidgetLifePhase::Entering;
			record.state.layoutWeight = 0.0f;
			record.state.opacity = 0.0f;
			record.animation.layoutWeight = context.animationSystem.EnsureChannel(id, SdAnimationChannelKind::LayoutWeight, 0.0f);
			record.animation.opacity = context.animationSystem.EnsureChannel(id, SdAnimationChannelKind::Opacity, 0.0f);
			record.animation.rectX = context.animationSystem.EnsureChannel(id, SdAnimationChannelKind::RectPosition, 0.0f);
			record.animation.rectY = context.animationSystem.EnsureChannel(id, SdAnimationChannelKind::RectPosition, 0.0f);
			record.animation.rectWidth = context.animationSystem.EnsureChannel(id, SdAnimationChannelKind::RectSize, 0.0f);
			record.animation.rectHeight = context.animationSystem.EnsureChannel(id, SdAnimationChannelKind::RectSize, 0.0f);
			record.animation.styleColorR = context.animationSystem.EnsureChannel(id, SdAnimationChannelKind::StyleColor, 0.0f);
			record.animation.styleColorG = context.animationSystem.EnsureChannel(id, SdAnimationChannelKind::StyleColor, 0.0f);
			record.animation.styleColorB = context.animationSystem.EnsureChannel(id, SdAnimationChannelKind::StyleColor, 0.0f);
			record.animation.styleColorA = context.animationSystem.EnsureChannel(id, SdAnimationChannelKind::StyleColor, 1.0f);
			record.animation.styleBackgroundR = context.animationSystem.EnsureChannel(id, SdAnimationChannelKind::StyleColor, 0.0f);
			record.animation.styleBackgroundG = context.animationSystem.EnsureChannel(id, SdAnimationChannelKind::StyleColor, 0.0f);
			record.animation.styleBackgroundB = context.animationSystem.EnsureChannel(id, SdAnimationChannelKind::StyleColor, 0.0f);
			record.animation.styleBackgroundA = context.animationSystem.EnsureChannel(id, SdAnimationChannelKind::StyleColor, 1.0f);
			record.animation.styleBorderR = context.animationSystem.EnsureChannel(id, SdAnimationChannelKind::StyleColor, 0.0f);
			record.animation.styleBorderG = context.animationSystem.EnsureChannel(id, SdAnimationChannelKind::StyleColor, 0.0f);
			record.animation.styleBorderB = context.animationSystem.EnsureChannel(id, SdAnimationChannelKind::StyleColor, 0.0f);
			record.animation.styleBorderA = context.animationSystem.EnsureChannel(id, SdAnimationChannelKind::StyleColor, 1.0f);
			record.animation.scrollOffset = context.animationSystem.EnsureChannel(id, SdAnimationChannelKind::ScrollOffset, 0.0f);
		}
		return record;
	}

	inline void SdInstance::MarkSubmitted(SdWidgetRecord& record, SdWidgetId id, SdWidgetId parentId, SdResolvedKey resolvedKey, SdUtf8StringView debugKey)
	{
		record.parentId = parentId;
		record.order = context.nextOrder++;
		record.resolvedKey = resolvedKey;
		record.debugKey.assign(debugKey.data(), debugKey.size());
		record.state.id = id;
		context.stateStorage.EnsureRootStyleNode(record, id);
		record.state.submittedThisFrame = true;
		record.state.lastSubmittedFrame = context.frame.frameIndex;
		record.state.inputEnabled = true;
		record.state.targetTypeId = SdWidgetTargetIds::Default;
		record.state.manualLayout = false;
		record.state.arrangeChildren = false;
		record.state.clipChildren = false;
		record.state.manualRect = {};
		record.state.childContentRect = {};
		record.state.computedClipRect = {};
		record.state.childPadding = {};
		record.state.childSpacing = 0.0f;
		if (record.state.lifePhase == SdWidgetLifePhase::Leaving || record.state.lifePhase == SdWidgetLifePhase::Dead)
			record.state.lifePhase = SdWidgetLifePhase::Entering;
		context.stateStorage.RegisterResolvedKey(resolvedKey, id);
		UpdateWidgetAnimation(record);
		context.frameOrder.push_back(id);
	}

	inline void SdInstance::FinishWidgetFrame()
	{
		EndDeclarationStage();
		RunLifecycleAnimationStage();
		RunLayoutAndPaintStage();
		RunSweepStage();
		RefreshDiagnostics();

		for (auto& [id, record] : context.stateStorage.GetWidgetRecords())
			record.state.submittedThisFrame = false;
	}

	inline void SdInstance::EndDeclarationStage()
	{
		for (auto& [id, record] : context.stateStorage.GetWidgetRecords())
		{
			(void)id;
			if (!record.state.submittedThisFrame && record.state.lifePhase != SdWidgetLifePhase::Dead)
			{
				record.state.lifePhase = SdWidgetLifePhase::Leaving;
				record.state.inputEnabled = false;
				++context.stateStorage.GetStats().leavingWidgetCount;
			}
			if (record.state.lifePhase == SdWidgetLifePhase::Leaving)
				UpdateWidgetAnimation(record);
		}
	}

	inline void SdInstance::RunLifecycleAnimationStage()
	{
		context.animationSystem.Update(context.frame.deltaTime, true, true, false, false, false, false);
		for (auto& [id, record] : context.stateStorage.GetWidgetRecords())
		{
			(void)id;
			record.state.layoutWeight = context.animationSystem.GetValue(record.animation.layoutWeight, record.state.layoutWeight);
			record.state.opacity = context.animationSystem.GetValue(record.animation.opacity, record.state.opacity);
			record.state.animationActive = context.animationSystem.GetChannel(record.animation.layoutWeight).active
				|| context.animationSystem.GetChannel(record.animation.opacity).active;

			const bool leaving = record.state.lifePhase == SdWidgetLifePhase::Leaving;
			if (!leaving && record.state.layoutWeight >= 1.0f && record.state.opacity >= 1.0f)
				record.state.lifePhase = SdWidgetLifePhase::Alive;
			if (leaving && record.state.layoutWeight <= 0.0f && record.state.opacity <= 0.0f)
				record.state.lifePhase = SdWidgetLifePhase::Dead;
		}
	}

	inline void SdInstance::RunLayoutAndPaintStage()
	{
		SolveLayoutAndPaint();
	}

	inline void SdInstance::RunSweepStage()
	{
		context.frame.diagnostics.removedWidgetCount = RemoveDeadWidgets();
	}

	inline SdTransition SdInstance::GetDefaultTransition() const noexcept
	{
		const float seconds = std::max(0.001f, context.styleSystem.GetTheme().GetMetricVariable(SdThemeVariableLiteral("duration.fast")));
		return {
			std::chrono::duration_cast<SdDuration>(std::chrono::duration<float>(seconds)),
			SdAnimationEasing::OutCubic
		};
	}

	inline void SdInstance::UpdateWidgetAnimation(SdWidgetRecord& record)
	{
		const bool leaving = record.state.lifePhase == SdWidgetLifePhase::Leaving;
		const float target = leaving ? 0.0f : 1.0f;
		const SdTransition transition = GetDefaultTransition();
		context.animationSystem.SetTarget(record.animation.layoutWeight, target, transition);
		context.animationSystem.SetTarget(record.animation.opacity, target, transition);
		record.state.animationActive = record.state.layoutWeight != target || record.state.opacity != target;
	}
	inline SdUInt32 SdInstance::RemoveDeadWidgets()
	{
		return context.stateStorage.RemoveDeadWidgets([this](SdWidgetId widgetId)
		{
			if (const SdWidgetRecord* record = context.stateStorage.FindWidgetRecord(widgetId))
			{
				context.styleAnimationChannels.RemoveStyleNode(record->rootStyleNodeId);
				for (SdStyleNodeId partStyleNodeId : record->partStyleNodeIds)
					context.styleAnimationChannels.RemoveStyleNode(partStyleNodeId);
			}
			context.animationSystem.RemoveWidget(widgetId);
		});
	}
}
