#pragma once

namespace Sodium
{
	namespace Detail
	{
		inline bool TransitionsEqual(const SdTransition& left, const SdTransition& right) noexcept
		{
			return left.duration == right.duration
				&& left.easing == right.easing
				&& left.delay == right.delay
				&& left.behavior == right.behavior;
		}

		inline void MarkTypedStyleFieldImpact(SdWidgetRecord& record, SdStyleFieldImpact impact, bool animationActive) noexcept
		{
			if (impact == SdStyleFieldImpact::Layout)
				record.state.layoutDirty = true;
			if (animationActive)
				record.state.animationActive = true;
		}

		inline SdPropertyAnimationChannel& SetStylePropertyChannelTarget(
			SdStyleAnimationChannels& channels,
			SdStyleNodeId styleNodeId,
			SdPropertyId propertyId,
			SdStyleFieldImpact impact,
			SdStyleInterpolation interpolation,
			const SdStyleValue& currentValue,
			const SdStyleValue& targetValue,
			SdTransition transition,
			bool immediate,
			bool expensiveLayout = false)
		{
			SdPropertyAnimationChannel& channel = channels.Ensure(styleNodeId, propertyId);
			const SdTransition effectiveTransition = immediate ? SdTransition{} : transition;
			const bool discrete = transition.behavior == SdTransitionBehavior::AllowDiscrete
				&& interpolation == SdStyleInterpolation::None;
			const bool noop = !channel.active
				&& channel.impact == impact
				&& channel.interpolation == interpolation
				&& TransitionsEqual(channel.transition, effectiveTransition)
				&& channel.delay == effectiveTransition.delay
				&& StyleValuesEqual(channel.startValue, targetValue)
				&& StyleValuesEqual(channel.currentValue, targetValue)
				&& StyleValuesEqual(channel.targetValue, targetValue)
				&& channel.expensiveLayout == expensiveLayout
				&& channel.discrete == discrete;
			channels.RecordTargetSet(noop);
			if (noop)
				return channel;

			channel.impact = impact;
			channel.interpolation = interpolation;
			channel.transition = effectiveTransition;
			channel.delay = channel.transition.delay;
			channel.elapsed = {};
			channel.startValue = currentValue;
			channel.targetValue = targetValue;
			channel.currentValue = channel.startValue;
			channel.expensiveLayout = expensiveLayout;
			channel.discrete = discrete;
			channel.active = !immediate && !StyleValuesEqual(channel.startValue, channel.targetValue);
			if (immediate || !channel.active)
				channel.currentValue = channel.targetValue;
			return channel;
		}

		inline void SetStyleColorPropertyChannelTarget(
			SdStyleAnimationChannels& channels,
			SdStyleNodeId styleNodeId,
			SdPropertyId propertyId,
			SdColor currentValue,
			SdColor targetValue,
			SdTransition transition,
			bool immediate)
		{
			SetStylePropertyChannelTarget(
				channels,
				styleNodeId,
				propertyId,
				SdStyleFieldImpact::Paint,
				SdStyleInterpolation::Color,
				SdStyleValue::FromColor(currentValue),
				SdStyleValue::FromColor(targetValue),
				transition,
				immediate);
		}

		inline void SetStyleFloatPropertyChannelTarget(
			SdStyleAnimationChannels& channels,
			SdStyleNodeId styleNodeId,
			SdPropertyId propertyId,
			float currentValue,
			float targetValue,
			SdTransition transition,
			bool immediate,
			SdStyleFieldImpact impact = SdStyleFieldImpact::Composite)
		{
			SetStylePropertyChannelTarget(
				channels,
				styleNodeId,
				propertyId,
				impact,
				SdStyleInterpolation::Float,
				SdStyleValue::FromFloat(currentValue),
				SdStyleValue::FromFloat(targetValue),
				transition,
				immediate);
		}

		inline SdColor GetStyleColorPropertyChannelValue(
			SdStyleAnimationChannels& channels,
			SdStyleNodeId styleNodeId,
			SdPropertyId propertyId,
			SdColor fallback)
		{
			SdPropertyAnimationChannel* channel = channels.Find(styleNodeId, propertyId);
			return channel && channel->currentValue.kind == SdStyleValueKind::Color ? channel->currentValue.color : fallback;
		}

		inline float GetStyleFloatPropertyChannelValue(
			SdStyleAnimationChannels& channels,
			SdStyleNodeId styleNodeId,
			SdPropertyId propertyId,
			float fallback)
		{
			SdPropertyAnimationChannel* channel = channels.Find(styleNodeId, propertyId);
			return channel && channel->currentValue.kind == SdStyleValueKind::Float ? channel->currentValue.number : fallback;
		}
	}

