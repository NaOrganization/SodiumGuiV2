#pragma once

#include "Core/SdHash.h"

namespace Sodium
{
	struct SdTypeTag final {};
	using SdTypeId = SdId<SdTypeTag>;

	namespace Detail
	{
		template<class T>
		consteval SdTypeId SdCompilerTypeNameHash()
		{
#if defined(_MSC_VER)
			return SdHashCString64(__FUNCSIG__);
#else
			return SdHashCString64(__PRETTY_FUNCTION__);
#endif
		}
	}

	template<class T>
	consteval SdUInt64 SdStableTypeId()
	{
		if constexpr (requires { T::TypeId; })
			return static_cast<SdTypeId>(T::TypeId).value;
		else
			return Detail::SdCompilerTypeNameHash<T>().value;
	}
}
