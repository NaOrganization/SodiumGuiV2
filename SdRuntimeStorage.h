#pragma once

#include "SdAnimation.h"
#include "SdStyleCore.h"
#include "SdWidgetContext.h"

#include <cassert>
#include <memory>
#include <new>
#include <string>
#include <typeindex>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Sodium::Detail
{
	struct SdObjectHandle final
	{
		std::type_index type = std::type_index(typeid(void));
		SdUInt32 index = SdInvalidIndex<SdUInt32>;
		SdUInt32 generation = 0;

		bool IsValid() const noexcept
		{
			return generation != 0 && index != SdInvalidIndex<SdUInt32> && type != std::type_index(typeid(void));
		}

		void Reset() noexcept
		{
			type = std::type_index(typeid(void));
			index = SdInvalidIndex<SdUInt32>;
			generation = 0;
		}
	};

	class SdObjectPoolBase
	{
	public:
		virtual ~SdObjectPoolBase() = default;
		virtual void* Get(const SdObjectHandle& handle) noexcept = 0;
		virtual void Destroy(const SdObjectHandle& handle) noexcept = 0;
		virtual void Clear() noexcept = 0;
		virtual SdUInt32 GetLiveCount() const noexcept = 0;
	};

	template<class T>
	class SdTypedObjectPool final : public SdObjectPoolBase
	{
	private:
		static constexpr SdUInt32 kSlotsPerPage = 64;

		struct Slot final
		{
			alignas(T) std::byte storage[sizeof(T)] = {};
			SdUInt32 generation = 1;
			bool occupied = false;
		};

		std::vector<std::unique_ptr<Slot[]>> pages = {};
		std::vector<SdUInt32> freeIndices = {};
		SdUInt32 slotCount = 0;
		SdUInt32 liveCount = 0;

		Slot& GetSlot(SdUInt32 index) noexcept
		{
			return pages[index / kSlotsPerPage][index % kSlotsPerPage];
		}

		const Slot& GetSlot(SdUInt32 index) const noexcept
		{
			return pages[index / kSlotsPerPage][index % kSlotsPerPage];
		}

		static T* GetValue(Slot& slot) noexcept
		{
			return std::launder(reinterpret_cast<T*>(slot.storage));
		}

		static const T* GetValue(const Slot& slot) noexcept
		{
			return std::launder(reinterpret_cast<const T*>(slot.storage));
		}

		void AddPage()
		{
			pages.push_back(std::make_unique<Slot[]>(kSlotsPerPage));
			const SdUInt32 firstIndex = slotCount;
			slotCount += kSlotsPerPage;
			freeIndices.reserve(freeIndices.size() + kSlotsPerPage);
			for (SdUInt32 offset = 0; offset < kSlotsPerPage; ++offset)
				freeIndices.push_back(firstIndex + (kSlotsPerPage - 1 - offset));
		}

	public:
		template<class... TArgs>
		SdObjectHandle Create(TArgs&&... args)
		{
			if (freeIndices.empty())
				AddPage();

			const SdUInt32 index = freeIndices.back();
			freeIndices.pop_back();
			Slot& slot = GetSlot(index);
			assert(!slot.occupied);
			::new (static_cast<void*>(slot.storage)) T(std::forward<TArgs>(args)...);
			slot.occupied = true;
			++liveCount;
			return { std::type_index(typeid(T)), index, slot.generation };
		}

		void* Get(const SdObjectHandle& handle) noexcept override
		{
			if (handle.index >= slotCount)
				return nullptr;
			Slot& slot = GetSlot(handle.index);
			if (!slot.occupied || slot.generation != handle.generation)
				return nullptr;
			return GetValue(slot);
		}

		void Destroy(const SdObjectHandle& handle) noexcept override
		{
			if (handle.index >= slotCount)
				return;
			Slot& slot = GetSlot(handle.index);
			if (!slot.occupied || slot.generation != handle.generation)
				return;

			GetValue(slot)->~T();
			slot.occupied = false;
			++slot.generation;
			if (slot.generation == 0)
				slot.generation = 1;
			freeIndices.push_back(handle.index);
			--liveCount;
		}

		void Clear() noexcept override
		{
			for (SdUInt32 index = 0; index < slotCount; ++index)
			{
				Slot& slot = GetSlot(index);
				if (slot.occupied)
				{
					GetValue(slot)->~T();
					slot.occupied = false;
				}
			}
			pages.clear();
			freeIndices.clear();
			slotCount = 0;
			liveCount = 0;
		}

		SdUInt32 GetLiveCount() const noexcept override
		{
			return liveCount;
		}
	};

	class SdObjectStore final
	{
	private:
		std::unordered_map<std::type_index, std::unique_ptr<SdObjectPoolBase>> pools = {};

		template<class T>
		SdTypedObjectPool<T>& GetOrCreatePool()
		{
			const std::type_index type = std::type_index(typeid(T));
			auto it = pools.find(type);
			if (it == pools.end())
			{
				auto pool = std::make_unique<SdTypedObjectPool<T>>();
				SdTypedObjectPool<T>* rawPool = pool.get();
				pools.emplace(type, std::move(pool));
				return *rawPool;
			}
			return *static_cast<SdTypedObjectPool<T>*>(it->second.get());
		}

	public:
		template<class T, class... TArgs>
		SdObjectHandle Create(TArgs&&... args)
		{
			return GetOrCreatePool<T>().Create(std::forward<TArgs>(args)...);
		}

		template<class T>
		T* Get(const SdObjectHandle& handle) noexcept
		{
			if (handle.type != std::type_index(typeid(T)))
				return nullptr;
			auto it = pools.find(handle.type);
			if (it == pools.end())
				return nullptr;
			return static_cast<T*>(it->second->Get(handle));
		}

		void Destroy(SdObjectHandle& handle) noexcept
		{
			if (!handle.IsValid())
				return;
			auto it = pools.find(handle.type);
			if (it != pools.end())
				it->second->Destroy(handle);
			handle.Reset();
		}

		void* GetRaw(const SdObjectHandle& handle) noexcept
		{
			if (!handle.IsValid())
				return nullptr;
			auto it = pools.find(handle.type);
			if (it == pools.end())
				return nullptr;
			return it->second->Get(handle);
		}

		bool Contains(const SdObjectHandle& handle) noexcept
		{
			if (!handle.IsValid())
				return false;
			auto it = pools.find(handle.type);
			return it != pools.end() && it->second->Get(handle) != nullptr;
		}

		void Clear() noexcept
		{
			for (auto& [type, pool] : pools)
				pool->Clear();
			pools.clear();
		}

		SdUInt32 GetLiveObjectCount() const noexcept
		{
			SdUInt32 count = 0;
			for (const auto& [type, pool] : pools)
				count += pool->GetLiveCount();
			return count;
		}
	};
}

