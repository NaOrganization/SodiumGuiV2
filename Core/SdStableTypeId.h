#pragma once

#include "Core/SdHash.h"

namespace Sodium
{
	namespace Detail
	{
		template<class T>
		consteval SdStyleTargetId SdCompilerTypeNameHash()
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
			return static_cast<SdStyleTargetId>(T::TypeId).value;
		else
			return Detail::SdCompilerTypeNameHash<T>().value;
	}
}
