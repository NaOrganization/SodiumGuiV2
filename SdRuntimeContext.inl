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
	const typename TWidget::Style& SdInstance::GetTargetStyle(SdWidgetId widgetId)
	{
		SdWidgetRecord* record = context.stateStorage.FindWidgetRecord(widgetId);
		assert(record);
		using Style = typename TWidget::Style;
		if (Style* style = context.stateStorage.FindTargetStyle<Style>(*record))
			return *style;
		ResolveTypedWidgetStyle<TWidget>(*record, SdStyleInteractionState::Normal, record->state.layerPriority);
		Style* style = context.stateStorage.FindTargetStyle<Style>(*record);
		assert(style);
		return *style;
	}

	template<class TWidget>
	const typename TWidget::Style& SdInstance::GetComputedStyle(SdWidgetId widgetId)
	{
		SdWidgetRecord* record = context.stateStorage.FindWidgetRecord(widgetId);
		assert(record);
		using Style = typename TWidget::Style;
		if (Style* style = context.stateStorage.FindComputedStyle<Style>(*record))
			return *style;
		ResolveTypedWidgetStyle<TWidget>(*record, SdStyleInteractionState::Normal, record->state.layerPriority);
		Style* style = context.stateStorage.FindComputedStyle<Style>(*record);
		assert(style);
		return *style;
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

	template<class TWidget>
	const typename TWidget::Style& SdWidgetContextBase::TargetStyle()
	{
		return instance.GetTargetStyle<TWidget>(id);
	}

	template<class TWidget>
	const typename TWidget::Style& SdWidgetContextBase::ComputedStyle()
	{
		return instance.GetComputedStyle<TWidget>(id);
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
