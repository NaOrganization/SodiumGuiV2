#pragma once

namespace Sodium
{
	namespace Detail
	{
		inline SdTypedStyleAnimationChannel* FindTypedStyleAnimationChannel(SdTypedStyleRecord& styleRecord, SdUInt64 fieldId) noexcept
		{
			for (SdTypedStyleAnimationChannel& channel : styleRecord.animationChannels)
			{
				if (channel.fieldId == fieldId)
					return &channel;
			}
			return nullptr;
		}

		inline SdTypedStyleAnimationChannel& GetOrCreateTypedStyleAnimationChannel(SdTypedStyleRecord& styleRecord, SdUInt64 fieldId)
		{
			if (SdTypedStyleAnimationChannel* channel = FindTypedStyleAnimationChannel(styleRecord, fieldId))
				return *channel;

			SdTypedStyleAnimationChannel& channel = styleRecord.animationChannels.emplace_back();
			channel.fieldId = fieldId;
			return channel;
		}

		inline void MarkTypedStyleFieldImpact(SdWidgetRecord& record, SdStyleFieldImpact impact, bool animationActive) noexcept
		{
			if (impact == SdStyleFieldImpact::Layout)
				record.state.layoutDirty = true;
			if (animationActive)
				record.state.animationActive = true;
		}

		inline SdUInt8 ClampColorByte(float value) noexcept
		{
			return static_cast<SdUInt8>(std::clamp(value, 0.0f, 255.0f));
		}

		inline SdColor ReadStyleColorChannels(
			SdAnimationSystem& animationSystem,
			SdAnimationChannelId red,
			SdAnimationChannelId green,
			SdAnimationChannelId blue,
			SdAnimationChannelId alpha,
			SdColor fallback) noexcept
		{
			return {
				ClampColorByte(animationSystem.GetValue(red, static_cast<float>(fallback.r))),
				ClampColorByte(animationSystem.GetValue(green, static_cast<float>(fallback.g))),
				ClampColorByte(animationSystem.GetValue(blue, static_cast<float>(fallback.b))),
				ClampColorByte(animationSystem.GetValue(alpha, static_cast<float>(fallback.a)))
			};
		}

		inline bool IsAnyStyleColorChannelActive(
			SdAnimationSystem& animationSystem,
			SdAnimationChannelId red,
			SdAnimationChannelId green,
			SdAnimationChannelId blue,
			SdAnimationChannelId alpha) noexcept
		{
			return animationSystem.IsActive(red)
				|| animationSystem.IsActive(green)
				|| animationSystem.IsActive(blue)
				|| animationSystem.IsActive(alpha);
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
			SdPropertyAnimationChannel& channel = channels.Ensure(styleNodeId, propertyId);
			channel.impact = SdStyleFieldImpact::Paint;
			channel.interpolation = SdStyleInterpolation::Color;
			channel.transition = immediate ? SdTransition{} : transition;
			channel.elapsed = {};
			channel.startValue = SdStyleValue::FromColor(currentValue);
			channel.targetValue = SdStyleValue::FromColor(targetValue);
			channel.currentValue = channel.startValue;
			channel.active = !immediate && !StyleValuesEqual(channel.startValue, channel.targetValue);
			if (immediate || !channel.active)
				channel.currentValue = channel.targetValue;
		}

		inline void UpdateStyleColorPropertyChannel(
			SdStyleAnimationChannels& channels,
			SdAnimationSystem& animationSystem,
			SdStyleNodeId styleNodeId,
			SdPropertyId propertyId,
			SdAnimationChannelId red,
			SdAnimationChannelId green,
			SdAnimationChannelId blue,
			SdAnimationChannelId alpha,
			SdColor fallback)
		{
			SdPropertyAnimationChannel& channel = channels.Ensure(styleNodeId, propertyId);
			channel.currentValue = SdStyleValue::FromColor(ReadStyleColorChannels(
				animationSystem,
				red,
				green,
				blue,
				alpha,
				fallback));
			channel.active = IsAnyStyleColorChannelActive(animationSystem, red, green, blue, alpha);
			if (!channel.active)
				channel.currentValue = channel.targetValue;
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
		}
		record.styleCache.targetTypeId = record.state.targetTypeId;
		record.styleCache.interactionState = interactionState;
		record.styleCache.layerPriority = layerPriority;
		record.styleCache.styleIdentityRevision = record.styleIdentityRevision;
		record.styleCache.styleRevision = context.styleSystem.GetRevision();
		record.styleCache.valid = true;
		record.state.styleDirty = false;
		SetWidgetStyleAnimationTarget(record, record.styleCache.resolvedStyle, firstStyle);
		ApplyWidgetStyleAnimation(record);
		if (record.styleCallback)
			record.styleCallback(*this, record, interactionState, layerPriority);
	}

