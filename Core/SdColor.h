#pragma once

#include "Core/SdMath.h"
#include "Core/SdTypes.h"

#include <algorithm>
#include <cmath>

namespace Sodium
{
	struct SdColorRgba;

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

	struct SdColorHsl final
	{
		float h = 0.0f;
		float s = 0.0f;
		float l = 1.0f;
		float a = 1.0f;

		constexpr SdColorHsl() = default;
		constexpr SdColorHsl(float hueDegrees, float saturation, float lightness, float alpha = 1.0f)
			: h(hueDegrees), s(saturation), l(lightness), a(alpha) {}

		SdColorRgba ToRgba() const noexcept;
	};

	struct SdColorHsb final
	{
		float h = 0.0f;
		float s = 0.0f;
		float b = 1.0f;
		float a = 1.0f;

		constexpr SdColorHsb() = default;
		constexpr SdColorHsb(float hueDegrees, float saturation, float brightness, float alpha = 1.0f)
			: h(hueDegrees), s(saturation), b(brightness), a(alpha) {}

		SdColorRgba ToRgba() const noexcept;
	};

	struct SdColorOklch final
	{
		float l = 1.0f;
		float c = 0.0f;
		float h = 0.0f;
		float a = 1.0f;

		constexpr SdColorOklch() = default;
		constexpr SdColorOklch(float lightness, float chroma, float hueDegrees, float alpha = 1.0f)
			: l(lightness), c(chroma), h(hueDegrees), a(alpha) {}

		SdColorRgba ToRgba() const noexcept;
	};

	namespace ColorDetail
	{
		constexpr float ByteToUnit(SdUInt8 value) noexcept
		{
			return static_cast<float>(value) / 255.0f;
		}

		constexpr SdUInt8 UnitToByte(float value) noexcept
		{
			return static_cast<SdUInt8>(SdSaturate(value) * 255.0f + 0.5f);
		}

		inline float WrapHue(float hue) noexcept
		{
			if (!std::isfinite(hue))
				return 0.0f;
			float result = std::fmod(hue, 360.0f);
			if (result < 0.0f)
				result += 360.0f;
			return result;
		}

		inline float HueToRgb(float p, float q, float t) noexcept
		{
			if (t < 0.0f)
				t += 1.0f;
			if (t > 1.0f)
				t -= 1.0f;
			if (t < 1.0f / 6.0f)
				return p + (q - p) * 6.0f * t;
			if (t < 1.0f / 2.0f)
				return q;
			if (t < 2.0f / 3.0f)
				return p + (q - p) * (2.0f / 3.0f - t) * 6.0f;
			return p;
		}

		inline float RgbHue(float red, float green, float blue, float maxValue, float delta) noexcept
		{
			if (delta <= 0.0f)
				return 0.0f;

			float hue = 0.0f;
			if (maxValue == red)
				hue = 60.0f * std::fmod((green - blue) / delta, 6.0f);
			else if (maxValue == green)
				hue = 60.0f * (((blue - red) / delta) + 2.0f);
			else
				hue = 60.0f * (((red - green) / delta) + 4.0f);

			return hue < 0.0f ? hue + 360.0f : hue;
		}

		inline float SrgbToLinear(float value) noexcept
		{
			value = SdSaturate(value);
			return value <= 0.04045f
				? value / 12.92f
				: std::pow((value + 0.055f) / 1.055f, 2.4f);
		}

		inline float LinearToSrgb(float value) noexcept
		{
			value = SdSaturate(value);
			return value <= 0.0031308f
				? value * 12.92f
				: 1.055f * std::pow(value, 1.0f / 2.4f) - 0.055f;
		}
	}

	struct SdColorRgba final
	{
		SdUInt8 r = 255;
		SdUInt8 g = 255;
		SdUInt8 b = 255;
		SdUInt8 a = 255;

		constexpr SdColorRgba() = default;
		constexpr SdColorRgba(SdUInt8 red, SdUInt8 green, SdUInt8 blue, SdUInt8 alpha = 255)
			: r(red), g(green), b(blue), a(alpha) {}

		constexpr SdUInt32 Pack() const noexcept
		{
			return static_cast<SdUInt32>(r)
				| (static_cast<SdUInt32>(g) << 8)
				| (static_cast<SdUInt32>(b) << 16)
				| (static_cast<SdUInt32>(a) << 24);
		}

