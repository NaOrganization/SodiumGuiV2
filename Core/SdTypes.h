#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>

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
}
