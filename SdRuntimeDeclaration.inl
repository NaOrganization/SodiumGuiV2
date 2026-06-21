#pragma once

namespace Sodium
{
	template<SdDeclarableWidget T, class TConfigureStyle, class... TArgs>
	T& SdUi::DeclareResolved(
		SdWidgetId id,
		SdWidgetId parentId,
		SdResolvedKey resolvedKey,
		SdUtf8StringView debugKey,
		TConfigureStyle&& configureStyle,
		TArgs&&... args)
	{
		SdWidgetRecord& record = instance.GetOrCreateWidgetRecord(id);
		const bool created = record.widgetType != std::type_index(typeid(T)) || !instance.context.stateStorage.HasWidgetObject(record);

		if (created)
		{
			record.widgetType = std::type_index(typeid(T));
			instance.context.stateStorage.CreateWidgetObject<T>(record);
			record.styleCallback = &SdInstance::StyleThunk<T>;
			record.typedStyleAnimationCallback = &SdInstance::TypedStyleAnimationThunk<T>;
			record.layoutCallback = &SdInstance::LayoutThunk<T>;
			record.paintCallback = &SdInstance::PaintThunk<T>;
		}

		instance.MarkSubmitted(record, id, parentId, resolvedKey, debugKey);
		std::forward<TConfigureStyle>(configureStyle)(record);
		T& widget = *instance.context.stateStorage.GetWidgetObject<T>(record);

		SdCreateContext createContext{
			instance,
			*this,
			id,
			parentId,
			record.state,
			instance.context.theme,
			record.resolvedKey
		};
		if (created)
		{
			if constexpr (requires(T& value, SdCreateContext& context, TArgs&&... values) { value.OnCreate(context, std::forward<TArgs>(values)...); })
				widget.OnCreate(createContext, std::forward<TArgs>(args)...);
			else if constexpr (requires(T& value, SdCreateContext& context) { value.OnCreate(context); })
				widget.OnCreate(createContext);
		}

		SdUpdateContext updateContext{
			instance,
			*this,
			id,
			parentId,
			record.state,
			instance.context.theme,
			record.resolvedKey,
			instance.context.input.GetSnapshot(),
			instance.context.frame.deltaTime,
			instance.context.frame.frameIndex
		};

		idStack.PushParent(id);
		if constexpr (requires(T& value, SdUpdateContext& context, TArgs&&... values) { value.OnUpdate(context, std::forward<TArgs>(values)...); })
			widget.OnUpdate(updateContext, std::forward<TArgs>(args)...);
		else if constexpr (requires(T& value, SdUpdateContext& context) { value.OnUpdate(context); })
			widget.OnUpdate(updateContext);
		idStack.PopParent();

		return widget;
	}

	template<SdDeclarableWidget T, class... TArgs>
	T& SdUi::Declare(TArgs&&... args)
	{
		const SdWidgetId parentId = idStack.CurrentParentId();
		const SdWidgetId id = idStack.ResolveAnonymousWidgetId(Detail::SdTypeHash<T>());
		auto configureStyle = [this](SdWidgetRecord& record)
		{
			instance.SetWidgetStyleIdentity(record, {}, 0);
			if constexpr (requires { typename T::Style; })
			{
				if (instance.context.stateStorage.ClearInlineStyle<typename T::Style>(record))
					record.state.styleDirty = true;
			}
		};
		return DeclareResolved<T>(id, parentId, 0, {}, configureStyle, std::forward<TArgs>(args)...);
	}

	template<SdDeclarableWidget T, class... TArgs>
	T& SdUi::DeclareKeyed(SdUtf8StringView key, TArgs&&... args)
	{
		const SdWidgetId parentId = idStack.CurrentParentId();
		SdResolvedKey resolvedKey = 0;
		const SdWidgetId id = idStack.ResolveKeyedWidgetId(Detail::SdTypeHash<T>(), key, resolvedKey);
		auto configureStyle = [this](SdWidgetRecord& record)
		{
			instance.SetWidgetStyleIdentity(record, {}, 0);
			if constexpr (requires { typename T::Style; })
			{
				if (instance.context.stateStorage.ClearInlineStyle<typename T::Style>(record))
					record.state.styleDirty = true;
			}
		};
		return DeclareResolved<T>(id, parentId, resolvedKey, key, configureStyle, std::forward<TArgs>(args)...);
	}

	template<SdStylableWidget T, class... TArgs>
	T& SdUi::DeclareStyled(const typename T::Style* inlineStyle, TArgs&&... args)
	{
		return DeclareStyled<T>(SdStyleIdentity{}, inlineStyle, std::forward<TArgs>(args)...);
	}