	inline void SdInstance::SetWidgetStyleIdentity(SdWidgetRecord& record, SdSpan<const SdStyleClassId> styleClasses, SdStyleScopeId styleScope)
	{
		bool sameIdentity = record.styleScope == styleScope && record.styleClasses.size() == styleClasses.size();
		if (sameIdentity)
		{
			for (SdSize index = 0; index < styleClasses.size(); ++index)
			{
				if (record.styleClasses[index] != styleClasses[index])
				{
					sameIdentity = false;
					break;
				}
			}
		}
		if (sameIdentity)
			return;

		record.styleClasses.assign(styleClasses.begin(), styleClasses.end());
		record.styleScope = styleScope;
		++record.styleIdentityRevision;
		if (record.styleIdentityRevision == 0)
			record.styleIdentityRevision = 1;
		record.state.styleDirty = true;
	}

	inline void SdInstance::ResolveWidgetStyle(SdWidgetRecord& record, SdStyleInteractionState interactionState, SdRootLayer rootLayer)
	{
		SdFrameDiagnostics& diagnostics = context.frame.diagnostics;
		++diagnostics.styleResolveCount;
		const SdWidgetRootStyle* inlineRootStyle = context.stateStorage.FindInlineStyle<SdWidgetRootStyle>(record);
		const SdUInt64 inlineRootStyleRevision = context.stateStorage.GetInlineStyleRevision<SdWidgetRootStyle>(record);
		if (!record.state.styleDirty
			&& record.styleCache.valid
			&& record.styleCache.targetTypeId == record.state.targetTypeId
			&& record.styleCache.interactionState == interactionState
			&& record.styleCache.rootLayer == rootLayer
			&& record.styleCache.styleIdentityRevision == record.styleIdentityRevision
			&& record.styleCache.inlineStyleRevision == inlineRootStyleRevision
			&& record.styleCache.styleRevision == context.styling.GetRevision())
		{
			++diagnostics.styleResolveCacheHitCount;
			ApplyWidgetStyleAnimation(record);
			if (record.styleCallback)
				record.styleCallback(*this, record, interactionState, rootLayer);
			return;
		}

		++diagnostics.styleResolveCacheMissCount;
		const bool firstStyle = !record.styleCache.valid;
		const SdStyleResolveResult rootResult = context.styling.ResolveRootNode(
			record.state.targetTypeId,
			interactionState,
			rootLayer,
			record.styleClasses,
			record.styleScope,
			inlineRootStyle);
		SdStyleNode& rootNode = context.stateStorage.EnsureRootStyleNode(record, record.state.id);
		SdWidgetRootStyle resolvedStyle = {};
		static_cast<SdBoxStyle&>(resolvedStyle) = rootResult.resolvedStyle;
		record.styleCache.resolvedStyle = resolvedStyle;
		record.styleCache.presentationStyle = resolvedStyle;
		record.styleCache.rootStyleNodeId = rootNode.styleNodeId;
		rootNode.widgetId = record.state.id;
		rootNode.kind = SdStyleNodeKind::Root;
		rootNode.part = SdStylePart::Root();
		rootNode.scopeId = record.styleScope;
		rootNode.pseudoState = SdPseudoState::FromInteraction(interactionState);
		SdStyleResolver::WriteNode(rootNode, rootResult);
		record.rootStyleNode = rootNode;
		++diagnostics.styleResolvedNodeCount;
		for (SdStyleNodeId partNodeId : record.partStyleNodeIds)
		{
			SdStyleNode* partNode = context.stateStorage.FindStyleNodeById(partNodeId);
			if (!partNode)
				continue;
			const SdStyleResolveResult partResult = context.styling.ResolvePartNode(
				record.state.targetTypeId,
				partNode->part,
				resolvedStyle,
				interactionState,
				rootLayer,
				record.styleClasses,
				record.styleScope);
			partNode->widgetId = record.state.id;
			partNode->parentStyleNodeId = rootNode.styleNodeId;
			partNode->scopeId = record.styleScope;
			partNode->pseudoState = SdPseudoState::FromInteraction(interactionState);
			SdStyleResolver::WriteNode(*partNode, partResult);
			SetBoxStyleAnimationTarget(record, *partNode, partNode->resolvedStyle, interactionState, rootLayer, firstStyle);
			ApplyBoxStyleAnimation(*partNode);
			++diagnostics.styleResolvedNodeCount;
		}
		record.styleCache.targetTypeId = record.state.targetTypeId;
		record.styleCache.interactionState = interactionState;
		record.styleCache.rootLayer = rootLayer;
		record.styleCache.styleIdentityRevision = record.styleIdentityRevision;
		record.styleCache.inlineStyleRevision = inlineRootStyleRevision;
		record.styleCache.styleRevision = context.styling.GetRevision();
		record.styleCache.valid = true;
		record.state.styleDirty = false;
		SetWidgetStyleAnimationTarget(record, record.styleCache.resolvedStyle, interactionState, rootLayer, firstStyle);
		ApplyWidgetStyleAnimation(record);
		if (record.styleCallback)
			record.styleCallback(*this, record, interactionState, rootLayer);
	}

