#pragma once

#include "Core/SdCore.h"

namespace Sodium
{
	inline constexpr SdEffectTypeId SdBlurEffectTypeId = 0x5364456666656371ull;
	inline constexpr SdEffectTypeId SdBackdropBlurEffectTypeId = 0x5364456666656372ull;
	inline constexpr SdEffectTypeId SdDropShadowEffectTypeId = 0x5364456666656373ull;
	inline constexpr SdEffectTypeId SdInnerShadowEffectTypeId = 0x5364456666656374ull;
	inline constexpr SdEffectTypeId SdMaskEffectTypeId = 0x5364456666656375ull;
	inline constexpr SdEffectTypeId SdCustomEffectTypeId = 0x5364456666656376ull;

	struct SdBuiltInEffectHandles final
	{
		SdEffectHandle blur = SdEffectHandle(1, 1);
		SdEffectHandle backdropBlur = SdEffectHandle(2, 1);
		SdEffectHandle dropShadow = SdEffectHandle(3, 1);
		SdEffectHandle innerShadow = SdEffectHandle(4, 1);
		SdEffectHandle mask = SdEffectHandle(5, 1);
	};
}
