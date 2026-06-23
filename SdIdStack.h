#pragma once

#include "SdHash.h"

#include <cassert>
#include <unordered_map>
#include <vector>

namespace Sodium
{
	class SdIdStack final
	{
	private:
		std::vector<SdWidgetId> parentStack = {};
		std::unordered_map<SdWidgetId, SdUInt64> nextOrdinalByParent = {};

	public:
		SdIdStack()
		{
			parentStack.reserve(32);
		}

		void BeginFrame()
		{
			parentStack.clear();
			nextOrdinalByParent.clear();
		}

		SdWidgetId CurrentParentId() const noexcept
		{
			return parentStack.empty() ? 0 : parentStack.back();
		}

		void PushParent(SdWidgetId id)
		{
			parentStack.push_back(id);
		}

		void PopParent()
		{
			assert(!parentStack.empty());
			parentStack.pop_back();
		}

		SdWidgetId ResolveAnonymousWidgetId(SdUInt64 typeId)
		{
			const SdWidgetId parentId = CurrentParentId();
			SdUInt64& ordinal = nextOrdinalByParent[parentId];
			++ordinal;

			SdHashBuilder hash;
			hash.Add(parentId);
			hash.Add(typeId);
			hash.Add(ordinal);
			return hash.Finish();
		}

		SdWidgetId ResolveKeyedWidgetId(SdUInt64 typeId, SdUtf8StringView key, SdResolvedKey& resolvedKey) const
		{
			static constexpr SdUInt64 ResolvedKeySalt = 0xA0761D6478BD642Full;
			const SdWidgetId parentId = CurrentParentId();
			const SdUInt64 keyHash = SdHashString64(key);

			SdHashBuilder keyBuilder;
			keyBuilder.Add(ResolvedKeySalt);
			keyBuilder.Add(parentId);
			keyBuilder.Add(typeId);
			keyBuilder.Add(keyHash);
			resolvedKey = keyBuilder.Finish();

			SdHashBuilder widgetBuilder;
			widgetBuilder.Add(parentId);
			widgetBuilder.Add(typeId);
			widgetBuilder.Add(keyHash);
			return widgetBuilder.Finish();
		}

		SdResolvedKey ResolveModelKey(SdUInt64 typeId, SdUtf8StringView key) const
		{
			static constexpr SdUInt64 ResolvedKeySalt = 0xA0761D6478BD642Full;
			SdHashBuilder hash;
			hash.Add(ResolvedKeySalt);
			hash.Add(CurrentParentId());
			hash.Add(typeId);
			hash.Add(SdHashString64(key));
			return hash.Finish();
		}
	};
}