	inline void SdInstance::SetBoxStyleAnimationTarget(
		SdWidgetRecord& record,
		SdStyleNode& node,
		const SdBoxStyle& style,
		SdStyleInteractionState interactionState,
		SdRootLayer rootLayer,
		bool immediate)
	{
		const SdPropertyId colorPropertyId = Detail::SdStylePropertyId(&SdBoxStyle::color);
		const SdPropertyId backgroundPropertyId = Detail::SdStylePropertyId(&SdBoxStyle::backgroundColor);
		const SdPropertyId borderPropertyId = Detail::SdStylePropertyId(&SdBoxStyle::border);
		const SdPropertyId opacityPropertyId = Detail::SdStylePropertyId(&SdBoxStyle::opacity);

		const SdTransition defaultTransition = GetDefaultTransition();
		const auto resolveTransition = [this, &record, &node, interactionState, rootLayer, defaultTransition](SdPropertyId propertyId)
		{
			SdTransition transition = defaultTransition;
			if (node.kind == SdStyleNodeKind::Part)
			{
				context.styling.TryResolvePartTransition(
					record.state.targetTypeId,
					node.part,
					propertyId,
					interactionState,
					rootLayer,
					record.styleClasses,
					record.styleScope,
					transition);
			}
			else
			{
				context.styling.TryResolveRootTransition(
					record.state.targetTypeId,
					propertyId,
					interactionState,
					rootLayer,
					record.styleClasses,
					record.styleScope,
					transition);
			}
			return transition;
		};

		Detail::SetStyleColorPropertyChannelTarget(
			context.presentationChannels,
			node.styleNodeId,
			colorPropertyId,
			Detail::GetStyleColorPropertyChannelValue(
				context.presentationChannels,
				node.styleNodeId,
				colorPropertyId,
				style.color),
			style.color,
			resolveTransition(colorPropertyId),
			immediate);
		Detail::SetStyleColorPropertyChannelTarget(
			context.presentationChannels,
			node.styleNodeId,
			backgroundPropertyId,
			Detail::GetStyleColorPropertyChannelValue(
				context.presentationChannels,
				node.styleNodeId,
				backgroundPropertyId,
				style.backgroundColor),
			style.backgroundColor,
			resolveTransition(backgroundPropertyId),
			immediate);
		Detail::SetStyleColorPropertyChannelTarget(
			context.presentationChannels,
			node.styleNodeId,
			borderPropertyId,
			Detail::GetStyleColorPropertyChannelValue(
				context.presentationChannels,
				node.styleNodeId,
				borderPropertyId,
				style.border.left.color),
			style.border.left.color,
			resolveTransition(borderPropertyId),
			immediate);
		Detail::SetStyleFloatPropertyChannelTarget(
			context.presentationChannels,
			node.styleNodeId,
			opacityPropertyId,
			Detail::GetStyleFloatPropertyChannelValue(
				context.presentationChannels,
				node.styleNodeId,
				opacityPropertyId,
				style.opacity),
			style.opacity,
			resolveTransition(opacityPropertyId),
			immediate);
	}