		constexpr bool IsOpaque() const noexcept { return a == 255; }
		constexpr bool IsTransparent() const noexcept { return a == 0; }
		constexpr SdColorRgba WithAlpha(SdUInt8 alpha) const noexcept { return { r, g, b, alpha }; }
		constexpr SdColorRgba WithAlpha(float alpha) const noexcept { return { r, g, b, ColorDetail::UnitToByte(alpha) }; }

		constexpr SdColorLinear ToLinear() const noexcept
		{
			return {
				ColorDetail::ByteToUnit(r),
				ColorDetail::ByteToUnit(g),
				ColorDetail::ByteToUnit(b),
				ColorDetail::ByteToUnit(a)
			};
		}

		SdColorHsl ToHsl() const noexcept;
		SdColorHsb ToHsb() const noexcept;
		SdColorOklch ToOklch() const noexcept;

		static constexpr SdColorRgba FromLinear(const SdColorLinear& color) noexcept
		{
			return {
				ColorDetail::UnitToByte(color.r),
				ColorDetail::UnitToByte(color.g),
				ColorDetail::UnitToByte(color.b),
				ColorDetail::UnitToByte(color.a)
			};
		}

		static SdColorRgba FromHsl(const SdColorHsl& color) noexcept;
		static SdColorRgba FromHsb(const SdColorHsb& color) noexcept;
		static SdColorRgba FromOklch(const SdColorOklch& color) noexcept;

		friend constexpr bool operator==(const SdColorRgba&, const SdColorRgba&) = default;
	};

	using SdColor = SdColorRgba;

	inline SdColorRgba SdColorHsl::ToRgba() const noexcept
	{
		return SdColorRgba::FromHsl(*this);
	}

	inline SdColorRgba SdColorHsb::ToRgba() const noexcept
	{
		return SdColorRgba::FromHsb(*this);
	}

	inline SdColorRgba SdColorOklch::ToRgba() const noexcept
	{
		return SdColorRgba::FromOklch(*this);
	}

	inline SdColorRgba SdColorRgba::FromHsl(const SdColorHsl& color) noexcept
	{
		const float hue = ColorDetail::WrapHue(color.h) / 360.0f;
		const float saturation = SdSaturate(color.s);
		const float lightness = SdSaturate(color.l);
		const SdUInt8 alpha = ColorDetail::UnitToByte(color.a);

		if (saturation <= 0.0f)
		{
			const SdUInt8 gray = ColorDetail::UnitToByte(lightness);
			return { gray, gray, gray, alpha };
		}

		const float q = lightness < 0.5f
			? lightness * (1.0f + saturation)
			: lightness + saturation - lightness * saturation;
		const float p = 2.0f * lightness - q;
		return {
			ColorDetail::UnitToByte(ColorDetail::HueToRgb(p, q, hue + 1.0f / 3.0f)),
			ColorDetail::UnitToByte(ColorDetail::HueToRgb(p, q, hue)),
			ColorDetail::UnitToByte(ColorDetail::HueToRgb(p, q, hue - 1.0f / 3.0f)),
			alpha
		};
	}

	inline SdColorHsl SdColorRgba::ToHsl() const noexcept
	{
		const float red = ColorDetail::ByteToUnit(r);
		const float green = ColorDetail::ByteToUnit(g);
		const float blue = ColorDetail::ByteToUnit(b);
		const float maxValue = std::max({ red, green, blue });
		const float minValue = std::min({ red, green, blue });
		const float delta = maxValue - minValue;
		const float lightness = (maxValue + minValue) * 0.5f;
		const float saturation = delta <= 0.0f
			? 0.0f
			: delta / (1.0f - std::fabs(2.0f * lightness - 1.0f));

		return {
			ColorDetail::RgbHue(red, green, blue, maxValue, delta),
			saturation,
			lightness,
			ColorDetail::ByteToUnit(a)
		};
	}