	inline void SdInstance::SetWidgetStyleAnimationTarget(SdWidgetRecord& record, const SdWidgetRootStyle& style, bool immediate)
	{
		const SdPropertyId colorPropertyId = Detail::SdStylePropertyId(&SdBoxStyle::color);
		const SdPropertyId backgroundPropertyId = Detail::SdStylePropertyId(&SdBoxStyle::backgroundColor);
		const auto setChannel = [this, immediate](SdAnimationChannelId channelId, float target)
		{
			if (immediate)
			{
				context.animationSystem.SetImmediate(channelId, target);
				return;
			}
			context.animationSystem.SetTarget(channelId, target, GetDefaultTransition());
		};
		const auto setColorChannels = [&setChannel](SdAnimationChannelId red, SdAnimationChannelId green, SdAnimationChannelId blue, SdAnimationChannelId alpha, SdColor color)
		{
			setChannel(red, static_cast<float>(color.r));
			setChannel(green, static_cast<float>(color.g));
			setChannel(blue, static_cast<float>(color.b));
			setChannel(alpha, static_cast<float>(color.a));
		};

		setColorChannels(
			record.animation.styleColorR,
			record.animation.styleColorG,
			record.animation.styleColorB,
			record.animation.styleColorA,
			style.color);
		setColorChannels(
			record.animation.styleBackgroundR,
			record.animation.styleBackgroundG,
			record.animation.styleBackgroundB,
			record.animation.styleBackgroundA,
			style.backgroundColor);
		setColorChannels(
			record.animation.styleBorderR,
			record.animation.styleBorderG,
			record.animation.styleBorderB,
			record.animation.styleBorderA,
			style.border.left.color);

		const SdTransition transition = GetDefaultTransition();
		Detail::SetStyleColorPropertyChannelTarget(
			context.styleAnimationChannels,
			record.rootStyleNodeId,
			colorPropertyId,
			Detail::ReadStyleColorChannels(
				context.animationSystem,
				record.animation.styleColorR,
				record.animation.styleColorG,
				record.animation.styleColorB,
				record.animation.styleColorA,
				style.color),
			style.color,
			transition,
			immediate);
		Detail::SetStyleColorPropertyChannelTarget(
			context.styleAnimationChannels,
			record.rootStyleNodeId,
			backgroundPropertyId,
			Detail::ReadStyleColorChannels(
				context.animationSystem,
				record.animation.styleBackgroundR,
				record.animation.styleBackgroundG,
				record.animation.styleBackgroundB,
				record.animation.styleBackgroundA,
				style.backgroundColor),
			style.backgroundColor,
			transition,
			immediate);
	}