	inline void SdInstance::ApplyBoxStyleAnimation(SdStyleNode& node)
	{
		SdBoxStyle presentationStyle = node.resolvedStyle;
		SdPropertyAnimationChannel* colorChannel = context.presentationChannels.Find(
			node.styleNodeId,
			Detail::SdStylePropertyId(&SdBoxStyle::color));
		if (colorChannel && colorChannel->currentValue.kind == SdStyleValueKind::Color)
			presentationStyle.color = colorChannel->currentValue.color;
		SdPropertyAnimationChannel* backgroundChannel = context.presentationChannels.Find(
			node.styleNodeId,
			Detail::SdStylePropertyId(&SdBoxStyle::backgroundColor));
		if (backgroundChannel && backgroundChannel->currentValue.kind == SdStyleValueKind::Color)
			presentationStyle.backgroundColor = backgroundChannel->currentValue.color;
		SdPropertyAnimationChannel* borderChannel = context.presentationChannels.Find(
			node.styleNodeId,
			Detail::SdStylePropertyId(&SdBoxStyle::border));
		if (borderChannel && borderChannel->currentValue.kind == SdStyleValueKind::Color)
			presentationStyle.border = SdBorder::All(
				node.resolvedStyle.border.left.width,
				borderChannel->currentValue.color);
		SdPropertyAnimationChannel* opacityChannel = context.presentationChannels.Find(
			node.styleNodeId,
			Detail::SdStylePropertyId(&SdBoxStyle::opacity));
		if (opacityChannel && opacityChannel->currentValue.kind == SdStyleValueKind::Float)
			presentationStyle.opacity = opacityChannel->currentValue.number;

		node.presentationStyle = presentationStyle;
	}

	inline void SdInstance::SetWidgetStyleAnimationTarget(
		SdWidgetRecord& record,
		const SdWidgetRootStyle& style,
		SdStyleInteractionState interactionState,
		SdRootLayer rootLayer,
		bool immediate)
	{
		if (SdStyleNode* rootNode = context.stateStorage.FindStyleNodeById(record.rootStyleNodeId))
			SetBoxStyleAnimationTarget(record, *rootNode, style, interactionState, rootLayer, immediate);
	}

	inline void SdInstance::ApplyWidgetStyleAnimation(SdWidgetRecord& record)
	{
		++context.frame.diagnostics.applyStyleAnimationCount;
		if (SdStyleNode* rootNode = context.stateStorage.FindStyleNodeById(record.rootStyleNodeId))
		{
			ApplyBoxStyleAnimation(*rootNode);
			static_cast<SdBoxStyle&>(record.styleCache.presentationStyle) = rootNode->presentationStyle;
			record.rootStyleNode = *rootNode;
		}
		for (SdStyleNodeId partNodeId : record.partStyleNodeIds)
		{
			if (SdStyleNode* partNode = context.stateStorage.FindStyleNodeById(partNodeId))
				ApplyBoxStyleAnimation(*partNode);
		}
		record.state.animationActive = record.state.animationActive
			|| context.presentationChannels.HasActiveAny(record.rootStyleNodeId, record.partStyleNodeIds);
	}

