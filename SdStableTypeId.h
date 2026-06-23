#pragma once

#include "SdHash.h"

namespace Sodium
{
	using SdTypeId = SdUInt64;

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
	consteval SdTypeId SdStableTypeId()
	{
		if constexpr (requires { T::TypeId; })
			return static_cast<SdTypeId>(T::TypeId);
		else
			return Detail::SdCompilerTypeNameHash<T>();
	}
}
