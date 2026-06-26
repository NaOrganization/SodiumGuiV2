#pragma once

#include "Core/SdColor.h"
#include "Core/SdMath.h"
#include "Core/SdTypes.h"

#include <array>
#include <cassert>
#include <functional>
#include <limits>
#include <type_traits>
#include <utility>
#include <vector>

namespace Sodium
{
	template<typename T>
	struct SdRange final
	{
		T begin = {};
		T end = {};

		constexpr SdRange() = default;
		constexpr SdRange(T beginValue, T endValue) : begin(beginValue), end(endValue) {}

		constexpr bool IsEmpty() const { return begin == end; }
		constexpr T Count() const { return end - begin; }
	};

	template<SdSize N>
	class SdFixedBitset final
	{
	private:
		static constexpr SdSize BitsPerWord = 64;
		static constexpr SdSize WordCount = (N + BitsPerWord - 1) / BitsPerWord;
		std::array<SdUInt64, WordCount> words = {};

	public:
		constexpr void Clear()
		{
			for (SdUInt64& word : words)
				word = 0;
		}

		constexpr bool Test(SdSize index) const
		{
			assert(index < N);
			return (words[index / BitsPerWord] & (SdUInt64{ 1 } << (index % BitsPerWord))) != 0;
		}

		constexpr void Set(SdSize index, bool enabled = true)
		{
			assert(index < N);
			SdUInt64& word = words[index / BitsPerWord];
			const SdUInt64 mask = SdUInt64{ 1 } << (index % BitsPerWord);
			if (enabled)
				word |= mask;
			else
				word &= ~mask;
		}

		constexpr void Reset(SdSize index)
		{
			Set(index, false);
		}

		constexpr bool Any() const
		{
			for (SdUInt64 word : words)
			{
				if (word != 0)
					return true;
			}
			return false;
		}
	};

	template<typename T>
	constexpr T SdInvalidIndex = static_cast<T>(-1);

	template<typename TTag>
	struct SdHandle final
	{
		SdUInt32 index = 0;
		SdUInt32 generation = 0;

		constexpr SdHandle() = default;
		constexpr explicit SdHandle(SdUInt32 indexValue, SdUInt32 generationValue = 1)
			: index(indexValue), generation(generationValue) {}

		constexpr bool IsValid() const { return generation != 0; }
		friend constexpr bool operator==(const SdHandle&, const SdHandle&) = default;
	};

	template<typename TTag>
	struct SdId
	{
		SdUInt64 value = 0;
		constexpr SdId() noexcept = default;
		constexpr SdId(SdUInt64 value) noexcept : value(value) {}

		constexpr bool IsValid() const noexcept { return value != 0; }
		friend constexpr bool operator==(const SdId&, const SdId&) = default;
		friend constexpr bool operator!=(const SdId&, const SdId&) = default;

	};

	template <typename TTag>
	struct SdIdHash final
	{
		[[nodiscard]]
		constexpr std::size_t operator()(const SdId<TTag>& id) const noexcept
		{
			return std::hash<SdUInt64>{}(id.value);
		}
	};

	struct SdFontTag final {};
	struct SdFontFamilyTag final {};
	struct SdImageTag final {};
	struct SdStateTag final {};
	struct SdWidgetHandleTag final {};

	using SdFontHandle = SdHandle<SdFontTag>;
	using SdFontFamilyHandle = SdHandle<SdFontFamilyTag>;
	using SdImageHandle = SdHandle<SdImageTag>;
	using SdStateHandle = SdHandle<SdStateTag>;
	using SdWidgetHandle = SdHandle<SdWidgetHandleTag>;
}

namespace Sodium::Utf8
{
	inline constexpr SdUInt32 kReplacementCodepoint = 0xFFFDu;

	inline bool IsContinuation(unsigned char value) noexcept
	{
		return (value & 0xC0u) == 0x80u;
	}

	inline bool TryReadCodepoint(SdUtf8StringView text, SdSize& offset, SdUInt32& codepoint)
	{
		if (offset >= text.size())
			return false;

		const unsigned char first = static_cast<unsigned char>(text[offset]);
		if (first < 0x80u)
		{
			codepoint = first;
			++offset;
			return true;
		}

		const auto fail = [&]()
		{
			codepoint = kReplacementCodepoint;
			++offset;
			return false;
		};

		if ((first & 0xE0u) == 0xC0u)
		{
			if (offset + 1 >= text.size())
				return fail();
			const unsigned char b1 = static_cast<unsigned char>(text[offset + 1]);
			if (!IsContinuation(b1))
				return fail();
			const SdUInt32 value = ((first & 0x1Fu) << 6) | (b1 & 0x3Fu);
			if (value < 0x80u)
				return fail();
			codepoint = value;
			offset += 2;
			return true;
		}

		if ((first & 0xF0u) == 0xE0u)
		{
			if (offset + 2 >= text.size())
				return fail();
			const unsigned char b1 = static_cast<unsigned char>(text[offset + 1]);
			const unsigned char b2 = static_cast<unsigned char>(text[offset + 2]);
			if (!IsContinuation(b1) || !IsContinuation(b2))
				return fail();
			const SdUInt32 value = ((first & 0x0Fu) << 12) | ((b1 & 0x3Fu) << 6) | (b2 & 0x3Fu);
			if (value < 0x800u || (value >= 0xD800u && value <= 0xDFFFu))
				return fail();
			codepoint = value;
			offset += 3;
			return true;
		}

		if ((first & 0xF8u) == 0xF0u)
		{
			if (offset + 3 >= text.size())
				return fail();
			const unsigned char b1 = static_cast<unsigned char>(text[offset + 1]);
			const unsigned char b2 = static_cast<unsigned char>(text[offset + 2]);
			const unsigned char b3 = static_cast<unsigned char>(text[offset + 3]);
			if (!IsContinuation(b1) || !IsContinuation(b2) || !IsContinuation(b3))
				return fail();
			const SdUInt32 value = ((first & 0x07u) << 18) | ((b1 & 0x3Fu) << 12) | ((b2 & 0x3Fu) << 6) | (b3 & 0x3Fu);
			if (value < 0x10000u || value > 0x10FFFFu)
				return fail();
			codepoint = value;
			offset += 4;
			return true;
		}

		return fail();
	}

	inline bool IsValid(SdUtf8StringView text)
	{
		SdSize offset = 0;
		while (offset < text.size())
		{
			SdUInt32 codepoint = 0;
			if (!TryReadCodepoint(text, offset, codepoint))
				return false;
		}
		return true;
	}

	inline std::vector<SdUInt32> DecodeToCodepoints(SdUtf8StringView text)
	{
		std::vector<SdUInt32> codepoints;
		codepoints.reserve(text.size());
		SdSize offset = 0;
		while (offset < text.size())
		{
			SdUInt32 codepoint = 0;
			TryReadCodepoint(text, offset, codepoint);
			codepoints.push_back(codepoint);
		}
		return codepoints;
	}
}

namespace std
{
	template <typename TTag>
	struct hash<Sodium::SdId<TTag>>
	{
		[[nodiscard]]
		std::size_t operator()(const Sodium::SdId<TTag>& id) const noexcept
		{
			return Sodium::SdIdHash<TTag>{}(id);
		}
	};
}
