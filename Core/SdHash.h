#pragma once

#include "Core/SdCore.h"

namespace Sodium
{
	inline constexpr SdUInt64 SdHashOffsetBasis64 = 14695981039346656037ull;
	inline constexpr SdUInt64 SdHashPrime64 = 1099511628211ull;

	constexpr SdUInt64 SdMix64(SdUInt64 value) noexcept
	{
		value ^= value >> 30;
		value *= 0xbf58476d1ce4e5b9ull;
		value ^= value >> 27;
		value *= 0x94d049bb133111ebull;
		value ^= value >> 31;
		return value == 0 ? 1 : value;
	}

	constexpr SdUInt64 SdHashCString64(const char* text) noexcept
	{
		SdUInt64 hash = SdHashOffsetBasis64;
		while (*text != '\0')
		{
			hash ^= static_cast<SdUInt8>(*text);
			hash *= SdHashPrime64;
			++text;
		}
		return SdMix64(hash);
	}

	constexpr SdUInt64 SdHashString64(SdUtf8StringView text) noexcept
	{
		SdUInt64 hash = SdHashOffsetBasis64;
		for (char value : text)
		{
			hash ^= static_cast<SdUInt8>(value);
			hash *= SdHashPrime64;
		}
		return SdMix64(hash);
	}

	class SdHashBuilder final
	{
	private:
		SdUInt64 seed = SdHashOffsetBasis64;

	public:
		constexpr void Add(SdUInt64 value) noexcept
		{
			seed = SdMix64(seed ^ SdMix64(value + 0x9E3779B97F4A7C15ull));
		}

		template<typename TTag>
		constexpr void Add(SdId<TTag> id) noexcept
		{
			Add(id.value);
		}

		constexpr SdUInt64 Finish() const noexcept
		{
			return SdMix64(seed);
		}
	};
}
