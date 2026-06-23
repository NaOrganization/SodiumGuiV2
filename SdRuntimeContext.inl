#pragma once

namespace Sodium
{
	template<class T>
	T& SdInstance::GetOrCreateUserState(SdWidgetId widgetId)
	{
		return context.stateStorage.GetOrCreateUserState<T>(widgetId);
	}

	template<class T>
	T& SdInstance::GetOrCreateModel(SdResolvedKey resolvedKey, SdModelLifetime lifetime, SdWidgetId ownerWidgetId)
	{
		return context.stateStorage.GetOrCreateModel<T>(resolvedKey, lifetime, ownerWidgetId);
	}

	template<class TWidget>
	const typename TWidget::Style& SdInstance::GetResolvedStyle(SdWidgetId widgetId)
	{
		SdWidgetRecord* record = context.stateStorage.FindWidgetRecord(widgetId);
		assert(record);
		using Style = typename TWidget::Style;
		if (Style* style = context.stateStorage.FindResolvedStyle<Style>(*record))
			return *style;
		ResolveTypedWidgetStyle<TWidget>(*record, SdStyleInteractionState::Normal, record->state.rootLayer);
		Style* style = context.stateStorage.FindResolvedStyle<Style>(*record);
		assert(style);
		return *style;
	}

	template<class TWidget>
	const typename TWidget::Style& SdInstance::GetPresentationStyle(SdWidgetId widgetId)
	{
		SdWidgetRecord* record = context.stateStorage.FindWidgetRecord(widgetId);
		assert(record);
		using Style = typename TWidget::Style;
		if (Style* style = context.stateStorage.FindPresentationStyle<Style>(*record))
			return *style;
		ResolveTypedWidgetStyle<TWidget>(*record, SdStyleInteractionState::Normal, record->state.rootLayer);
		Style* style = context.stateStorage.FindPresentationStyle<Style>(*record);
		assert(style);
		return *style;
	}

	inline SdWidgetRootStyle SdInstance::ResolveRootStyleForWidget(
		SdWidgetId widgetId,
		SdStyleInteractionState interactionState,
		SdRootLayer rootLayer) const
	{
		const SdWidgetRecord* record = context.stateStorage.FindWidgetRecord(widgetId);
		assert(record);
		if (!record)
			return context.styling.ResolveRootStyle(SdWidgetTargetIds::Default, interactionState, rootLayer);
		const SdWidgetRootStyle* inlineRootStyle = context.stateStorage.FindInlineStyle<SdWidgetRootStyle>(*record);
		return context.styling.ResolveRootStyle(
			record->state.targetTypeId,
			interactionState,
			rootLayer,
			record->styleClasses,
			record->styleScope,
			inlineRootStyle);
	}

	inline const SdStyleNode& SdInstance::GetRootStyleNode(SdWidgetId widgetId) const
	{
		const SdWidgetRecord* record = context.stateStorage.FindWidgetRecord(widgetId);
		assert(record);
		const SdStyleNode* node = record ? context.stateStorage.FindStyleNode(*record, SdStylePart::Root()) : nullptr;
		assert(node);
		return node ? *node : record->rootStyleNode;
	}

	inline const SdStyleNode& SdInstance::GetStylePart(SdWidgetId widgetId, SdStylePart part) const
	{
		const SdWidgetRecord* record = context.stateStorage.FindWidgetRecord(widgetId);
		assert(record);
		const SdStyleNode* node = record ? context.stateStorage.FindStyleNode(*record, part) : nullptr;
		return node ? *node : GetRootStyleNode(widgetId);
	}

	inline SdStyleNode& SdInstance::EnsureStylePart(SdWidgetId widgetId, SdStylePart part)
	{
		SdWidgetRecord* record = context.stateStorage.FindWidgetRecord(widgetId);
		assert(record);
		return context.stateStorage.EnsurePartStyleNode(*record, part);
	}

	inline void SdInstance::SetPartUsedBox(SdWidgetId widgetId, SdStylePart part, const SdUsedBox& usedBox)
	{
		SdStyleNode& node = EnsureStylePart(widgetId, part);
		node.usedBox = usedBox;
	}

	inline void SdInstance::SetPartLayoutBox(SdWidgetId widgetId, SdStylePart part, const SdUsedBox& layoutBox)
	{
		SdStyleNode& node = EnsureStylePart(widgetId, part);
		node.layoutBox = layoutBox;
	}

	inline void SdInstance::SetPartBorderBox(SdWidgetId widgetId, SdStylePart part, SdRect borderBox)
	{
		SdStyleNode& node = EnsureStylePart(widgetId, part);
		const SdUsedBox box = SdBuildUsedBox(borderBox, node.resolvedStyle);
		node.usedBox = box;
		node.layoutBox = box;
	}