namespace Sodium
{
	struct SdLayoutCache final
	{
		SdVec2 measuredSize = {};
		SdRect targetRect = {};
		SdRect animatedRect = {};
		SdRect clipRect = {};
	};

	struct SdStyleCache final
	{
		SdWidgetRootStyle resolvedStyle = {};
		SdWidgetRootStyle presentationStyle = {};
		SdStyleNodeId rootStyleNodeId = SdInvalidStyleNodeId;
		SdStyleId targetTypeId = SdWidgetTargetIds::Default;
		SdStyleInteractionState interactionState = SdStyleInteractionState::Normal;
		SdLayerPriority layerPriority = SdLayerPriority::Content;
		SdUInt64 styleIdentityRevision = 0;
		SdUInt64 styleRevision = 0;
		bool valid = false;
	};

	struct SdTypedStyleRecord final
	{
		Detail::SdObjectHandle resolvedStyle = {};
		Detail::SdObjectHandle presentationStyle = {};
		Detail::SdObjectHandle inlineStyle = {};
		std::type_index styleType = std::type_index(typeid(void));
		SdStyleId targetTypeId = SdWidgetTargetIds::Default;
		SdStyleInteractionState interactionState = SdStyleInteractionState::Normal;
		SdLayerPriority layerPriority = SdLayerPriority::Content;
		SdUInt64 styleIdentityRevision = 0;
		SdUInt64 inlineStyleRevision = 0;
		SdUInt64 resolvedInlineStyleRevision = 0;
		SdUInt64 styleRevision = 0;
		bool valid = false;
	};

