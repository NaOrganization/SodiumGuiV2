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

	public:
		explicit SdUi(SdInstance& owner);

		template<SdDeclarableWidget T, class... TArgs>
		T& Declare(TArgs&&... args);

		template<SdDeclarableWidget T, class... TArgs>
		T& DeclareKeyed(SdUtf8StringView key, TArgs&&... args);

		template<SdDeclarableWidget TWidget, class TModel = typename TWidget::Model>
		TModel& Model(SdUtf8StringView key);

		template<SdDeclarableWidget TWidget, class TConfigure, class TModel = typename TWidget::Model>
		void ConfigureModel(SdUtf8StringView key, TConfigure&& configure);

		SdInstance& GetInstance() noexcept { return instance; }
	};
}