	template<class TWidget>
	void SdInstance::ResolveTypedWidgetStyle(SdWidgetRecord& record, SdStyleInteractionState interactionState, SdRootLayer rootLayer)
	{
		if constexpr (requires { typename TWidget::Style; })
		{
			using Style = typename TWidget::Style;
			SdTypedStyleRecord& styleRecord = context.stateStorage.GetOrCreateTypedStyleRecord<Style>(record);
			if (styleRecord.valid
				&& styleRecord.targetTypeId == record.state.targetTypeId
				&& styleRecord.interactionState == interactionState
				&& styleRecord.rootLayer == rootLayer
				&& styleRecord.styleIdentityRevision == record.styleIdentityRevision
				&& styleRecord.resolvedInlineStyleRevision == styleRecord.inlineStyleRevision
				&& styleRecord.styleRevision == context.styling.GetRevision())
			{
				return;
			}

			const Style* inlineStyle = context.stateStorage.FindInlineStyle<Style>(styleRecord);
			const Style resolvedStyleValue = context.styling.ResolveTypedStyle<TWidget>(
				interactionState,
				rootLayer,
				record.styleClasses,
				record.styleScope,
				inlineStyle);
			const bool firstStyle = !styleRecord.valid;
			Style& resolvedStyle = context.stateStorage.GetOrCreateResolvedStyle(styleRecord, resolvedStyleValue);
			Style& presentationStyle = context.stateStorage.GetOrCreatePresentationStyle(styleRecord, resolvedStyleValue);
			if (firstStyle)
			{
				resolvedStyle = resolvedStyleValue;
				presentationStyle = resolvedStyleValue;
				styleRecord.targetTypeId = record.state.targetTypeId;
				styleRecord.interactionState = interactionState;
				styleRecord.rootLayer = rootLayer;
				styleRecord.styleIdentityRevision = record.styleIdentityRevision;
				styleRecord.resolvedInlineStyleRevision = styleRecord.inlineStyleRevision;
				styleRecord.styleRevision = context.styling.GetRevision();
				styleRecord.valid = true;
				return;
			}

			const Style oldResolvedStyle = resolvedStyle;
			resolvedStyle = resolvedStyleValue;
			const auto& contract = context.styling.GetContract<TWidget>();
			if (contract.GetFields().empty())
			{
				presentationStyle = resolvedStyleValue;
			}
			else
			{
				for (const SdStyleFieldDescriptor& field : contract.GetFields())
				{
					if (!field.equals || !field.copy)
						continue;
					if (field.equals(field, &oldResolvedStyle, &resolvedStyle))
						continue;

					SdTransition transition = {};
					const bool hasTransition = context.styling.TryResolveTransition<TWidget>(
						field.fieldId,
						interactionState,
						rootLayer,
						record.styleClasses,
						record.styleScope,
						transition);
					const bool layoutTransitionAllowed = field.impact != SdStyleFieldImpact::Layout || field.expensiveTransition;
					const bool discreteTransitionAllowed = transition.behavior == SdTransitionBehavior::AllowDiscrete
						&& field.interpolation == SdStyleInterpolation::None;
					const bool canTransition = hasTransition
						&& layoutTransitionAllowed
						&& (field.interpolation != SdStyleInterpolation::None || discreteTransitionAllowed)
						&& field.readValue
						&& field.writeValue;

					if (canTransition)
					{
						const SdStyleValue startValue = field.readValue(field, &presentationStyle);
						const SdStyleValue targetValue = field.readValue(field, &resolvedStyle);
						SdPropertyAnimationChannel& propertyChannel = Detail::SetStylePropertyChannelTarget(
							context.presentationChannels,
							record.rootStyleNodeId,
							field.fieldId,
							field.impact,
							field.interpolation,
							startValue,
							targetValue,
							transition,
							false,
							field.expensiveTransition);
						if (!propertyChannel.active)
							field.writeValue(field, &presentationStyle, propertyChannel.currentValue, context.styling.GetDesignTokenSet());
						Detail::MarkTypedStyleFieldImpact(record, field.impact, propertyChannel.active);
					}
					else
					{
						if (field.readValue)
						{
							SdPropertyAnimationChannel& propertyChannel = context.presentationChannels.Ensure(record.rootStyleNodeId, field.fieldId);
							propertyChannel.active = false;
							propertyChannel.currentValue = field.readValue(field, &resolvedStyle);
							propertyChannel.targetValue = propertyChannel.currentValue;
						}
						field.copy(field, &presentationStyle, &resolvedStyle);
						Detail::MarkTypedStyleFieldImpact(record, field.impact, false);
					}
				}
			}
			styleRecord.targetTypeId = record.state.targetTypeId;
			styleRecord.interactionState = interactionState;
			styleRecord.rootLayer = rootLayer;
			styleRecord.styleIdentityRevision = record.styleIdentityRevision;
			styleRecord.resolvedInlineStyleRevision = styleRecord.inlineStyleRevision;
			styleRecord.styleRevision = context.styling.GetRevision();
			styleRecord.valid = true;
		}
	}

	template<class TWidget>
	void SdInstance::AdvanceTypedWidgetStyleAnimations(SdWidgetRecord& record, SdDuration deltaTime)
	{
		if constexpr (requires { typename TWidget::Style; })
		{
			using Style = typename TWidget::Style;
			SdTypedStyleRecord* styleRecord = context.stateStorage.FindTypedStyleRecord<Style>(record);
			if (!styleRecord)
				return;

			Style* presentationStyle = context.stateStorage.FindPresentationStyle<Style>(record);
			if (!presentationStyle)
				return;

			const auto& contract = context.styling.GetContract<TWidget>();
			(void)deltaTime;
			for (const SdStyleFieldDescriptor& field : contract.GetFields())
			{
				if (!field.writeValue)
					continue;
				SdPropertyAnimationChannel* propertyChannel = context.presentationChannels.Find(record.rootStyleNodeId, field.fieldId);
				if (!propertyChannel)
					continue;

				field.writeValue(field, presentationStyle, propertyChannel->currentValue, context.styling.GetDesignTokenSet());
				const bool active = propertyChannel->active
					&& !Detail::StyleValuesEqual(propertyChannel->currentValue, propertyChannel->targetValue);
				if (!active)
				{
					field.writeValue(field, presentationStyle, propertyChannel->targetValue, context.styling.GetDesignTokenSet());
					propertyChannel->active = false;
					propertyChannel->currentValue = propertyChannel->targetValue;
				}

				Detail::MarkTypedStyleFieldImpact(record, field.impact, active);
			}
		}
	}
}
