#pragma once

#include "SdWidgetContext.h"

#include <typeindex>
#include <utility>

namespace Sodium::Detail
{
	inline SdUInt64 SdHashCombine(SdUInt64 seed, SdUInt64 value) noexcept
	{
		return seed ^ (value + 0x9E3779B97F4A7C15ull + (seed << 6) + (seed >> 2));
	}

	template<class T>
	SdUInt64 SdTypeHash() noexcept
	{
		static const int anchor = 0;
		return static_cast<SdUInt64>(reinterpret_cast<std::uintptr_t>(&anchor));
	}

	struct SdAnyObject final
	{
		void* value = nullptr;
		void(*deleter)(void*) = nullptr;
		std::type_index type = std::type_index(typeid(void));

		SdAnyObject() = default;
		SdAnyObject(const SdAnyObject&) = delete;
		SdAnyObject& operator=(const SdAnyObject&) = delete;

		SdAnyObject(SdAnyObject&& other) noexcept
			: value(other.value), deleter(other.deleter), type(other.type)
		{
			other.value = nullptr;
			other.deleter = nullptr;
			other.type = std::type_index(typeid(void));
		}

		SdAnyObject& operator=(SdAnyObject&& other) noexcept
		{
			if (this != &other)
			{
				Reset();
				value = other.value;
				deleter = other.deleter;
				type = other.type;
				other.value = nullptr;
				other.deleter = nullptr;
				other.type = std::type_index(typeid(void));
			}
			return *this;
		}

		~SdAnyObject()
		{
			Reset();
		}

		void Reset()
		{
			if (value && deleter)
				deleter(value);
			value = nullptr;
			deleter = nullptr;
			type = std::type_index(typeid(void));
		}

		template<class T, class... TArgs>
		T& Create(TArgs&&... args)
		{
			Reset();
			value = new T(std::forward<TArgs>(args)...);
			deleter = [](void* pointer) { delete static_cast<T*>(pointer); };
			type = std::type_index(typeid(T));
			return *static_cast<T*>(value);
		}

		template<class T>
		T* Get() noexcept
		{
			if (type != std::type_index(typeid(T)))
				return nullptr;
			return static_cast<T*>(value);
		}
	};
}
