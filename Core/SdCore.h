#pragma once

#include <array>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#ifndef SODIUM_STRING
#define SODIUM_STRING(value) value
#endif

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

namespace Sodium
{
	using namespace std::chrono_literals;

	using SdUInt8 = std::uint8_t;
	using SdUInt16 = std::uint16_t;
	using SdUInt32 = std::uint32_t;
	using SdUInt64 = std::uint64_t;
	using SdInt32 = std::int32_t;
	using SdSize = std::size_t;

	using SdDuration = std::chrono::nanoseconds;
	using SdTimePoint = std::chrono::time_point<std::chrono::steady_clock, SdDuration>;
	using SdWidgetId = std::uint64_t;
	using SdResolvedKey = std::uint64_t;
	using SdLayerId = std::uint32_t;
	using SdFrameIndex = std::uint64_t;

	using SdUtf8String = std::string;
	using SdUtf8StringView = std::string_view;
	using SdPath = std::filesystem::path;
	template<typename T>
	using SdSpan = std::span<T>;

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

	inline constexpr float SdPIf = 3.14159265358979323846f;
	inline constexpr float SdPi = SdPIf;

	struct SdVec2 final
	{
		float x = 0.0f;
		float y = 0.0f;

		constexpr SdVec2() = default;
		constexpr SdVec2(float valueX, float valueY) : x(valueX), y(valueY) {}

		constexpr SdVec2 operator+(const SdVec2& other) const { return { x + other.x, y + other.y }; }
		constexpr SdVec2 operator-(const SdVec2& other) const { return { x - other.x, y - other.y }; }
		constexpr SdVec2 operator*(float scalar) const { return { x * scalar, y * scalar }; }
		constexpr SdVec2 operator/(float scalar) const { return { x / scalar, y / scalar }; }
		constexpr SdVec2& operator+=(const SdVec2& other) { x += other.x; y += other.y; return *this; }
		constexpr SdVec2& operator-=(const SdVec2& other) { x -= other.x; y -= other.y; return *this; }
		friend constexpr bool operator==(const SdVec2&, const SdVec2&) = default;
	};

	struct SdVec4 final
	{
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;
		float w = 0.0f;

		constexpr SdVec4() = default;
		constexpr SdVec4(float valueX, float valueY, float valueZ, float valueW)
			: x(valueX), y(valueY), z(valueZ), w(valueW) {}
	};

	// Corner order: top-left, top-right, bottom-right, bottom-left.
	struct SdCornerRadii final
	{
		float topLeft = 0.0f;
		float topRight = 0.0f;
		float bottomRight = 0.0f;
		float bottomLeft = 0.0f;

		constexpr SdCornerRadii() = default;
		constexpr SdCornerRadii(float valueTopLeft, float valueTopRight, float valueBottomRight, float valueBottomLeft)
			: topLeft(valueTopLeft), topRight(valueTopRight), bottomRight(valueBottomRight), bottomLeft(valueBottomLeft) {}

		friend constexpr bool operator==(const SdCornerRadii&, const SdCornerRadii&) = default;
	};

	struct SdMat3 final
	{
		float m[3][3] =
		{
			{ 1.0f, 0.0f, 0.0f },
			{ 0.0f, 1.0f, 0.0f },
			{ 0.0f, 0.0f, 1.0f }
		};

		constexpr SdMat3() = default;
		constexpr SdMat3(
			float m00, float m01, float m02,
			float m10, float m11, float m12,
			float m20, float m21, float m22)
			: m{
				{ m00, m01, m02 },
				{ m10, m11, m12 },
				{ m20, m21, m22 }
			} {}

		static constexpr SdMat3 Identity()
		{
			return {};
		}

		static constexpr SdMat3 Translation(const SdVec2& value)
		{
			return {
				1.0f, 0.0f, 0.0f,
				0.0f, 1.0f, 0.0f,
				value.x, value.y, 1.0f
			};
		}

		static constexpr SdMat3 Scale(const SdVec2& value)
		{
			return {
				value.x, 0.0f, 0.0f,
				0.0f, value.y, 0.0f,
				0.0f, 0.0f, 1.0f
			};
		}

		constexpr SdVec2 TransformPoint(const SdVec2& point) const
		{
			return {
				point.x * m[0][0] + point.y * m[1][0] + m[2][0],
				point.x * m[0][1] + point.y * m[1][1] + m[2][1]
			};
		}
	};

	struct SdRect final
	{
		SdVec2 min = {};
		SdVec2 max = {};

		constexpr SdRect() = default;
		constexpr SdRect(const SdVec2& minimum, const SdVec2& maximum) : min(minimum), max(maximum) {}
		constexpr SdRect(float left, float top, float right, float bottom) : min(left, top), max(right, bottom) {}

		constexpr float Width() const { return max.x - min.x; }
		constexpr float Height() const { return max.y - min.y; }
		constexpr SdVec2 Size() const { return { Width(), Height() }; }
		constexpr bool Contains(const SdVec2& point) const
		{
			return point.x >= min.x && point.y >= min.y && point.x <= max.x && point.y <= max.y;
		}
	};

	struct SdColorLinear final
	{
		float r = 1.0f;
		float g = 1.0f;
		float b = 1.0f;
		float a = 1.0f;

		constexpr SdColorLinear() = default;
		constexpr SdColorLinear(float red, float green, float blue, float alpha = 1.0f)
			: r(red), g(green), b(blue), a(alpha) {}
	};

	struct SdColorRgba final
	{
		SdUInt8 r = 255;
		SdUInt8 g = 255;
		SdUInt8 b = 255;
		SdUInt8 a = 255;

		constexpr SdColorRgba() = default;
		constexpr SdColorRgba(SdUInt8 red, SdUInt8 green, SdUInt8 blue, SdUInt8 alpha = 255)
			: r(red), g(green), b(blue), a(alpha) {}

		constexpr SdUInt32 Pack() const
		{
			return static_cast<SdUInt32>(r)
				| (static_cast<SdUInt32>(g) << 8)
				| (static_cast<SdUInt32>(b) << 16)
				| (static_cast<SdUInt32>(a) << 24);
		}

		constexpr SdColorLinear ToLinear() const
		{
			return {
				static_cast<float>(r) / 255.0f,
				static_cast<float>(g) / 255.0f,
				static_cast<float>(b) / 255.0f,
				static_cast<float>(a) / 255.0f
			};
		}

		friend constexpr bool operator==(const SdColorRgba&, const SdColorRgba&) = default;
	};

	using SdColor = SdColorRgba;

	inline constexpr SdColor SdColorWhite = SdColor(255, 255, 255, 255);
	inline constexpr SdColor SdColorBlack = SdColor(0, 0, 0, 255);
	inline constexpr SdColor SdColorTransparent = SdColor(0, 0, 0, 0);

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
