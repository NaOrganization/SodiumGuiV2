#pragma once

#include "SdIdStack.h"
#include "SdRuntimeStorage.h"

#include <functional>
#include <string_view>

namespace Sodium
{
	class SdUi final
	{
	private:
		friend class SdInstance;

		SdInstance& instance;
		SdIdStack idStack = {};
		void BeginDeclarationFrame();

		template<SdDeclarableWidget T, class TConfigureStyle, class... TArgs>
		T& DeclareResolved(
			SdWidgetId id,
			SdWidgetId parentId,
			SdResolvedKey resolvedKey,
			SdUtf8StringView debugKey,
			TConfigureStyle&& configureStyle,
			TArgs&&... args);

	public:
		explicit SdUi(SdInstance& owner);

		template<SdDeclarableWidget T, class... TArgs>
		T& Declare(TArgs&&... args);

		template<SdDeclarableWidget T, class... TArgs>
		T& DeclareKeyed(SdUtf8StringView key, TArgs&&... args);

		template<SdStylableWidget T, class... TArgs>
		T& DeclareStyled(const typename T::Style* inlineStyle, TArgs&&... args);

		template<SdStylableWidget T, class... TArgs>
		T& DeclareStyled(SdStyleIdentity styleIdentity, TArgs&&... args);

		template<SdStylableWidget T, class... TArgs>
		T& DeclareStyled(SdStyleIdentity styleIdentity, const typename T::Style* inlineStyle, TArgs&&... args);

		template<SdStylableWidget T, class... TArgs>
		T& DeclareStyledKeyed(SdUtf8StringView key, const typename T::Style* inlineStyle, TArgs&&... args);

		template<SdStylableWidget T, class... TArgs>
		T& DeclareStyledKeyed(SdUtf8StringView key, SdStyleIdentity styleIdentity, TArgs&&... args);

		template<SdStylableWidget T, class... TArgs>
		T& DeclareStyledKeyed(SdUtf8StringView key, SdStyleIdentity styleIdentity, const typename T::Style* inlineStyle, TArgs&&... args);

		template<SdDeclarableWidget TWidget, class TModel = typename TWidget::Model>
		TModel& Model(SdUtf8StringView key);

		template<SdDeclarableWidget TWidget, class TConfigure, class TModel = typename TWidget::Model>
		void ConfigureModel(SdUtf8StringView key, TConfigure&& configure);

		SdInstance& GetInstance() noexcept { return instance; }
	};
}
