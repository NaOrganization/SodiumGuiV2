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
			&& record.styleCache.styleTokenTag == record.state.styleTokenTag
			&& record.styleCache.interactionState == interactionState
			&& record.styleCache.layerPriority == layerPriority
			&& record.styleCache.styleIdentityRevision == record.styleIdentityRevision
			&& record.styleCache.styleRevision == context.styleSystem.GetRevision())
		{
			record.style = record.styleCache.computed;
			ApplyWidgetStyleAnimation(record);
			if (record.styleCallback)
				record.styleCallback(*this, record, interactionState, layerPriority);
			return;
		}

		const bool firstStyle = !record.styleCache.valid;
		record.style = context.styleSystem.Resolve(record.state.styleTokenTag, interactionState, layerPriority, record.styleClasses, record.styleScope);
		record.styleCache.computed = record.style;
		record.styleCache.presentation = record.style;
		record.styleCache.rootStyleNodeId = 0;
		record.rootStyleNode.styleNodeId = 0;
		record.rootStyleNode.widgetId = record.state.id;
		record.rootStyleNode.kind = SdStyleNodeKind::Root;
		record.rootStyleNode.part = SdStylePart::Root();
		record.rootStyleNode.scopeId = record.styleScope;
		record.rootStyleNode.pseudoState = SdPseudoState::FromInteraction(interactionState);
		record.rootStyleNode.specifiedStyle = SdPresentationStyleFromLegacyComputed(record.style);
		record.rootStyleNode.resolvedStyle = record.rootStyleNode.specifiedStyle;
		record.rootStyleNode.presentationStyle = record.rootStyleNode.resolvedStyle;
		record.styleCache.styleTokenTag = record.state.styleTokenTag;
		record.styleCache.interactionState = interactionState;
		record.styleCache.layerPriority = layerPriority;
		record.styleCache.styleIdentityRevision = record.styleIdentityRevision;
		record.styleCache.styleRevision = context.styleSystem.GetRevision();
		record.styleCache.valid = true;
		record.state.styleDirty = false;
		SetWidgetStyleAnimationTarget(record, record.styleCache.computed, firstStyle);
		ApplyWidgetStyleAnimation(record);
		if (record.styleCallback)
			record.styleCallback(*this, record, interactionState, layerPriority);
	}

	inline void SdInstance::SetWidgetStyleAnimationTarget(SdWidgetRecord& record, const SdComputedStyle& style, bool immediate)
	{
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
			style.background);
		setColorChannels(
			record.animation.styleBorderR,
			record.animation.styleBorderG,
			record.animation.styleBorderB,
			record.animation.styleBorderA,
			style.border);
	}

	inline void SdInstance::ApplyWidgetStyleAnimation(SdWidgetRecord& record)
	{
		const auto toByte = [](float value) noexcept
		{
			return static_cast<SdUInt8>(std::clamp(value, 0.0f, 255.0f));
		};
		const auto readColorChannels = [this, &toByte](SdAnimationChannelId red, SdAnimationChannelId green, SdAnimationChannelId blue, SdAnimationChannelId alpha, SdColor fallback)
		{
			return SdColor{
				toByte(context.animationSystem.GetValue(red, static_cast<float>(fallback.r))),
				toByte(context.animationSystem.GetValue(green, static_cast<float>(fallback.g))),
				toByte(context.animationSystem.GetValue(blue, static_cast<float>(fallback.b))),
				toByte(context.animationSystem.GetValue(alpha, static_cast<float>(fallback.a)))
			};
		};
		const auto anyColorChannelActive = [this](SdAnimationChannelId red, SdAnimationChannelId green, SdAnimationChannelId blue, SdAnimationChannelId alpha)
		{
			return context.animationSystem.IsActive(red)
				|| context.animationSystem.IsActive(green)
				|| context.animationSystem.IsActive(blue)
				|| context.animationSystem.IsActive(alpha);
		};

		record.style.color = readColorChannels(
			record.animation.styleColorR,
			record.animation.styleColorG,
			record.animation.styleColorB,
			record.animation.styleColorA,
			record.style.color);
		record.style.background = readColorChannels(
			record.animation.styleBackgroundR,
			record.animation.styleBackgroundG,
			record.animation.styleBackgroundB,
			record.animation.styleBackgroundA,
			record.style.background);
		record.style.border = readColorChannels(
			record.animation.styleBorderR,
			record.animation.styleBorderG,
			record.animation.styleBorderB,
			record.animation.styleBorderA,
			record.style.border);

		record.state.animationActive = record.state.animationActive
			|| anyColorChannelActive(
				record.animation.styleColorR,
				record.animation.styleColorG,
				record.animation.styleColorB,
				record.animation.styleColorA)
			|| anyColorChannelActive(
				record.animation.styleBackgroundR,
				record.animation.styleBackgroundG,
				record.animation.styleBackgroundB,
				record.animation.styleBackgroundA)
			|| anyColorChannelActive(
				record.animation.styleBorderR,
				record.animation.styleBorderG,
				record.animation.styleBorderB,
				record.animation.styleBorderA);
		record.styleCache.presentation = record.style;
		record.rootStyleNode.presentationStyle = SdPresentationStyleFromLegacyComputed(record.style);
	}

	template<class TWidget>
	void SdInstance::ResolveTypedWidgetStyle(SdWidgetRecord& record, SdStyleInteractionState interactionState, SdLayerPriority layerPriority)
	{
		if constexpr (requires { typename TWidget::Style; })
		{
			using Style = typename TWidget::Style;
			SdTypedStyleRecord& styleRecord = context.stateStorage.GetOrCreateTypedStyleRecord<Style>(record);
			if (styleRecord.valid
				&& styleRecord.styleTokenTag == record.state.styleTokenTag
				&& styleRecord.interactionState == interactionState
				&& styleRecord.layerPriority == layerPriority
				&& styleRecord.styleIdentityRevision == record.styleIdentityRevision
				&& styleRecord.resolvedInlineStyleRevision == styleRecord.inlineStyleRevision
				&& styleRecord.styleRevision == context.styleSystem.GetRevision())
			{
				return;
			}

			const Style* inlineStyle = context.stateStorage.FindInlineStyle<Style>(styleRecord);
			const Style resolvedTarget = context.styleSystem.ResolveTargetStyle<TWidget>(
				interactionState,
				layerPriority,
				record.styleClasses,
				record.styleScope,
				inlineStyle);
			const bool firstStyle = !styleRecord.valid;
			Style& targetStyle = context.stateStorage.GetOrCreateTargetStyle(styleRecord, resolvedTarget);
			Style& computedStyle = context.stateStorage.GetOrCreateComputedStyle(styleRecord, resolvedTarget);
			if (firstStyle)
			{
				targetStyle = resolvedTarget;
				computedStyle = resolvedTarget;
				styleRecord.animationChannels.clear();
				styleRecord.styleTokenTag = record.state.styleTokenTag;
				styleRecord.interactionState = interactionState;
				styleRecord.layerPriority = layerPriority;
				styleRecord.styleIdentityRevision = record.styleIdentityRevision;
				styleRecord.resolvedInlineStyleRevision = styleRecord.inlineStyleRevision;
				styleRecord.styleRevision = context.styleSystem.GetRevision();
				styleRecord.valid = true;
				return;
			}

			const Style oldTargetStyle = targetStyle;
			targetStyle = resolvedTarget;
			const auto& contract = context.styleSystem.GetContract<TWidget>();
			if (contract.GetFields().empty())
			{
				computedStyle = resolvedTarget;
			}
			else
			{
				for (const SdStyleFieldDescriptor& field : contract.GetFields())
				{
					if (!field.equals || !field.copy)
						continue;
					if (field.equals(&oldTargetStyle, &targetStyle))
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
						channel.startValue = field.readValue(&computedStyle);
						channel.targetValue = field.readValue(&targetStyle);
						channel.currentValue = channel.startValue;
						channel.active = !Detail::StyleValuesEqual(channel.startValue, channel.targetValue);
						if (!channel.active)
							field.writeValue(&computedStyle, channel.targetValue, context.styleSystem.GetTheme());
						Detail::MarkTypedStyleFieldImpact(record, field.impact, channel.active);
					}
					else
					{
						if (SdTypedStyleAnimationChannel* channel = Detail::FindTypedStyleAnimationChannel(styleRecord, field.fieldId))
							channel->active = false;
						field.copy(&computedStyle, &targetStyle);
						Detail::MarkTypedStyleFieldImpact(record, field.impact, false);
					}
				}
			}
			styleRecord.styleTokenTag = record.state.styleTokenTag;
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
			Style* computedStyle = context.stateStorage.FindComputedStyle<Style>(record);
			if (!computedStyle)
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
				field->writeValue(computedStyle, channel.currentValue, context.styleSystem.GetTheme());
				channel.active = t < 1.0f && !Detail::StyleValuesEqual(channel.currentValue, channel.targetValue);
				if (!channel.active)
				{
					channel.currentValue = channel.targetValue;
					field->writeValue(computedStyle, channel.targetValue, context.styleSystem.GetTheme());
				}

				Detail::MarkTypedStyleFieldImpact(record, channel.impact, channel.active);
			}
		}
	}
}