	inline SdColorRgba SdColorRgba::FromHsb(const SdColorHsb& color) noexcept
	{
		const float hue = ColorDetail::WrapHue(color.h);
		const float saturation = SdSaturate(color.s);
		const float brightness = SdSaturate(color.b);
		const float chroma = brightness * saturation;
		const float huePrime = hue / 60.0f;
		const float x = chroma * (1.0f - std::fabs(std::fmod(huePrime, 2.0f) - 1.0f));
		const float match = brightness - chroma;
		float red = 0.0f;
		float green = 0.0f;
		float blue = 0.0f;

		if (hue < 60.0f)
		{
			red = chroma;
			green = x;
		}
		else if (hue < 120.0f)
		{
			red = x;
			green = chroma;
		}
		else if (hue < 180.0f)
		{
			green = chroma;
			blue = x;
		}
		else if (hue < 240.0f)
		{
			green = x;
			blue = chroma;
		}
		else if (hue < 300.0f)
		{
			red = x;
			blue = chroma;
		}
		else
		{
			red = chroma;
			blue = x;
		}

		return {
			ColorDetail::UnitToByte(red + match),
			ColorDetail::UnitToByte(green + match),
			ColorDetail::UnitToByte(blue + match),
			ColorDetail::UnitToByte(color.a)
		};
	}

	inline SdColorHsb SdColorRgba::ToHsb() const noexcept
	{
		const float red = ColorDetail::ByteToUnit(r);
		const float green = ColorDetail::ByteToUnit(g);
		const float blue = ColorDetail::ByteToUnit(b);
		const float maxValue = std::max({ red, green, blue });
		const float minValue = std::min({ red, green, blue });
		const float delta = maxValue - minValue;

		return {
			ColorDetail::RgbHue(red, green, blue, maxValue, delta),
			maxValue <= 0.0f ? 0.0f : delta / maxValue,
			maxValue,
			ColorDetail::ByteToUnit(a)
		};
	}

	inline SdColorOklch SdColorRgba::ToOklch() const noexcept
	{
		const float red = ColorDetail::SrgbToLinear(ColorDetail::ByteToUnit(r));
		const float green = ColorDetail::SrgbToLinear(ColorDetail::ByteToUnit(g));
		const float blue = ColorDetail::SrgbToLinear(ColorDetail::ByteToUnit(b));

		const float coneL = 0.4122214708f * red + 0.5363325363f * green + 0.0514459929f * blue;
		const float coneM = 0.2119034982f * red + 0.6806995451f * green + 0.1073969566f * blue;
		const float coneS = 0.0883024619f * red + 0.2817188376f * green + 0.6299787005f * blue;

		const float cubeRootL = std::cbrt(coneL);
		const float cubeRootM = std::cbrt(coneM);
		const float cubeRootS = std::cbrt(coneS);

		const float okL = 0.2104542553f * cubeRootL + 0.7936177850f * cubeRootM - 0.0040720468f * cubeRootS;
		const float okA = 1.9779984951f * cubeRootL - 2.4285922050f * cubeRootM + 0.4505937099f * cubeRootS;
		const float okB = 0.0259040371f * cubeRootL + 0.7827717662f * cubeRootM - 0.8086757660f * cubeRootS;
		float hue = SdDegrees(std::atan2(okB, okA));
		if (hue < 0.0f)
			hue += 360.0f;

		return {
			okL,
			std::sqrt(okA * okA + okB * okB),
			hue,
			ColorDetail::ByteToUnit(a)
		};
	}

	inline SdColorRgba SdColorRgba::FromOklch(const SdColorOklch& color) noexcept
	{
		const float lightness = SdSaturate(color.l);
		const float chroma = SdMax(0.0f, color.c);
		const float hue = SdRadians(ColorDetail::WrapHue(color.h));
		const float okA = chroma * std::cos(hue);
		const float okB = chroma * std::sin(hue);

		const float cubeRootL = lightness + 0.3963377774f * okA + 0.2158037573f * okB;
		const float cubeRootM = lightness - 0.1055613458f * okA - 0.0638541728f * okB;
		const float cubeRootS = lightness - 0.0894841775f * okA - 1.2914855480f * okB;

		const float coneL = cubeRootL * cubeRootL * cubeRootL;
		const float coneM = cubeRootM * cubeRootM * cubeRootM;
		const float coneS = cubeRootS * cubeRootS * cubeRootS;

		const float red = 4.0767416621f * coneL - 3.3077115913f * coneM + 0.2309699292f * coneS;
		const float green = -1.2684380046f * coneL + 2.6097574011f * coneM - 0.3413193965f * coneS;
		const float blue = -0.0041960863f * coneL - 0.7034186147f * coneM + 1.7076147010f * coneS;

		return {
			ColorDetail::UnitToByte(ColorDetail::LinearToSrgb(red)),
			ColorDetail::UnitToByte(ColorDetail::LinearToSrgb(green)),
			ColorDetail::UnitToByte(ColorDetail::LinearToSrgb(blue)),
			ColorDetail::UnitToByte(color.a)
		};
	}