	struct SdWidgetRecord final
	{
		SdWidgetState state = {};
		SdStyleNode rootStyleNode = {};
		SdStyleNodeId rootStyleNodeId = SdInvalidStyleNodeId;
		std::vector<SdStyleNodeId> partStyleNodeIds = {};
		SdLayoutCache layoutCache = {};
		SdStyleCache styleCache = {};
		SdAnimationWidgetState animation = {};
		Detail::SdObjectHandle widgetObject = {};
		std::unordered_map<std::type_index, Detail::SdObjectHandle> userStates = {};
		std::unordered_map<std::type_index, SdTypedStyleRecord> typedStyles = {};
		std::type_index widgetType = std::type_index(typeid(void));
		std::vector<SdStyleClassId> styleClasses = {};
		SdStyleScopeId styleScope = 0;
		SdUInt64 styleIdentityRevision = 1;
		SdWidgetId parentId = 0;
		SdUInt32 order = 0;
		SdResolvedKey resolvedKey = 0;
		SdUtf8String debugKey = {};
		void(*styleCallback)(SdInstance&, SdWidgetRecord&, SdStyleInteractionState, SdLayerPriority) = nullptr;
		void(*typedStyleAnimationCallback)(SdInstance&, SdWidgetRecord&, SdDuration) = nullptr;
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
		SdUInt32 styleNodeCount = 0;
		SdUInt32 liveObjectCount = 0;

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
		std::unordered_map<SdResolvedKey, std::unordered_map<std::type_index, Detail::SdObjectHandle>> keyedModels = {};
		std::vector<SdStyleNode> styleNodes = {};
		Detail::SdObjectStore objectStore = {};
		SdStateStorageStats stats = {};

		SdStyleNode& CreateStyleNode(SdWidgetRecord& record, SdWidgetId widgetId, SdStylePart part, SdStyleNodeKind kind)
		{
			const SdStyleNodeId index = static_cast<SdStyleNodeId>(styleNodes.size());
			SdStyleNode& node = styleNodes.emplace_back();
			node.styleNodeId = index;
			node.widgetId = widgetId;
			node.part = part;
			node.kind = kind;
			node.parentStyleNodeId = kind == SdStyleNodeKind::Root ? SdInvalidStyleNodeId : record.rootStyleNodeId;
			if (kind == SdStyleNodeKind::Root)
			{
				record.rootStyleNodeId = index;
				record.styleCache.rootStyleNodeId = index;
				record.rootStyleNode = node;
			}
			else
			{
				record.partStyleNodeIds.push_back(index);
			}
			stats.styleNodeCount = CountLiveStyleNodes();
			return node;
		}

		void ReleaseStyleNodes(SdWidgetRecord& record) noexcept
		{
			if (record.rootStyleNodeId != SdInvalidStyleNodeId && record.rootStyleNodeId < styleNodes.size())
				styleNodes[record.rootStyleNodeId].widgetId = 0;
			for (SdStyleNodeId partNodeId : record.partStyleNodeIds)
			{
				if (partNodeId < styleNodes.size())
					styleNodes[partNodeId].widgetId = 0;
			}
			record.rootStyleNodeId = SdInvalidStyleNodeId;
			record.styleCache.rootStyleNodeId = SdInvalidStyleNodeId;
			record.partStyleNodeIds.clear();
			stats.styleNodeCount = CountLiveStyleNodes();
		}

		void DestroyWidgetRecordObjects(SdWidgetRecord& record)
		{
			ReleaseStyleNodes(record);
			objectStore.Destroy(record.widgetObject);
			for (auto& [type, handle] : record.userStates)
				objectStore.Destroy(handle);
			record.userStates.clear();
			for (auto& [type, styleRecord] : record.typedStyles)
			{
				objectStore.Destroy(styleRecord.resolvedStyle);
				objectStore.Destroy(styleRecord.presentationStyle);
				objectStore.Destroy(styleRecord.inlineStyle);
			}
			record.typedStyles.clear();
		}

