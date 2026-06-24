#pragma once

#include "Core/SdIdStack.h"
#include "Core/SdRuntimeStorage.h"

#include <functional>
#include <string_view>
#include <type_traits>
#include <vector>

namespace Sodium
{
	namespace Detail
	{
		template<class TStyle, class... TArgs>
		inline constexpr bool SdFirstArgumentIsStylePointer = false;

		template<class TStyle, class TFirst, class... TRest>
		inline constexpr bool SdFirstArgumentIsStylePointer<TStyle, TFirst, TRest...> =
			std::is_convertible_v<std::remove_reference_t<TFirst>, const TStyle*>;
	}

	class SdUi final
	{
	private:
		friend class SdInstance;

		struct SdPortalFrame final
		{
			SdPortalRoot root = SdPortalRoot::None;
			SdWidgetId ownerWidgetId = 0;
			SdWidgetId anchorWidgetId = 0;
		};

		SdInstance& instance;
		SdIdStack idStack = {};
		std::vector<SdPortalFrame> portalStack = {};
		void BeginDeclarationFrame();
		SdPortalFrame CurrentPortalFrame() const noexcept;

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
			requires (!Detail::SdFirstArgumentIsStylePointer<typename T::Style, TArgs...>)
		T& DeclareStyled(SdStyleIdentity styleIdentity, TArgs&&... args);

		template<SdStylableWidget T, class... TArgs>
		T& DeclareStyled(SdStyleIdentity styleIdentity, const typename T::Style* inlineStyle, TArgs&&... args);

		template<SdStylableWidget T, class... TArgs>
		T& DeclareStyledKeyed(SdUtf8StringView key, const typename T::Style* inlineStyle, TArgs&&... args);

		template<SdStylableWidget T, class... TArgs>
			requires (!Detail::SdFirstArgumentIsStylePointer<typename T::Style, TArgs...>)
		T& DeclareStyledKeyed(SdUtf8StringView key, SdStyleIdentity styleIdentity, TArgs&&... args);

		template<SdStylableWidget T, class... TArgs>
		T& DeclareStyledKeyed(SdUtf8StringView key, SdStyleIdentity styleIdentity, const typename T::Style* inlineStyle, TArgs&&... args);

		template<SdDeclarableWidget TWidget, class TModel = typename TWidget::Model>
		TModel& Model(SdUtf8StringView key);

		template<SdDeclarableWidget TWidget, class TModel = typename TWidget::Model>
		TModel& Model(SdUtf8StringView key, SdModelLifetime lifetime);

		template<SdDeclarableWidget TWidget, class TModel = typename TWidget::Model>
		TModel& WidgetModel(SdUtf8StringView key);

		template<SdDeclarableWidget TWidget, class TModel = typename TWidget::Model>
		TModel& ScopeModel(SdUtf8StringView key);

		template<SdDeclarableWidget TWidget, class TModel = typename TWidget::Model>
		TModel& GlobalModel(SdUtf8StringView key);

		template<SdDeclarableWidget TWidget, class TConfigure, class TModel = typename TWidget::Model>
		void ConfigureModel(SdUtf8StringView key, TConfigure&& configure);

		template<SdDeclarableWidget TWidget, class TConfigure, class TModel = typename TWidget::Model>
		void ConfigureModel(SdUtf8StringView key, SdModelLifetime lifetime, TConfigure&& configure);

		void BeginPortal(SdPortalRoot root, SdWidgetId ownerWidgetId = 0, SdWidgetId anchorWidgetId = 0);
		void EndPortal();

		SdInstance& GetInstance() noexcept { return instance; }
	};
}