	inline void SdInstance::ApplyWidgetStyleAnimation(SdWidgetRecord& record)
	{
		SdWidgetRootStyle presentationStyle = record.styleCache.resolvedStyle;
		presentationStyle.border = SdBorder::All(
			record.styleCache.resolvedStyle.border.left.width,
			Detail::ReadStyleColorChannels(
				context.animationSystem,
				record.animation.styleBorderR,
				record.animation.styleBorderG,
				record.animation.styleBorderB,
				record.animation.styleBorderA,
				record.styleCache.resolvedStyle.border.left.color));

		record.state.animationActive = record.state.animationActive
			|| Detail::IsAnyStyleColorChannelActive(
				context.animationSystem,
				record.animation.styleColorR,
				record.animation.styleColorG,
				record.animation.styleColorB,
				record.animation.styleColorA)
			|| Detail::IsAnyStyleColorChannelActive(
				context.animationSystem,
				record.animation.styleBackgroundR,
				record.animation.styleBackgroundG,
				record.animation.styleBackgroundB,
				record.animation.styleBackgroundA)
			|| Detail::IsAnyStyleColorChannelActive(
				context.animationSystem,
				record.animation.styleBorderR,
				record.animation.styleBorderG,
				record.animation.styleBorderB,
				record.animation.styleBorderA);
		Detail::UpdateStyleColorPropertyChannel(
			context.styleAnimationChannels,
			context.animationSystem,
			record.rootStyleNodeId,
			Detail::SdStylePropertyId(&SdBoxStyle::color),
			record.animation.styleColorR,
			record.animation.styleColorG,
			record.animation.styleColorB,
			record.animation.styleColorA,
			record.styleCache.resolvedStyle.color);
		SdPropertyAnimationChannel& colorChannel = context.styleAnimationChannels.Ensure(
			record.rootStyleNodeId,
			Detail::SdStylePropertyId(&SdBoxStyle::color));
		if (colorChannel.currentValue.kind == SdStyleValueKind::Color)
			presentationStyle.color = colorChannel.currentValue.color;
		Detail::UpdateStyleColorPropertyChannel(
			context.styleAnimationChannels,
			context.animationSystem,
			record.rootStyleNodeId,
			Detail::SdStylePropertyId(&SdBoxStyle::backgroundColor),
			record.animation.styleBackgroundR,
			record.animation.styleBackgroundG,
			record.animation.styleBackgroundB,
			record.animation.styleBackgroundA,
			record.styleCache.resolvedStyle.backgroundColor);
		SdPropertyAnimationChannel& backgroundChannel = context.styleAnimationChannels.Ensure(
			record.rootStyleNodeId,
			Detail::SdStylePropertyId(&SdBoxStyle::backgroundColor));
		if (backgroundChannel.currentValue.kind == SdStyleValueKind::Color)
			presentationStyle.backgroundColor = backgroundChannel.currentValue.color;
		record.styleCache.presentationStyle = presentationStyle;
		if (SdStyleNode* rootNode = context.stateStorage.FindStyleNodeById(record.rootStyleNodeId))
		{
			rootNode->presentationStyle = presentationStyle;
			record.rootStyleNode = *rootNode;
		}
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
				styleRecord.animationChannels.clear();
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
					if (field.equals(&oldResolvedStyle, &resolvedStyle))
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
					const bool canTransition = hasTransition
						&& layoutTransitionAllowed
						&& field.interpolation != SdStyleInterpolation::None
						&& field.readValue
						&& field.writeValue;

					if (canTransition)
					{
						SdTypedStyleAnimationChannel& channel = Detail::GetOrCreateTypedStyleAnimationChannel(styleRecord, field.fieldId);
						channel.impact = field.impact;
						channel.interpolation = field.interpolation;
						channel.transition = transition;
						channel.elapsed = {};
						channel.startValue = field.readValue(&presentationStyle);
						channel.targetValue = field.readValue(&resolvedStyle);
						channel.currentValue = channel.startValue;
						channel.active = !Detail::StyleValuesEqual(channel.startValue, channel.targetValue);
						if (!channel.active)
							field.writeValue(&presentationStyle, channel.targetValue, context.styleSystem.GetTheme());
						Detail::MarkTypedStyleFieldImpact(record, field.impact, channel.active);
					}
					else
					{
						if (SdTypedStyleAnimationChannel* channel = Detail::FindTypedStyleAnimationChannel(styleRecord, field.fieldId))
							channel->active = false;
						field.copy(&presentationStyle, &resolvedStyle);
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
			const float seconds = std::max(0.001f, static_cast<float>(deltaTime.count()) / 1000000000.0f);
			for (SdTypedStyleAnimationChannel& channel : styleRecord.animationChannels)
			{
				if (!channel.active)
					continue;

				const SdStyleFieldDescriptor* field = contract.Find(channel.fieldId);
				if (!field || !field->writeValue)
				{
					channel.active = false;
					continue;
				}

				channel.elapsed += std::chrono::duration_cast<SdDuration>(std::chrono::duration<float>(seconds));
				const float durationSeconds = std::max(0.001f, static_cast<float>(channel.transition.duration.count()) / 1000000000.0f);
				const float elapsedSeconds = std::max(0.0f, static_cast<float>(channel.elapsed.count()) / 1000000000.0f);
				const float t = std::clamp(elapsedSeconds / durationSeconds, 0.0f, 1.0f);
				const float eased = SdApplyEasing(t, channel.transition.easing);
				channel.currentValue = Detail::InterpolateStyleValue(channel.startValue, channel.targetValue, channel.interpolation, eased);
				field->writeValue(presentationStyle, channel.currentValue, context.styleSystem.GetTheme());
				channel.active = t < 1.0f && !Detail::StyleValuesEqual(channel.currentValue, channel.targetValue);
				if (!channel.active)
				{
					channel.currentValue = channel.targetValue;
					field->writeValue(presentationStyle, channel.targetValue, context.styleSystem.GetTheme());
				}

				Detail::MarkTypedStyleFieldImpact(record, channel.impact, channel.active);
			}
		}
	}
}
