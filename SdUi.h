#pragma once

#include "SdRuntimeStorage.h"

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
		SdWidgetId CurrentParentId() const noexcept;
		void BeginDeclarationFrame();

	public:
		explicit SdUi(SdInstance& owner);

		template<SdDeclarableWidget T, class... TArgs>
		T& Declare(TArgs&&... args);

		SdInstance& GetInstance() noexcept { return instance; }
	};
}
