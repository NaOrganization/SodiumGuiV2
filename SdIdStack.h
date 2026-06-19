#pragma once

#include "SdCore.h"

#include <cassert>
#include <functional>
#include <unordered_map>
#include <vector>

namespace Sodium
{
	class SdIdStack final
	{
	private:
		std::vector<SdWidgetId> parentStack = {};
		std::unordered_map<SdWidgetId, SdUInt32> nextOrdinalByParent = {};

		static SdUInt64 HashCombine(SdUInt64 seed, SdUInt64 value) noexcept
		{
			return seed ^ (value + 0x9E3779B97F4A7C15ull + (seed << 6) + (seed >> 2));
		}

		static SdUInt64 NormalizeHash(SdUInt64 value) noexcept
		{
			return value == 0 ? 1 : value;
		}

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

		SdWidgetId ResolveAnonymousWidgetId(SdUInt64 typeHash)
		{
			const SdWidgetId parentId = CurrentParentId();
			SdUInt32& ordinal = nextOrdinalByParent[parentId];
			++ordinal;

			SdUInt64 seed = 1469598103934665603ull;
			seed = HashCombine(seed, parentId);
			seed = HashCombine(seed, typeHash);
			seed = HashCombine(seed, ordinal);
			return NormalizeHash(seed);
		}

		SdWidgetId ResolveKeyedWidgetId(SdUInt64 typeHash, SdUtf8StringView key, SdResolvedKey& resolvedKey) const
		{
			const SdWidgetId parentId = CurrentParentId();
			SdUInt64 keyHash = std::hash<SdUtf8StringView>{}(key);
			keyHash = NormalizeHash(keyHash);

			SdUInt64 keySeed = 1469598103934665603ull;
			keySeed = HashCombine(keySeed, parentId);
			keySeed = HashCombine(keySeed, keyHash);
			resolvedKey = NormalizeHash(keySeed);

			SdUInt64 seed = 1469598103934665603ull;
			seed = HashCombine(seed, parentId);
			seed = HashCombine(seed, typeHash);
			seed = HashCombine(seed, keyHash);
			return NormalizeHash(seed);
		}

		SdResolvedKey ResolveModelKey(SdUInt64 typeHash, SdUtf8StringView key) const
		{
			(void)typeHash;
			SdUInt64 keyHash = std::hash<SdUtf8StringView>{}(key);
			keyHash = NormalizeHash(keyHash);

			SdUInt64 keySeed = 1469598103934665603ull;
			keySeed = HashCombine(keySeed, CurrentParentId());
			keySeed = HashCombine(keySeed, keyHash);
			return NormalizeHash(keySeed);
		}
	};
}