	template<class T>
	T& SdWidgetContextBase::State()
	{
		return instance.GetOrCreateUserState<T>(id);
	}

	template<class T>
	T& SdWidgetContextBase::Model()
	{
		return Model<T>(SdModelLifetime::Widget);
	}

	template<class T>
	T& SdWidgetContextBase::Model(SdModelLifetime lifetime)
	{
		assert(resolvedKey != 0);
		SdWidgetId ownerWidgetId = 0;
		if (lifetime == SdModelLifetime::Widget)
			ownerWidgetId = id;
		else if (lifetime == SdModelLifetime::Scope)
			ownerWidgetId = parentId;
		return instance.GetOrCreateModel<T>(resolvedKey, lifetime, ownerWidgetId);
	}

	inline const SdStyleNode& SdWidgetContextBase::RootStyleNode() const
	{
		return instance.GetRootStyleNode(id);
	}

	inline const SdStyleNode& SdWidgetContextBase::Part(SdStylePart part) const
	{
		return instance.GetStylePart(id, part);
	}

	inline SdStyleNode& SdWidgetContextBase::EnsurePart(SdStylePart part)
	{
		return instance.EnsureStylePart(id, part);
	}

	inline void SdWidgetContextBase::SetPartUsedBox(SdStylePart part, const SdUsedBox& usedBox)
	{
		instance.SetPartUsedBox(id, part, usedBox);
	}

	inline void SdWidgetContextBase::SetPartLayoutBox(SdStylePart part, const SdUsedBox& layoutBox)
	{
		instance.SetPartLayoutBox(id, part, layoutBox);
	}

	inline void SdWidgetContextBase::SetPartBorderBox(SdStylePart part, SdRect borderBox)
	{
		instance.SetPartBorderBox(id, part, borderBox);
	}

	template<class TWidget>
	const typename TWidget::Style& SdWidgetContextBase::RootResolvedStyle()
	{
		return instance.GetResolvedStyle<TWidget>(id);
	}

	template<class TWidget>
	const typename TWidget::Style& SdWidgetContextBase::RootPresentationStyle()
	{
		return instance.GetPresentationStyle<TWidget>(id);
	}

	inline bool SdWidgetContextBase::HasModelKey() const noexcept
	{
		return resolvedKey != 0;
	}

	inline bool SdWidgetContextBase::IsHovered() const noexcept
	{
		return instance.GetInteractionSystem().IsHovered(id);
	}

	inline bool SdWidgetContextBase::IsPressed() const noexcept
	{
		return instance.GetInteractionSystem().IsPressed(id);
	}

	inline bool SdWidgetContextBase::WasPressed() const noexcept
	{
		return instance.GetInteractionSystem().WasPressed(id);
	}

	inline bool SdWidgetContextBase::WasReleased() const noexcept
	{
		return instance.GetInteractionSystem().WasReleased(id);
	}

	inline bool SdWidgetContextBase::WasClicked() const noexcept
	{
		return instance.GetInteractionSystem().WasClicked(id);
	}

	inline bool SdWidgetContextBase::WasClicked(SdMouseButton button) const noexcept
	{
		return instance.GetInteractionSystem().WasClicked(id, button);
	}

	inline bool SdWidgetContextBase::WasDoubleClicked(SdMouseButton button) const noexcept
	{
		return instance.GetInteractionSystem().WasDoubleClicked(id, button);
	}

	inline bool SdWidgetContextBase::WasLongPressed(SdMouseButton button) const noexcept
	{
		return instance.GetInteractionSystem().WasLongPressed(id, button);
	}

	inline bool SdWidgetContextBase::WasOutsideClicked() const noexcept
	{
		return instance.GetInteractionSystem().WasOutsideClicked(id);
	}

	inline bool SdWidgetContextBase::IsWheelTarget() const noexcept
	{
		return instance.GetInteractionSystem().IsWheelTarget(id);
	}

	inline bool SdWidgetContextBase::IsKeyboardTarget() const noexcept
	{
		return instance.GetInteractionSystem().IsKeyboardTarget(id);
	}

	inline bool SdWidgetContextBase::IsDragSource() const noexcept
	{
		return instance.GetInteractionSystem().IsDragSource(id);
	}

	inline bool SdWidgetContextBase::IsDragTarget() const noexcept
	{
		return instance.GetInteractionSystem().IsDragTarget(id);
	}

	inline bool SdWidgetContextBase::IsDropTarget() const noexcept
	{
		return instance.GetInteractionSystem().IsDropTarget(id);
	}

	inline bool SdWidgetContextBase::IsCaptured() const noexcept
	{
		return instance.GetInteractionSystem().IsCaptured(id);
	}

	inline bool SdWidgetContextBase::IsFocused() const noexcept
	{
		return instance.GetInteractionSystem().IsFocused(id);
	}
}
