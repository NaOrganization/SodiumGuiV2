#pragma once

#include "Animation/SdAnimation.h"
#include "Style/SdStyleCore.h"
#include "Widget/SdWidgetContext.h"

#include <cassert>
#include <cstddef>
#include <memory>
#include <new>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Sodium::Detail
{
	template<class T, SdSize InlineCapacity>
	class SdSmallVector final
	{
	private:
		static constexpr SdSize kInlineStorageCount = InlineCapacity == 0 ? 1 : InlineCapacity;

		alignas(T) std::byte inlineStorage[sizeof(T) * kInlineStorageCount] = {};
		T* items = InlineData();
		SdSize itemCount = 0;
		SdSize itemCapacity = InlineCapacity;
		std::allocator<T> allocator = {};

		T* InlineData() noexcept
		{
			return std::launder(reinterpret_cast<T*>(inlineStorage));
		}

		const T* InlineData() const noexcept
		{
			return std::launder(reinterpret_cast<const T*>(inlineStorage));
		}

		bool IsInline() const noexcept
		{
			return items == InlineData();
		}

		void DestroyRange(T* beginValue, T* endValue) noexcept
		{
			while (beginValue != endValue)
			{
				--endValue;
				std::destroy_at(endValue);
			}
		}

		void Reallocate(SdSize newCapacity)
		{
			assert(newCapacity >= itemCount);
			T* newItems = std::allocator_traits<std::allocator<T>>::allocate(allocator, newCapacity);
			SdSize movedCount = 0;
			try
			{
				for (; movedCount < itemCount; ++movedCount)
					std::construct_at(newItems + movedCount, std::move_if_noexcept(items[movedCount]));
			}
			catch (...)
			{
				DestroyRange(newItems, newItems + movedCount);
				std::allocator_traits<std::allocator<T>>::deallocate(allocator, newItems, newCapacity);
				throw;
			}

			DestroyRange(items, items + itemCount);
			if (!IsInline())
				std::allocator_traits<std::allocator<T>>::deallocate(allocator, items, itemCapacity);
			items = newItems;
			itemCapacity = newCapacity;
		}

		void EnsureCapacity(SdSize requiredCapacity)
		{
			if (requiredCapacity <= itemCapacity)
				return;
			SdSize newCapacity = itemCapacity == 0 ? 1 : itemCapacity * 2;
			if (newCapacity < requiredCapacity)
				newCapacity = requiredCapacity;
			Reallocate(newCapacity);
		}

	public:
		using iterator = T*;
		using const_iterator = const T*;

		SdSmallVector() = default;

		SdSmallVector(const SdSmallVector& other)
		{
			EnsureCapacity(other.itemCount);
			for (; itemCount < other.itemCount; ++itemCount)
				std::construct_at(items + itemCount, other.items[itemCount]);
		}

		SdSmallVector& operator=(const SdSmallVector& other)
		{
			if (this == &other)
				return *this;
			clear();
			EnsureCapacity(other.itemCount);
			for (; itemCount < other.itemCount; ++itemCount)
				std::construct_at(items + itemCount, other.items[itemCount]);
			return *this;
		}

		SdSmallVector(SdSmallVector&& other) noexcept
		{
			if (other.IsInline())
			{
				for (; itemCount < other.itemCount; ++itemCount)
					std::construct_at(items + itemCount, std::move(other.items[itemCount]));
				other.clear();
			}
			else
			{
				items = other.items;
				itemCount = other.itemCount;
				itemCapacity = other.itemCapacity;
				other.items = other.InlineData();
				other.itemCount = 0;
				other.itemCapacity = InlineCapacity;
			}
		}

		SdSmallVector& operator=(SdSmallVector&& other) noexcept
		{
			if (this == &other)
				return *this;
			clear();
			if (!IsInline())
				std::allocator_traits<std::allocator<T>>::deallocate(allocator, items, itemCapacity);
			items = InlineData();
			itemCapacity = InlineCapacity;

			if (other.IsInline())
			{
				for (; itemCount < other.itemCount; ++itemCount)
					std::construct_at(items + itemCount, std::move(other.items[itemCount]));
				other.clear();
			}
			else
			{
				items = other.items;
				itemCount = other.itemCount;
				itemCapacity = other.itemCapacity;
				other.items = other.InlineData();
				other.itemCount = 0;
				other.itemCapacity = InlineCapacity;
			}
			return *this;
		}

		~SdSmallVector()
		{
			clear();
			if (!IsInline())
				std::allocator_traits<std::allocator<T>>::deallocate(allocator, items, itemCapacity);
		}

		template<class... TArgs>
		T& emplace_back(TArgs&&... args)
		{
			EnsureCapacity(itemCount + 1);
			T* item = items + itemCount;
			std::construct_at(item, std::forward<TArgs>(args)...);
			++itemCount;
			return *item;
		}

		void push_back(const T& item)
		{
			emplace_back(item);
		}

		void push_back(T&& item)
		{
			emplace_back(std::move(item));
		}

		iterator erase(const_iterator position)
		{
			assert(position >= begin() && position < end());
			const SdSize index = static_cast<SdSize>(position - begin());
			std::destroy_at(items + index);
			for (SdSize moveIndex = index; moveIndex + 1 < itemCount; ++moveIndex)
			{
				std::construct_at(items + moveIndex, std::move(items[moveIndex + 1]));
				std::destroy_at(items + moveIndex + 1);
			}
			--itemCount;
			return items + index;
		}

		void clear() noexcept
		{
			DestroyRange(items, items + itemCount);
			itemCount = 0;
		}

		void pop_back() noexcept
		{
			assert(itemCount != 0);
			--itemCount;
			std::destroy_at(items + itemCount);
		}

		bool empty() const noexcept { return itemCount == 0; }
		SdSize size() const noexcept { return itemCount; }
		SdSize capacity() const noexcept { return itemCapacity; }

		T& operator[](SdSize index) noexcept
		{
			assert(index < itemCount);
			return items[index];
		}

		const T& operator[](SdSize index) const noexcept
		{
			assert(index < itemCount);
			return items[index];
		}

		iterator begin() noexcept { return items; }
		const_iterator begin() const noexcept { return items; }
		const_iterator cbegin() const noexcept { return items; }
		iterator end() noexcept { return items + itemCount; }
		const_iterator end() const noexcept { return items + itemCount; }
		const_iterator cend() const noexcept { return items + itemCount; }
	};

	struct SdObjectHandle final
	{
		SdTypeId type = 0;
		SdUInt32 index = SdInvalidIndex<SdUInt32>;
		SdUInt32 generation = 0;

		bool IsValid() const noexcept
		{
			return generation != 0 && index != SdInvalidIndex<SdUInt32> && type != 0;
		}

		void Reset() noexcept
		{
			type = 0;
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
			return { SdStableTypeId<T>(), index, slot.generation };
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
		std::unordered_map<SdTypeId, std::unique_ptr<SdObjectPoolBase>> pools = {};

		template<class T>
		SdTypedObjectPool<T>& GetOrCreatePool()
		{
			const SdTypeId type = SdStableTypeId<T>();
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
			if (handle.type != SdStableTypeId<T>())
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
		SdTypeId targetTypeId = SdWidgetTargetIds::Default;
		SdStyleInteractionState interactionState = SdStyleInteractionState::Normal;
		SdRootLayer rootLayer = SdRootLayer::Content;
		SdUInt64 styleIdentityRevision = 0;
		SdUInt64 inlineStyleRevision = 0;
		SdUInt64 styleRevision = 0;
		bool valid = false;
	};

	struct SdTypedStyleRecord final
	{
		Detail::SdObjectHandle resolvedStyle = {};
		Detail::SdObjectHandle presentationStyle = {};
		Detail::SdObjectHandle inlineStyle = {};
		SdTypeId styleType = 0;
		SdTypeId targetTypeId = SdWidgetTargetIds::Default;
		SdStyleInteractionState interactionState = SdStyleInteractionState::Normal;
		SdRootLayer rootLayer = SdRootLayer::Content;
		SdUInt64 styleIdentityRevision = 0;
		SdUInt64 inlineStyleRevision = 0;
		SdUInt64 resolvedInlineStyleRevision = 0;
		SdUInt64 styleRevision = 0;
		bool valid = false;
	};

	struct SdTypedObjectSlot final
	{
		SdTypeId type = 0;
		Detail::SdObjectHandle object = {};
	};

	struct SdTypedStyleSlot final
	{
		SdTypeId type = 0;
		SdTypedStyleRecord record = {};
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
		Detail::SdSmallVector<SdTypedObjectSlot, 2> userStates = {};
		Detail::SdSmallVector<SdTypedStyleSlot, 1> typedStyles = {};
		SdTypeId widgetType = 0;
		std::vector<SdStyleClassId> styleClasses = {};
		SdStyleScopeId styleScope = 0;
		SdUInt64 styleIdentityRevision = 1;
		SdWidgetId parentId = 0;
		SdUInt32 order = 0;
		SdResolvedKey resolvedKey = 0;
		SdUtf8String debugKey = {};
		void(*styleCallback)(SdInstance&, SdWidgetRecord&, SdStyleInteractionState, SdRootLayer) = nullptr;
		void(*typedStyleAnimationCallback)(SdInstance&, SdWidgetRecord&, SdDuration) = nullptr;
		void(*layoutCallback)(void*, SdLayoutContext&) = nullptr;
		void(*arrangeCallback)(void*, SdArrangeContext&) = nullptr;
		void(*paintCallback)(void*, SdPaintContext&) = nullptr;

		bool HasResolvedKey() const noexcept
		{
			return resolvedKey != 0;
		}
	};

	struct SdModelRecord final
	{
		Detail::SdObjectHandle object = {};
		SdModelLifetime lifetime = SdModelLifetime::Manual;
		SdWidgetId ownerWidgetId = 0;
	};

	struct SdModelSlot final
	{
		SdTypeId type = 0;
		SdModelRecord record = {};
	};

	struct SdModelBucket final
	{
		SdResolvedKey resolvedKey = 0;
		Detail::SdSmallVector<SdModelSlot, 2> models = {};
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

#ifndef NDEBUG
	struct SdSubmittedIdInfo final
	{
		SdWidgetId parentId = 0;
		SdResolvedKey resolvedKey = 0;
		SdUtf8String debugKey = {};
		SdTypeId widgetType = 0;
	};
#endif

	class SdStateStorage final
	{
	private:
		std::unordered_map<SdWidgetId, SdWidgetRecord> widgetRecords = {};
		std::unordered_map<SdResolvedKey, SdWidgetId> widgetIdByResolvedKey = {};
		std::unordered_map<SdResolvedKey, SdSize> modelBucketByResolvedKey = {};
		std::vector<SdModelBucket> modelBuckets = {};
#ifndef NDEBUG
		std::unordered_map<SdWidgetId, SdSubmittedIdInfo> submittedIdsThisFrame = {};
#endif
		std::vector<SdStyleNode> styleNodes = {};
		Detail::SdObjectStore objectStore = {};
		SdStateStorageStats stats = {};

		static Detail::SdObjectHandle* FindTypedObject(Detail::SdSmallVector<SdTypedObjectSlot, 2>& slots, SdTypeId type) noexcept
		{
			for (SdTypedObjectSlot& slot : slots)
			{
				if (slot.type == type)
					return &slot.object;
			}
			return nullptr;
		}

		static const Detail::SdObjectHandle* FindTypedObject(const Detail::SdSmallVector<SdTypedObjectSlot, 2>& slots, SdTypeId type) noexcept
		{
			for (const SdTypedObjectSlot& slot : slots)
			{
				if (slot.type == type)
					return &slot.object;
			}
			return nullptr;
		}

		static SdTypedStyleRecord* FindTypedStyle(Detail::SdSmallVector<SdTypedStyleSlot, 1>& slots, SdTypeId type) noexcept
		{
			for (SdTypedStyleSlot& slot : slots)
			{
				if (slot.type == type)
					return &slot.record;
			}
			return nullptr;
		}

		static const SdTypedStyleRecord* FindTypedStyle(const Detail::SdSmallVector<SdTypedStyleSlot, 1>& slots, SdTypeId type) noexcept
		{
			for (const SdTypedStyleSlot& slot : slots)
			{
				if (slot.type == type)
					return &slot.record;
			}
			return nullptr;
		}

		static SdModelRecord* FindModelRecord(SdModelBucket& bucket, SdTypeId type) noexcept
		{
			for (SdModelSlot& slot : bucket.models)
			{
				if (slot.type == type)
					return &slot.record;
			}
			return nullptr;
		}

		SdModelBucket* FindModelBucket(SdResolvedKey resolvedKey) noexcept
		{
			auto it = modelBucketByResolvedKey.find(resolvedKey);
			if (it == modelBucketByResolvedKey.end() || it->second >= modelBuckets.size())
				return nullptr;
			return &modelBuckets[it->second];
		}

		SdModelBucket& GetOrCreateModelBucket(SdResolvedKey resolvedKey)
		{
			if (SdModelBucket* bucket = FindModelBucket(resolvedKey))
				return *bucket;
			const SdSize index = modelBuckets.size();
			SdModelBucket& bucket = modelBuckets.emplace_back();
			bucket.resolvedKey = resolvedKey;
			modelBucketByResolvedKey[resolvedKey] = index;
			return bucket;
		}

		void EraseModelBucket(SdSize bucketIndex)
		{
			assert(bucketIndex < modelBuckets.size());
			const SdResolvedKey erasedKey = modelBuckets[bucketIndex].resolvedKey;
			const SdSize lastIndex = modelBuckets.size() - 1;
			if (bucketIndex != lastIndex)
			{
				modelBuckets[bucketIndex] = std::move(modelBuckets[lastIndex]);
				modelBucketByResolvedKey[modelBuckets[bucketIndex].resolvedKey] = bucketIndex;
			}
			modelBuckets.pop_back();
			modelBucketByResolvedKey.erase(erasedKey);
		}

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
			for (SdTypedObjectSlot& slot : record.userStates)
				objectStore.Destroy(slot.object);
			record.userStates.clear();
			for (SdTypedStyleSlot& slot : record.typedStyles)
			{
				SdTypedStyleRecord& styleRecord = slot.record;
				objectStore.Destroy(styleRecord.resolvedStyle);
				objectStore.Destroy(styleRecord.presentationStyle);
				objectStore.Destroy(styleRecord.inlineStyle);
			}
			record.typedStyles.clear();
		}

		void DestroyModelBucket(SdModelBucket& bucket)
		{
			for (SdModelSlot& slot : bucket.models)
				objectStore.Destroy(slot.record.object);
			bucket.models.clear();
		}

		void UpdateModelOwnership(SdModelRecord& modelRecord, SdModelLifetime lifetime, SdWidgetId ownerWidgetId) noexcept
		{
			if (modelRecord.lifetime == SdModelLifetime::Manual)
				return;
			modelRecord.lifetime = lifetime;
			modelRecord.ownerWidgetId = ownerWidgetId;
		}

		void BindWidgetModelOwner(SdResolvedKey resolvedKey, SdWidgetId widgetId) noexcept
		{
			SdModelBucket* bucket = FindModelBucket(resolvedKey);
			if (!bucket)
				return;
			for (SdModelSlot& slot : bucket->models)
			{
				SdModelRecord& modelRecord = slot.record;
				if (modelRecord.lifetime == SdModelLifetime::Widget && modelRecord.ownerWidgetId == 0)
					modelRecord.ownerWidgetId = widgetId;
			}
		}

	public:
		void Clear()
		{
			for (auto& [id, record] : widgetRecords)
				DestroyWidgetRecordObjects(record);
			for (SdModelBucket& bucket : modelBuckets)
				DestroyModelBucket(bucket);
			widgetRecords.clear();
			widgetIdByResolvedKey.clear();
			modelBucketByResolvedKey.clear();
			modelBuckets.clear();
#ifndef NDEBUG
			submittedIdsThisFrame.clear();
#endif
			styleNodes.clear();
			objectStore.Clear();
			stats = {};
		}

		void BeginFrame()
		{
#ifndef NDEBUG
			submittedIdsThisFrame.clear();
#endif
			stats.ResetFrameTransient();
			stats.modelCount = static_cast<SdUInt32>(modelBuckets.size());
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

#ifndef NDEBUG
		void CheckSubmittedWidgetId(
			SdWidgetId widgetId,
			SdWidgetId parentId,
			SdResolvedKey resolvedKey,
			SdUtf8StringView debugKey,
			SdTypeId widgetType)
		{
			SdSubmittedIdInfo info = {};
			info.parentId = parentId;
			info.resolvedKey = resolvedKey;
			info.debugKey.assign(debugKey.data(), debugKey.size());
			info.widgetType = widgetType;

			auto [it, inserted] = submittedIdsThisFrame.try_emplace(widgetId, std::move(info));
			(void)it;
			assert(inserted && SODIUM_STRING("Duplicate widget id detected. Use a unique key or push a different id scope."));
		}
#endif

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
#ifndef NDEBUG
			auto [it, inserted] = widgetIdByResolvedKey.try_emplace(resolvedKey, widgetId);
			assert(inserted || it->second == widgetId);
#else
			widgetIdByResolvedKey[resolvedKey] = widgetId;
#endif
			BindWidgetModelOwner(resolvedKey, widgetId);
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
			const SdTypeId type = SdStableTypeId<T>();
			Detail::SdObjectHandle* objectSlot = FindTypedObject(record.userStates, type);
			if (!objectSlot)
			{
				SdTypedObjectSlot& slot = record.userStates.emplace_back();
				slot.type = type;
				objectSlot = &slot.object;
			}
			Detail::SdObjectHandle& object = *objectSlot;
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
		T& GetOrCreateModel(SdResolvedKey resolvedKey, SdModelLifetime lifetime = SdModelLifetime::Manual, SdWidgetId ownerWidgetId = 0)
		{
			assert(resolvedKey != 0);
			SdModelBucket& bucket = GetOrCreateModelBucket(resolvedKey);
			const SdTypeId type = SdStableTypeId<T>();
			SdModelRecord* foundRecord = FindModelRecord(bucket, type);
			if (!foundRecord)
			{
				SdModelSlot& slot = bucket.models.emplace_back();
				slot.type = type;
				foundRecord = &slot.record;
			}
			SdModelRecord& modelRecord = *foundRecord;
			UpdateModelOwnership(modelRecord, lifetime, ownerWidgetId);
			if (!modelRecord.object.IsValid())
			{
				modelRecord.object = objectStore.Create<T>();
				modelRecord.lifetime = lifetime;
				modelRecord.ownerWidgetId = ownerWidgetId;
				stats.modelCount = static_cast<SdUInt32>(modelBuckets.size());
				stats.liveObjectCount = objectStore.GetLiveObjectCount();
				return *objectStore.Get<T>(modelRecord.object);
			}
			if (T* model = objectStore.Get<T>(modelRecord.object))
				return *model;
			objectStore.Destroy(modelRecord.object);
			modelRecord.object = objectStore.Create<T>();
			stats.liveObjectCount = objectStore.GetLiveObjectCount();
			return *objectStore.Get<T>(modelRecord.object);
		}

		bool RemoveModel(SdResolvedKey resolvedKey)
		{
			auto it = modelBucketByResolvedKey.find(resolvedKey);
			if (it == modelBucketByResolvedKey.end() || it->second >= modelBuckets.size())
				return false;
			DestroyModelBucket(modelBuckets[it->second]);
			EraseModelBucket(it->second);
			stats.modelCount = static_cast<SdUInt32>(modelBuckets.size());
			stats.liveObjectCount = objectStore.GetLiveObjectCount();
			return true;
		}

		SdUInt32 RemoveOwnedModels(SdWidgetId ownerWidgetId)
		{
			SdUInt32 removedCount = 0;
			for (SdSize bucketIndex = 0; bucketIndex < modelBuckets.size();)
			{
				SdModelBucket& bucket = modelBuckets[bucketIndex];
				for (auto modelIt = bucket.models.begin(); modelIt != bucket.models.end();)
				{
					SdModelRecord& modelRecord = modelIt->record;
					if (modelRecord.lifetime != SdModelLifetime::Manual && modelRecord.ownerWidgetId == ownerWidgetId)
					{
						objectStore.Destroy(modelRecord.object);
						modelIt = bucket.models.erase(modelIt);
						++removedCount;
					}
					else
					{
						++modelIt;
					}
				}

				if (bucket.models.empty())
					EraseModelBucket(bucketIndex);
				else
					++bucketIndex;
			}
			stats.modelCount = static_cast<SdUInt32>(modelBuckets.size());
			stats.liveObjectCount = objectStore.GetLiveObjectCount();
			return removedCount;
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
					RemoveOwnedModels(it->first);
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
			stats.modelCount = static_cast<SdUInt32>(modelBuckets.size());
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
			const SdTypeId type = SdStableTypeId<TStyle>();
			SdTypedStyleRecord* foundRecord = FindTypedStyle(record.typedStyles, type);
			if (!foundRecord)
			{
				SdTypedStyleSlot& slot = record.typedStyles.emplace_back();
				slot.type = type;
				foundRecord = &slot.record;
			}
			SdTypedStyleRecord& styleRecord = *foundRecord;
			if (styleRecord.styleType == 0)
				styleRecord.styleType = type;
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
			SdTypedStyleRecord* styleRecord = FindTypedStyle(record.typedStyles, SdStableTypeId<TStyle>());
			if (!styleRecord)
				return nullptr;
			return objectStore.Get<TStyle>(styleRecord->resolvedStyle);
		}

		template<class TStyle>
		TStyle* FindPresentationStyle(SdWidgetRecord& record) noexcept
		{
			SdTypedStyleRecord* styleRecord = FindTypedStyle(record.typedStyles, SdStableTypeId<TStyle>());
			if (!styleRecord)
				return nullptr;
			return objectStore.Get<TStyle>(styleRecord->presentationStyle);
		}

		template<class TStyle>
		const TStyle* FindInlineStyle(SdTypedStyleRecord& styleRecord) const noexcept
		{
			return const_cast<Detail::SdObjectStore&>(objectStore).Get<TStyle>(styleRecord.inlineStyle);
		}

		template<class TStyle>
		SdTypedStyleRecord* FindTypedStyleRecord(SdWidgetRecord& record) noexcept
		{
			return FindTypedStyle(record.typedStyles, SdStableTypeId<TStyle>());
		}

		template<class TStyle>
		const SdTypedStyleRecord* FindTypedStyleRecord(const SdWidgetRecord& record) const noexcept
		{
			return FindTypedStyle(record.typedStyles, SdStableTypeId<TStyle>());
		}

		template<class TStyle>
		const TStyle* FindInlineStyle(const SdWidgetRecord& record) const noexcept
		{
			const SdTypedStyleRecord* styleRecord = FindTypedStyleRecord<TStyle>(record);
			return styleRecord ? const_cast<Detail::SdObjectStore&>(objectStore).Get<TStyle>(styleRecord->inlineStyle) : nullptr;
		}

		template<class TStyle>
		SdUInt64 GetInlineStyleRevision(const SdWidgetRecord& record) const noexcept
		{
			const SdTypedStyleRecord* styleRecord = FindTypedStyleRecord<TStyle>(record);
			return styleRecord ? styleRecord->inlineStyleRevision : 0;
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
				if constexpr (requires(const TStyle& left, const TStyle& right) { left == right; })
				{
					if (*storedStyle == *inlineStyle)
						return false;
				}
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
			SdTypedStyleRecord* styleRecord = FindTypedStyle(record.typedStyles, SdStableTypeId<TStyle>());
			if (!styleRecord || !styleRecord->inlineStyle.IsValid())
				return false;

			objectStore.Destroy(styleRecord->inlineStyle);
			++styleRecord->inlineStyleRevision;
			if (styleRecord->inlineStyleRevision == 0)
				styleRecord->inlineStyleRevision = 1;
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
