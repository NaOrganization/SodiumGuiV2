#pragma once

#include "SdAnimation.h"
#include "SdWidgetContext.h"

#include <cassert>
#include <string>
#include <typeindex>
#include <unordered_map>
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

namespace Sodium
{
	struct SdWidgetRecord final
	{
		SdWidgetState state = {};
		SdComputedStyle style = {};
		SdAnimationWidgetState animation = {};
		Detail::SdAnyObject widgetObject = {};
		std::unordered_map<std::type_index, Detail::SdAnyObject> userStates = {};
		std::type_index widgetType = std::type_index(typeid(void));
		SdWidgetId parentId = 0;
		SdUInt32 order = 0;
		SdResolvedKey resolvedKey = 0;
		SdUtf8String debugKey = {};
		SdStyleWidgetClass cachedStyleClass = SdStyleWidgetClass::Default;
		SdStyleInteractionState cachedStyleInteraction = SdStyleInteractionState::Normal;
		bool hasCachedStyle = false;
		void(*layoutCallback)(void*, SdLayoutContext&) = nullptr;
		void(*paintCallback)(void*, SdPaintContext&) = nullptr;

		bool HasResolvedKey() const noexcept
		{
			return resolvedKey != 0;
		}
	};

	struct SdStateStorageStats final
	{
		SdUInt32 createdWidgetCount = 0;
		SdUInt32 reusedWidgetCount = 0;
		SdUInt32 leavingWidgetCount = 0;
		SdUInt32 removedWidgetCount = 0;
		SdUInt32 modelCount = 0;

		void ResetFrameTransient() noexcept
		{
			createdWidgetCount = 0;
			reusedWidgetCount = 0;
			leavingWidgetCount = 0;
			removedWidgetCount = 0;
		}
	};

	class SdStateStorage final
	{
	private:
		std::unordered_map<SdWidgetId, SdWidgetRecord> widgetRecords = {};
		std::unordered_map<SdResolvedKey, SdWidgetId> widgetIdByResolvedKey = {};
		std::unordered_map<SdResolvedKey, std::unordered_map<std::type_index, Detail::SdAnyObject>> keyedModels = {};
		SdStateStorageStats stats = {};

	public:
		void Clear()
		{
			widgetRecords.clear();
			widgetIdByResolvedKey.clear();
			keyedModels.clear();
			stats = {};
		}

		void BeginFrame()
		{
			stats.ResetFrameTransient();
			stats.modelCount = static_cast<SdUInt32>(keyedModels.size());
		}

		SdWidgetRecord& GetOrCreateWidgetRecord(SdWidgetId id, bool* created = nullptr)
		{
			auto [it, inserted] = widgetRecords.try_emplace(id);
			if (created)
				*created = inserted || it->second.state.id == 0;
			if (inserted || it->second.state.id == 0)
				++stats.createdWidgetCount;
			else
				++stats.reusedWidgetCount;
			return it->second;
		}

		SdWidgetRecord* FindWidgetRecord(SdWidgetId id) noexcept
		{
			auto it = widgetRecords.find(id);
			return it == widgetRecords.end() ? nullptr : &it->second;
		}

		const SdWidgetRecord* FindWidgetRecord(SdWidgetId id) const noexcept
		{
			auto it = widgetRecords.find(id);
			return it == widgetRecords.end() ? nullptr : &it->second;
		}

		std::unordered_map<SdWidgetId, SdWidgetRecord>& GetWidgetRecords() noexcept
		{
			return widgetRecords;
		}

		const std::unordered_map<SdWidgetId, SdWidgetRecord>& GetWidgetRecords() const noexcept
		{
			return widgetRecords;
		}

		void RegisterResolvedKey(SdResolvedKey resolvedKey, SdWidgetId widgetId)
		{
			if (resolvedKey == 0)
				return;
			widgetIdByResolvedKey[resolvedKey] = widgetId;
		}

		SdWidgetId FindWidgetIdByResolvedKey(SdResolvedKey resolvedKey) const noexcept
		{
			auto it = widgetIdByResolvedKey.find(resolvedKey);
			return it == widgetIdByResolvedKey.end() ? 0 : it->second;
		}

		template<class T>
		T& GetOrCreateUserState(SdWidgetId widgetId)
		{
			auto recordIt = widgetRecords.find(widgetId);
			assert(recordIt != widgetRecords.end());
			SdWidgetRecord& record = recordIt->second;
			Detail::SdAnyObject& object = record.userStates[std::type_index(typeid(T))];
			if (!object.value)
				return object.Create<T>();
			if (T* state = object.Get<T>())
				return *state;
			return object.Create<T>();
		}

		template<class T>
		T& GetOrCreateModel(SdResolvedKey resolvedKey)
		{
			assert(resolvedKey != 0);
			std::unordered_map<std::type_index, Detail::SdAnyObject>& modelMap = keyedModels[resolvedKey];
			Detail::SdAnyObject& object = modelMap[std::type_index(typeid(T))];
			if (!object.value)
			{
				++stats.modelCount;
				return object.Create<T>();
			}
			if (T* model = object.Get<T>())
				return *model;
			return object.Create<T>();
		}

		bool RemoveModel(SdResolvedKey resolvedKey)
		{
			const bool removed = keyedModels.erase(resolvedKey) != 0;
			stats.modelCount = static_cast<SdUInt32>(keyedModels.size());
			return removed;
		}

		template<class TRemoveCallback>
		SdUInt32 RemoveDeadWidgets(TRemoveCallback&& removeCallback)
		{
			SdUInt32 removedCount = 0;
			for (auto it = widgetRecords.begin(); it != widgetRecords.end();)
			{
				if (it->second.state.lifePhase == SdWidgetLifePhase::Dead)
				{
					if (it->second.resolvedKey != 0)
						widgetIdByResolvedKey.erase(it->second.resolvedKey);
					removeCallback(it->first);
					it = widgetRecords.erase(it);
					++removedCount;
				}
				else
				{
					++it;
				}
			}
			stats.removedWidgetCount = removedCount;
			stats.modelCount = static_cast<SdUInt32>(keyedModels.size());
			return removedCount;
		}

		SdStateStorageStats& GetStats() noexcept
		{
			return stats;
		}

		const SdStateStorageStats& GetStats() const noexcept
		{
			return stats;
		}
	};
}