	namespace SdColors
	{
		inline constexpr SdColor Transparent = SdColor(0, 0, 0, 0);
		inline constexpr SdColor Clear = Transparent;
		inline constexpr SdColor White = SdColor(255, 255, 255, 255);
		inline constexpr SdColor Black = SdColor(0, 0, 0, 255);
		inline constexpr SdColor LightGray = SdColor(229, 229, 229, 255);
		inline constexpr SdColor Gray = SdColor(128, 128, 128, 255);
		inline constexpr SdColor DarkGray = SdColor(64, 64, 64, 255);
		inline constexpr SdColor Red = SdColor(244, 67, 54, 255);
		inline constexpr SdColor Orange = SdColor(255, 152, 0, 255);
		inline constexpr SdColor Amber = SdColor(255, 193, 7, 255);
		inline constexpr SdColor Yellow = SdColor(255, 235, 59, 255);
		inline constexpr SdColor Lime = SdColor(205, 220, 57, 255);
		inline constexpr SdColor Green = SdColor(76, 175, 80, 255);
		inline constexpr SdColor Teal = SdColor(0, 150, 136, 255);
		inline constexpr SdColor Cyan = SdColor(0, 188, 212, 255);
		inline constexpr SdColor Sky = SdColor(3, 169, 244, 255);
		inline constexpr SdColor Blue = SdColor(33, 150, 243, 255);
		inline constexpr SdColor Indigo = SdColor(63, 81, 181, 255);
		inline constexpr SdColor Purple = SdColor(156, 39, 176, 255);
		inline constexpr SdColor Pink = SdColor(233, 30, 99, 255);
	}

	enum class SdColorPreset
	{
		Transparent,
		White,
		Black,
		LightGray,
		Gray,
		DarkGray,
		Red,
		Orange,
		Amber,
		Yellow,
		Lime,
		Green,
		Teal,
		Cyan,
		Sky,
		Blue,
		Indigo,
		Purple,
		Pink
	};

	constexpr SdColor SdGetColorPreset(SdColorPreset preset) noexcept
	{
		switch (preset)
		{
		case SdColorPreset::Transparent:
			return SdColors::Transparent;
		case SdColorPreset::White:
			return SdColors::White;
		case SdColorPreset::Black:
			return SdColors::Black;
		case SdColorPreset::LightGray:
			return SdColors::LightGray;
		case SdColorPreset::Gray:
			return SdColors::Gray;
		case SdColorPreset::DarkGray:
			return SdColors::DarkGray;
		case SdColorPreset::Red:
			return SdColors::Red;
		case SdColorPreset::Orange:
			return SdColors::Orange;
		case SdColorPreset::Amber:
			return SdColors::Amber;
		case SdColorPreset::Yellow:
			return SdColors::Yellow;
		case SdColorPreset::Lime:
			return SdColors::Lime;
		case SdColorPreset::Green:
			return SdColors::Green;
		case SdColorPreset::Teal:
			return SdColors::Teal;
		case SdColorPreset::Cyan:
			return SdColors::Cyan;
		case SdColorPreset::Sky:
			return SdColors::Sky;
		case SdColorPreset::Blue:
			return SdColors::Blue;
		case SdColorPreset::Indigo:
			return SdColors::Indigo;
		case SdColorPreset::Purple:
			return SdColors::Purple;
		case SdColorPreset::Pink:
			return SdColors::Pink;
		default:
			return SdColors::Transparent;
		}
	}

	inline constexpr SdColor SdColorWhite = SdColors::White;
	inline constexpr SdColor SdColorBlack = SdColors::Black;
	inline constexpr SdColor SdColorTransparent = SdColors::Transparent;
}