		void DestroyModelMap(std::unordered_map<std::type_index, Detail::SdObjectHandle>& modelMap)
		{
			for (auto& [type, handle] : modelMap)
				objectStore.Destroy(handle);
			modelMap.clear();
		}

	public:
		void Clear()
		{
			for (auto& [id, record] : widgetRecords)
				DestroyWidgetRecordObjects(record);
			for (auto& [resolvedKey, modelMap] : keyedModels)
				DestroyModelMap(modelMap);
			widgetRecords.clear();
			widgetIdByResolvedKey.clear();
			keyedModels.clear();
			styleNodes.clear();
			objectStore.Clear();
			stats = {};
		}

		void BeginFrame()
		{
			stats.ResetFrameTransient();
			stats.modelCount = static_cast<SdUInt32>(keyedModels.size());
			stats.styleNodeCount = CountLiveStyleNodes();
			stats.liveObjectCount = objectStore.GetLiveObjectCount();
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

		SdStyleNode& EnsureRootStyleNode(SdWidgetRecord& record, SdWidgetId widgetId)
		{
			if (record.rootStyleNodeId != SdInvalidStyleNodeId
				&& record.rootStyleNodeId < styleNodes.size()
				&& styleNodes[record.rootStyleNodeId].kind == SdStyleNodeKind::Root)
			{
				SdStyleNode& node = styleNodes[record.rootStyleNodeId];
				node.widgetId = widgetId;
				node.part = SdStylePart::Root();
				node.parentStyleNodeId = SdInvalidStyleNodeId;
				record.rootStyleNode = node;
				record.styleCache.rootStyleNodeId = record.rootStyleNodeId;
				return node;
			}

			return CreateStyleNode(record, widgetId, SdStylePart::Root(), SdStyleNodeKind::Root);
		}

		SdStyleNode& EnsurePartStyleNode(SdWidgetRecord& record, SdStylePart part)
		{
			if (part.IsRoot())
				return EnsureRootStyleNode(record, record.state.id);

			EnsureRootStyleNode(record, record.state.id);
			for (SdStyleNodeId partNodeId : record.partStyleNodeIds)
			{
				if (partNodeId >= styleNodes.size())
					continue;
				SdStyleNode& node = styleNodes[partNodeId];
				if (node.part == part)
				{
					node.widgetId = record.state.id;
					node.parentStyleNodeId = record.rootStyleNodeId;
					return node;
				}
			}

			SdStyleNode& node = CreateStyleNode(record, record.state.id, part, SdStyleNodeKind::Part);
			const SdStyleNode& root = styleNodes[record.rootStyleNodeId];
			node.scopeId = root.scopeId;
			node.pseudoState = root.pseudoState;
			node.specifiedStyle = root.resolvedStyle;
			node.resolvedStyle = root.resolvedStyle;
			node.presentationStyle = root.presentationStyle;
			return node;
		}

		SdStyleNode* FindStyleNode(SdWidgetRecord& record, SdStylePart part) noexcept
		{
			if (part.IsRoot())
				return (record.rootStyleNodeId != SdInvalidStyleNodeId && record.rootStyleNodeId < styleNodes.size())
					? &styleNodes[record.rootStyleNodeId]
					: nullptr;

			for (SdStyleNodeId partNodeId : record.partStyleNodeIds)
			{
				if (partNodeId < styleNodes.size() && styleNodes[partNodeId].part == part)
					return &styleNodes[partNodeId];
			}
			return nullptr;
		}

		const SdStyleNode* FindStyleNode(const SdWidgetRecord& record, SdStylePart part) const noexcept
		{
			return const_cast<SdStateStorage*>(this)->FindStyleNode(const_cast<SdWidgetRecord&>(record), part);
		}

		SdStyleNode* FindStyleNodeById(SdStyleNodeId styleNodeId) noexcept
		{
			return styleNodeId < styleNodes.size() ? &styleNodes[styleNodeId] : nullptr;
		}

		const SdStyleNode* FindStyleNodeById(SdStyleNodeId styleNodeId) const noexcept
		{
			return styleNodeId < styleNodes.size() ? &styleNodes[styleNodeId] : nullptr;
		}

		const std::vector<SdStyleNode>& GetStyleNodes() const noexcept
		{
			return styleNodes;
		}

		SdUInt32 CountLiveStyleNodes() const noexcept
		{
			SdUInt32 count = 0;
			for (const SdStyleNode& node : styleNodes)
			{
				if (node.widgetId != 0)
					++count;
			}
			return count;
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
			Detail::SdObjectHandle& object = record.userStates[std::type_index(typeid(T))];
			if (!object.IsValid())
			{
				object = objectStore.Create<T>();
				stats.liveObjectCount = objectStore.GetLiveObjectCount();
				return *objectStore.Get<T>(object);
			}
			if (T* state = objectStore.Get<T>(object))
				return *state;
			objectStore.Destroy(object);
			object = objectStore.Create<T>();
			stats.liveObjectCount = objectStore.GetLiveObjectCount();
			return *objectStore.Get<T>(object);
		}

		template<class T>
		T& GetOrCreateModel(SdResolvedKey resolvedKey)
		{
			assert(resolvedKey != 0);
			std::unordered_map<std::type_index, Detail::SdObjectHandle>& modelMap = keyedModels[resolvedKey];
			Detail::SdObjectHandle& object = modelMap[std::type_index(typeid(T))];
			if (!object.IsValid())
			{
				object = objectStore.Create<T>();
				stats.modelCount = static_cast<SdUInt32>(keyedModels.size());
				stats.liveObjectCount = objectStore.GetLiveObjectCount();
				return *objectStore.Get<T>(object);
			}
			if (T* model = objectStore.Get<T>(object))
				return *model;
			objectStore.Destroy(object);
			object = objectStore.Create<T>();
			stats.liveObjectCount = objectStore.GetLiveObjectCount();
			return *objectStore.Get<T>(object);
		}

		bool RemoveModel(SdResolvedKey resolvedKey)
		{
			auto it = keyedModels.find(resolvedKey);
			if (it == keyedModels.end())
				return false;
			DestroyModelMap(it->second);
			keyedModels.erase(it);
			stats.modelCount = static_cast<SdUInt32>(keyedModels.size());
			stats.liveObjectCount = objectStore.GetLiveObjectCount();
			return true;
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
					DestroyWidgetRecordObjects(it->second);
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
			stats.liveObjectCount = objectStore.GetLiveObjectCount();
			return removedCount;
		}

		template<class T, class... TArgs>
		T& CreateWidgetObject(SdWidgetRecord& record, TArgs&&... args)
		{
			objectStore.Destroy(record.widgetObject);
			record.widgetObject = objectStore.Create<T>(std::forward<TArgs>(args)...);
			stats.liveObjectCount = objectStore.GetLiveObjectCount();
			return *objectStore.Get<T>(record.widgetObject);
		}

		template<class T>
		T* GetWidgetObject(SdWidgetRecord& record) noexcept
		{
			return objectStore.Get<T>(record.widgetObject);
		}

		template<class T>
		const T* GetWidgetObject(const SdWidgetRecord& record) const noexcept
		{
			return const_cast<Detail::SdObjectStore&>(objectStore).Get<T>(record.widgetObject);
		}

		void* GetWidgetObjectPointer(SdWidgetRecord& record) noexcept
		{
			return objectStore.GetRaw(record.widgetObject);
		}

		bool HasWidgetObject(const SdWidgetRecord& record) noexcept
		{
			return objectStore.Contains(record.widgetObject);
		}

		template<class TStyle>
		SdTypedStyleRecord& GetOrCreateTypedStyleRecord(SdWidgetRecord& record)
		{
			SdTypedStyleRecord& styleRecord = record.typedStyles[std::type_index(typeid(TStyle))];
			if (styleRecord.styleType == std::type_index(typeid(void)))
				styleRecord.styleType = std::type_index(typeid(TStyle));
			return styleRecord;
		}

		template<class TStyle>
		TStyle& GetOrCreateResolvedStyle(SdTypedStyleRecord& styleRecord, const TStyle& initialStyle)
		{
			if (!styleRecord.resolvedStyle.IsValid())
			{
				styleRecord.resolvedStyle = objectStore.Create<TStyle>(initialStyle);
				stats.liveObjectCount = objectStore.GetLiveObjectCount();
			}
			if (TStyle* style = objectStore.Get<TStyle>(styleRecord.resolvedStyle))
				return *style;
			objectStore.Destroy(styleRecord.resolvedStyle);
			styleRecord.resolvedStyle = objectStore.Create<TStyle>(initialStyle);
			stats.liveObjectCount = objectStore.GetLiveObjectCount();
			return *objectStore.Get<TStyle>(styleRecord.resolvedStyle);
		}

		template<class TStyle>
		TStyle& GetOrCreatePresentationStyle(SdTypedStyleRecord& styleRecord, const TStyle& initialStyle)
		{
			if (!styleRecord.presentationStyle.IsValid())
			{
				styleRecord.presentationStyle = objectStore.Create<TStyle>(initialStyle);
				stats.liveObjectCount = objectStore.GetLiveObjectCount();
			}
			if (TStyle* style = objectStore.Get<TStyle>(styleRecord.presentationStyle))
				return *style;
			objectStore.Destroy(styleRecord.presentationStyle);
			styleRecord.presentationStyle = objectStore.Create<TStyle>(initialStyle);
			stats.liveObjectCount = objectStore.GetLiveObjectCount();
			return *objectStore.Get<TStyle>(styleRecord.presentationStyle);
		}

		template<class TStyle>
		TStyle* FindResolvedStyle(SdWidgetRecord& record) noexcept
		{
			auto it = record.typedStyles.find(std::type_index(typeid(TStyle)));
			if (it == record.typedStyles.end())
				return nullptr;
			return objectStore.Get<TStyle>(it->second.resolvedStyle);
		}

		template<class TStyle>
		TStyle* FindPresentationStyle(SdWidgetRecord& record) noexcept
		{
			auto it = record.typedStyles.find(std::type_index(typeid(TStyle)));
			if (it == record.typedStyles.end())
				return nullptr;
			return objectStore.Get<TStyle>(it->second.presentationStyle);
		}

		template<class TStyle>
		const TStyle* FindInlineStyle(SdTypedStyleRecord& styleRecord) const noexcept
		{
			return const_cast<Detail::SdObjectStore&>(objectStore).Get<TStyle>(styleRecord.inlineStyle);
		}

		template<class TStyle>
		bool SetInlineStyle(SdWidgetRecord& record, const TStyle* inlineStyle)
		{
			SdTypedStyleRecord& styleRecord = GetOrCreateTypedStyleRecord<TStyle>(record);
			if (!inlineStyle)
				return ClearInlineStyle<TStyle>(record);

			if (!styleRecord.inlineStyle.IsValid())
			{
				styleRecord.inlineStyle = objectStore.Create<TStyle>(*inlineStyle);
				stats.liveObjectCount = objectStore.GetLiveObjectCount();
			}
			else if (TStyle* storedStyle = objectStore.Get<TStyle>(styleRecord.inlineStyle))
			{
				*storedStyle = *inlineStyle;
			}
			else
			{
				objectStore.Destroy(styleRecord.inlineStyle);
				styleRecord.inlineStyle = objectStore.Create<TStyle>(*inlineStyle);
				stats.liveObjectCount = objectStore.GetLiveObjectCount();
			}

			++styleRecord.inlineStyleRevision;
			if (styleRecord.inlineStyleRevision == 0)
				styleRecord.inlineStyleRevision = 1;
			return true;
		}

		template<class TStyle>
		bool ClearInlineStyle(SdWidgetRecord& record)
		{
			auto it = record.typedStyles.find(std::type_index(typeid(TStyle)));
			if (it == record.typedStyles.end() || !it->second.inlineStyle.IsValid())
				return false;

			objectStore.Destroy(it->second.inlineStyle);
			++it->second.inlineStyleRevision;
			if (it->second.inlineStyleRevision == 0)
				it->second.inlineStyleRevision = 1;
			stats.liveObjectCount = objectStore.GetLiveObjectCount();
			return true;
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