	template<SdStylableWidget T, class... TArgs>
	T& SdUi::DeclareStyled(SdStyleIdentity styleIdentity, const typename T::Style* inlineStyle, TArgs&&... args)
	{
		const SdWidgetId parentId = idStack.CurrentParentId();
		const SdWidgetId id = idStack.ResolveAnonymousWidgetId(Detail::SdTypeHash<T>());
		auto configureStyle = [this, styleIdentity, inlineStyle](SdWidgetRecord& record)
		{
			instance.SetWidgetStyleIdentity(record, styleIdentity.classes, styleIdentity.scope);
			if (instance.context.stateStorage.SetInlineStyle<typename T::Style>(record, inlineStyle))
				record.state.styleDirty = true;
		};
		return DeclareResolved<T>(id, parentId, 0, {}, configureStyle, std::forward<TArgs>(args)...);
	}

	template<SdStylableWidget T, class... TArgs>
	T& SdUi::DeclareStyled(SdStyleIdentity styleIdentity, TArgs&&... args)
	{
		const SdWidgetId parentId = idStack.CurrentParentId();
		const SdWidgetId id = idStack.ResolveAnonymousWidgetId(Detail::SdTypeHash<T>());
		auto configureStyle = [this, styleIdentity](SdWidgetRecord& record)
		{
			instance.SetWidgetStyleIdentity(record, styleIdentity.classes, styleIdentity.scope);
			if (instance.context.stateStorage.ClearInlineStyle<typename T::Style>(record))
				record.state.styleDirty = true;
		};
		return DeclareResolved<T>(id, parentId, 0, {}, configureStyle, std::forward<TArgs>(args)...);
	}

	template<SdStylableWidget T, class... TArgs>
	T& SdUi::DeclareStyledKeyed(SdUtf8StringView key, const typename T::Style* inlineStyle, TArgs&&... args)
	{
		return DeclareStyledKeyed<T>(key, SdStyleIdentity{}, inlineStyle, std::forward<TArgs>(args)...);
	}

	template<SdStylableWidget T, class... TArgs>
	T& SdUi::DeclareStyledKeyed(SdUtf8StringView key, SdStyleIdentity styleIdentity, const typename T::Style* inlineStyle, TArgs&&... args)
	{
		const SdWidgetId parentId = idStack.CurrentParentId();
		SdResolvedKey resolvedKey = 0;
		const SdWidgetId id = idStack.ResolveKeyedWidgetId(Detail::SdTypeHash<T>(), key, resolvedKey);
		auto configureStyle = [this, styleIdentity, inlineStyle](SdWidgetRecord& record)
		{
			instance.SetWidgetStyleIdentity(record, styleIdentity.classes, styleIdentity.scope);
			if (instance.context.stateStorage.SetInlineStyle<typename T::Style>(record, inlineStyle))
				record.state.styleDirty = true;
		};
		return DeclareResolved<T>(id, parentId, resolvedKey, key, configureStyle, std::forward<TArgs>(args)...);
	}

	template<SdStylableWidget T, class... TArgs>
	T& SdUi::DeclareStyledKeyed(SdUtf8StringView key, SdStyleIdentity styleIdentity, TArgs&&... args)
	{
		const SdWidgetId parentId = idStack.CurrentParentId();
		SdResolvedKey resolvedKey = 0;
		const SdWidgetId id = idStack.ResolveKeyedWidgetId(Detail::SdTypeHash<T>(), key, resolvedKey);
		auto configureStyle = [this, styleIdentity](SdWidgetRecord& record)
		{
			instance.SetWidgetStyleIdentity(record, styleIdentity.classes, styleIdentity.scope);
			if (instance.context.stateStorage.ClearInlineStyle<typename T::Style>(record))
				record.state.styleDirty = true;
		};
		return DeclareResolved<T>(id, parentId, resolvedKey, key, configureStyle, std::forward<TArgs>(args)...);
	}

	template<SdDeclarableWidget TWidget, class TModel>
	TModel& SdUi::Model(SdUtf8StringView key)
	{
		const SdResolvedKey resolvedKey = idStack.ResolveModelKey(Detail::SdTypeHash<TWidget>(), key);
		return instance.GetOrCreateModel<TModel>(resolvedKey);
	}

	template<SdDeclarableWidget TWidget, class TConfigure, class TModel>
	void SdUi::ConfigureModel(SdUtf8StringView key, TConfigure&& configure)
	{
		TModel& model = Model<TWidget, TModel>(key);
		std::forward<TConfigure>(configure)(model);
	}
}
