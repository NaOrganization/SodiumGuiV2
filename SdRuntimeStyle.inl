#pragma once

namespace Sodium
{
	namespace Detail
	{
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
			channel.impact = impact;
			channel.interpolation = interpolation;
			channel.transition = immediate ? SdTransition{} : transition;
			channel.delay = channel.transition.delay;
			channel.elapsed = {};
			channel.startValue = currentValue;
			channel.targetValue = targetValue;
			channel.currentValue = channel.startValue;
			channel.expensiveLayout = expensiveLayout;
			channel.discrete = transition.behavior == SdTransitionBehavior::AllowDiscrete
				&& interpolation == SdStyleInterpolation::None;
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
			SdPropertyAnimationChannel& channel = channels.Ensure(styleNodeId, propertyId);
			return channel.currentValue.kind == SdStyleValueKind::Color ? channel.currentValue.color : fallback;
		}

		inline float GetStyleFloatPropertyChannelValue(
			SdStyleAnimationChannels& channels,
			SdStyleNodeId styleNodeId,
			SdPropertyId propertyId,
			float fallback)
		{
			SdPropertyAnimationChannel& channel = channels.Ensure(styleNodeId, propertyId);
			return channel.currentValue.kind == SdStyleValueKind::Float ? channel.currentValue.number : fallback;
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

	inline void SdInstance::ResolveWidgetStyle(SdWidgetRecord& record, SdStyleInteractionState interactionState, SdLayerPriority layerPriority)
	{
		if (!record.state.styleDirty
			&& record.styleCache.valid
			&& record.styleCache.targetTypeId == record.state.targetTypeId
			&& record.styleCache.interactionState == interactionState
			&& record.styleCache.layerPriority == layerPriority
			&& record.styleCache.styleIdentityRevision == record.styleIdentityRevision
			&& record.styleCache.styleRevision == context.styleSystem.GetRevision())
		{
			ApplyWidgetStyleAnimation(record);
			if (record.styleCallback)
				record.styleCallback(*this, record, interactionState, layerPriority);
			return;
		}

		const bool firstStyle = !record.styleCache.valid;
		const SdWidgetRootStyle resolvedStyle = context.styleSystem.ResolveRootStyle(
			record.state.targetTypeId,
			interactionState,
			layerPriority,
			record.styleClasses,
			record.styleScope);
		SdStyleNode& rootNode = context.stateStorage.EnsureRootStyleNode(record, record.state.id);
		record.styleCache.resolvedStyle = resolvedStyle;
		record.styleCache.presentationStyle = resolvedStyle;
		record.styleCache.rootStyleNodeId = rootNode.styleNodeId;
		rootNode.widgetId = record.state.id;
		rootNode.kind = SdStyleNodeKind::Root;
		rootNode.part = SdStylePart::Root();
		rootNode.scopeId = record.styleScope;
		rootNode.pseudoState = SdPseudoState::FromInteraction(interactionState);
		rootNode.specifiedStyle = resolvedStyle;
		rootNode.resolvedStyle = resolvedStyle;
		rootNode.presentationStyle = resolvedStyle;
		record.rootStyleNode = rootNode;
		for (SdStyleNodeId partNodeId : record.partStyleNodeIds)
		{
			SdStyleNode* partNode = context.stateStorage.FindStyleNodeById(partNodeId);
			if (!partNode)
				continue;
			const SdWidgetPartStyle partStyle = context.styleSystem.ResolvePartStyle(
				record.state.targetTypeId,
				partNode->part,
				resolvedStyle,
				interactionState,
				layerPriority,
				record.styleClasses,
				record.styleScope);
			partNode->widgetId = record.state.id;
			partNode->parentStyleNodeId = rootNode.styleNodeId;
			partNode->scopeId = record.styleScope;
			partNode->pseudoState = SdPseudoState::FromInteraction(interactionState);
			partNode->specifiedStyle = partStyle;
			partNode->resolvedStyle = partStyle;
			partNode->presentationStyle = partStyle;
			SetBoxStyleAnimationTarget(record, *partNode, partNode->resolvedStyle, interactionState, layerPriority, firstStyle);
			ApplyBoxStyleAnimation(*partNode);
		}
		record.styleCache.targetTypeId = record.state.targetTypeId;
		record.styleCache.interactionState = interactionState;
		record.styleCache.layerPriority = layerPriority;
		record.styleCache.styleIdentityRevision = record.styleIdentityRevision;
		record.styleCache.styleRevision = context.styleSystem.GetRevision();
		record.styleCache.valid = true;
		record.state.styleDirty = false;
		SetWidgetStyleAnimationTarget(record, record.styleCache.resolvedStyle, interactionState, layerPriority, firstStyle);
		ApplyWidgetStyleAnimation(record);
		if (record.styleCallback)
			record.styleCallback(*this, record, interactionState, layerPriority);
	}

	inline void SdInstance::SetBoxStyleAnimationTarget(
		SdWidgetRecord& record,
		SdStyleNode& node,
		const SdBoxStyle& style,
		SdStyleInteractionState interactionState,
		SdLayerPriority layerPriority,
		bool immediate)
	{
		const SdPropertyId colorPropertyId = Detail::SdStylePropertyId(&SdBoxStyle::color);
		const SdPropertyId backgroundPropertyId = Detail::SdStylePropertyId(&SdBoxStyle::backgroundColor);
		const SdPropertyId borderPropertyId = Detail::SdStylePropertyId(&SdBoxStyle::border);
		const SdPropertyId opacityPropertyId = Detail::SdStylePropertyId(&SdBoxStyle::opacity);

		const SdTransition defaultTransition = GetDefaultTransition();
		const auto resolveTransition = [this, &record, &node, interactionState, layerPriority, defaultTransition](SdPropertyId propertyId)
		{
			SdTransition transition = defaultTransition;
			if (node.kind == SdStyleNodeKind::Part)
			{
				context.styleSystem.TryResolvePartTransition(
					record.state.targetTypeId,
					node.part,
					propertyId,
					interactionState,
					layerPriority,
					record.styleClasses,
					record.styleScope,
					transition);
			}
			else
			{
				context.styleSystem.TryResolveRootTransition(
					record.state.targetTypeId,
					propertyId,
					interactionState,
					layerPriority,
					record.styleClasses,
					record.styleScope,
					transition);
			}
			return transition;
		};

		Detail::SetStyleColorPropertyChannelTarget(
			context.styleAnimationChannels,
			node.styleNodeId,
			colorPropertyId,
			Detail::GetStyleColorPropertyChannelValue(
				context.styleAnimationChannels,
				node.styleNodeId,
				colorPropertyId,
				style.color),
			style.color,
			resolveTransition(colorPropertyId),
			immediate);
		Detail::SetStyleColorPropertyChannelTarget(
			context.styleAnimationChannels,
			node.styleNodeId,
			backgroundPropertyId,
			Detail::GetStyleColorPropertyChannelValue(
				context.styleAnimationChannels,
				node.styleNodeId,
				backgroundPropertyId,
				style.backgroundColor),
			style.backgroundColor,
			resolveTransition(backgroundPropertyId),
			immediate);
		Detail::SetStyleColorPropertyChannelTarget(
			context.styleAnimationChannels,
			node.styleNodeId,
			borderPropertyId,
			Detail::GetStyleColorPropertyChannelValue(
				context.styleAnimationChannels,
				node.styleNodeId,
				borderPropertyId,
				style.border.left.color),
			style.border.left.color,
			resolveTransition(borderPropertyId),
			immediate);
		Detail::SetStyleFloatPropertyChannelTarget(
			context.styleAnimationChannels,
			node.styleNodeId,
			opacityPropertyId,
			Detail::GetStyleFloatPropertyChannelValue(
				context.styleAnimationChannels,
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
		SdPropertyAnimationChannel& colorChannel = context.styleAnimationChannels.Ensure(
			node.styleNodeId,
			Detail::SdStylePropertyId(&SdBoxStyle::color));
		if (colorChannel.currentValue.kind == SdStyleValueKind::Color)
			presentationStyle.color = colorChannel.currentValue.color;
		SdPropertyAnimationChannel& backgroundChannel = context.styleAnimationChannels.Ensure(
			node.styleNodeId,
			Detail::SdStylePropertyId(&SdBoxStyle::backgroundColor));
		if (backgroundChannel.currentValue.kind == SdStyleValueKind::Color)
			presentationStyle.backgroundColor = backgroundChannel.currentValue.color;
		SdPropertyAnimationChannel& borderChannel = context.styleAnimationChannels.Ensure(
			node.styleNodeId,
			Detail::SdStylePropertyId(&SdBoxStyle::border));
		if (borderChannel.currentValue.kind == SdStyleValueKind::Color)
			presentationStyle.border = SdBorder::All(
				node.resolvedStyle.border.left.width,
				borderChannel.currentValue.color);
		SdPropertyAnimationChannel& opacityChannel = context.styleAnimationChannels.Ensure(
			node.styleNodeId,
			Detail::SdStylePropertyId(&SdBoxStyle::opacity));
		if (opacityChannel.currentValue.kind == SdStyleValueKind::Float)
			presentationStyle.opacity = opacityChannel.currentValue.number;

		node.presentationStyle = presentationStyle;
	}

	inline void SdInstance::SetWidgetStyleAnimationTarget(
		SdWidgetRecord& record,
		const SdWidgetRootStyle& style,
		SdStyleInteractionState interactionState,
		SdLayerPriority layerPriority,
		bool immediate)
	{
		if (SdStyleNode* rootNode = context.stateStorage.FindStyleNodeById(record.rootStyleNodeId))
			SetBoxStyleAnimationTarget(record, *rootNode, style, interactionState, layerPriority, immediate);
	}

	inline void SdInstance::ApplyWidgetStyleAnimation(SdWidgetRecord& record)
	{
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
			|| context.styleAnimationChannels.HasActiveAny(record.rootStyleNodeId, record.partStyleNodeIds);
	}

	template<class TWidget>
	void SdInstance::ResolveTypedWidgetStyle(SdWidgetRecord& record, SdStyleInteractionState interactionState, SdLayerPriority layerPriority)
	{
		if constexpr (requires { typename TWidget::Style; })
		{
			using Style = typename TWidget::Style;
			SdTypedStyleRecord& styleRecord = context.stateStorage.GetOrCreateTypedStyleRecord<Style>(record);
			if (styleRecord.valid
				&& styleRecord.targetTypeId == record.state.targetTypeId
				&& styleRecord.interactionState == interactionState
				&& styleRecord.layerPriority == layerPriority
				&& styleRecord.styleIdentityRevision == record.styleIdentityRevision
				&& styleRecord.resolvedInlineStyleRevision == styleRecord.inlineStyleRevision
				&& styleRecord.styleRevision == context.styleSystem.GetRevision())
			{
				return;
			}

			const Style* inlineStyle = context.stateStorage.FindInlineStyle<Style>(styleRecord);
			const Style resolvedStyleValue = context.styleSystem.ResolveTypedStyle<TWidget>(
				interactionState,
				layerPriority,
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
				styleRecord.layerPriority = layerPriority;
				styleRecord.styleIdentityRevision = record.styleIdentityRevision;
				styleRecord.resolvedInlineStyleRevision = styleRecord.inlineStyleRevision;
				styleRecord.styleRevision = context.styleSystem.GetRevision();
				styleRecord.valid = true;
				return;
			}

			const Style oldResolvedStyle = resolvedStyle;
			resolvedStyle = resolvedStyleValue;
			const auto& contract = context.styleSystem.GetContract<TWidget>();
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
					const bool hasTransition = context.styleSystem.TryResolveTransition<TWidget>(
						field.fieldId,
						interactionState,
						layerPriority,
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
							context.styleAnimationChannels,
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
							field.writeValue(field, &presentationStyle, propertyChannel.currentValue, context.styleSystem.GetTheme());
						Detail::MarkTypedStyleFieldImpact(record, field.impact, propertyChannel.active);
					}
					else
					{
						if (field.readValue)
						{
							SdPropertyAnimationChannel& propertyChannel = context.styleAnimationChannels.Ensure(record.rootStyleNodeId, field.fieldId);
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
			styleRecord.layerPriority = layerPriority;
			styleRecord.styleIdentityRevision = record.styleIdentityRevision;
			styleRecord.resolvedInlineStyleRevision = styleRecord.inlineStyleRevision;
			styleRecord.styleRevision = context.styleSystem.GetRevision();
			styleRecord.valid = true;
		}
	}

	template<class TWidget>
	void SdInstance::AdvanceTypedWidgetStyleAnimations(SdWidgetRecord& record, SdDuration deltaTime)
	{
		if constexpr (requires { typename TWidget::Style; })
		{
			using Style = typename TWidget::Style;
			auto styleIt = record.typedStyles.find(std::type_index(typeid(Style)));
			if (styleIt == record.typedStyles.end())
				return;

			SdTypedStyleRecord& styleRecord = styleIt->second;
			Style* presentationStyle = context.stateStorage.FindPresentationStyle<Style>(record);
			if (!presentationStyle)
				return;

			const auto& contract = context.styleSystem.GetContract<TWidget>();
			(void)deltaTime;
			for (const SdStyleFieldDescriptor& field : contract.GetFields())
			{
				if (!field.writeValue)
					continue;
				SdPropertyAnimationChannel* propertyChannel = context.styleAnimationChannels.Find(record.rootStyleNodeId, field.fieldId);
				if (!propertyChannel)
					continue;

				field.writeValue(field, presentationStyle, propertyChannel->currentValue, context.styleSystem.GetTheme());
				const bool active = propertyChannel->active
					&& !Detail::StyleValuesEqual(propertyChannel->currentValue, propertyChannel->targetValue);
				if (!active)
				{
					field.writeValue(field, presentationStyle, propertyChannel->targetValue, context.styleSystem.GetTheme());
					propertyChannel->active = false;
					propertyChannel->currentValue = propertyChannel->targetValue;
				}

				Detail::MarkTypedStyleFieldImpact(record, field.impact, active);
			}
		}
	}
}
