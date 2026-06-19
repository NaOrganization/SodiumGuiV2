#pragma once

#include "SdRuntimeStorage.h"

#include <functional>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Sodium
{
	class SdUi final
	{
	private:
		friend class SdInstance;

		SdInstance& instance;
		std::vector<SdWidgetId> parentStack = {};
		std::unordered_map<SdWidgetId, SdUInt32> nextOrdinalByParent = {};

		SdWidgetId ResolveWidgetId(SdUInt64 typeHash);
		SdWidgetId ResolveKeyedWidgetId(SdUInt64 typeHash, SdUtf8StringView key, SdResolvedKey& resolvedKey);
		SdResolvedKey ResolveModelKey(SdUInt64 typeHash, SdUtf8StringView key) const;
		SdWidgetId CurrentParentId() const noexcept;
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
