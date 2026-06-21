#pragma once

namespace Sodium
{
	template<class T>
	T& SdInstance::GetOrCreateUserState(SdWidgetId widgetId)
	{
		return context.stateStorage.GetOrCreateUserState<T>(widgetId);
	}

	template<class T>
	T& SdInstance::GetOrCreateModel(SdResolvedKey resolvedKey)
	{
		return context.stateStorage.GetOrCreateModel<T>(resolvedKey);
	}

	template<class TWidget>
	const typename TWidget::Style& SdInstance::GetResolvedStyle(SdWidgetId widgetId)
	{
		SdWidgetRecord* record = context.stateStorage.FindWidgetRecord(widgetId);
		assert(record);
		using Style = typename TWidget::Style;
		if (Style* style = context.stateStorage.FindResolvedStyle<Style>(*record))
			return *style;
		ResolveTypedWidgetStyle<TWidget>(*record, SdStyleInteractionState::Normal, record->state.layerPriority);
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
		ResolveTypedWidgetStyle<TWidget>(*record, SdStyleInteractionState::Normal, record->state.layerPriority);
		Style* style = context.stateStorage.FindPresentationStyle<Style>(*record);
		assert(style);
		return *style;
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
		assert(node);
		return node ? *node : GetRootStyleNode(widgetId);
	}

	inline SdStyleNode& SdInstance::EnsureStylePart(SdWidgetId widgetId, SdStylePart part)
	{
		SdWidgetRecord* record = context.stateStorage.FindWidgetRecord(widgetId);
		assert(record);
		return context.stateStorage.EnsurePartStyleNode(*record, part);
	}

	template<class T>
	T& SdWidgetContextBase::State()
	{
		return instance.GetOrCreateUserState<T>(id);
	}

	template<class T>
	T& SdWidgetContextBase::Model()
	{
		assert(resolvedKey != 0);
		return instance.GetOrCreateModel<T>(resolvedKey);
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

	inline bool SdWidgetContextBase::IsCaptured() const noexcept
	{
		return instance.GetInteractionSystem().IsCaptured(id);
	}

	inline bool SdWidgetContextBase::IsFocused() const noexcept
	{
		return instance.GetInteractionSystem().IsFocused(id);
	}
}
