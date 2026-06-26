#pragma once

#include "Effects/SdEffect.hpp"

#include <memory>

namespace Sodium
{
	class SdEffectRegistry final
	{
	private:
		struct Entry final
		{
			std::unique_ptr<ISdEffector> effector = {};
			SdUInt32 generation = 0;
			bool occupied = false;
			bool initialized = false;
		};

		std::vector<Entry> entries = {};
		Rhi::ISdGpuDevice* device = nullptr;

		static SdEffectHandle Allocate(std::vector<Entry>& entries)
		{
			SdUInt32 index = 1;
			for (; index < entries.size(); ++index)
			{
				if (!entries[index].occupied)
					break;
			}
			if (entries.size() <= index)
				entries.resize(index + 1);

			Entry& entry = entries[index];
			entry.generation = entry.generation == 0 ? 1 : entry.generation + 1;
			entry.occupied = true;
			return SdEffectHandle(index, entry.generation);
		}

		Entry* FindEntry(SdEffectHandle handle) noexcept
		{
			if (!handle.IsValid() || handle.index >= entries.size())
				return nullptr;
			Entry& entry = entries[handle.index];
			if (!entry.occupied || entry.generation != handle.generation)
				return nullptr;
			return &entry;
		}

		const Entry* FindEntry(SdEffectHandle handle) const noexcept
		{
			if (!handle.IsValid() || handle.index >= entries.size())
				return nullptr;
			const Entry& entry = entries[handle.index];
			if (!entry.occupied || entry.generation != handle.generation)
				return nullptr;
			return &entry;
		}

		bool InitializeEntry(Entry& entry)
		{
			if (!device || !entry.effector || entry.initialized)
				return true;
			const SdEffectInitContext context =
			{
				*device,
				device->GetCaps()
			};
			entry.initialized = entry.effector->Initialize(context);
			return entry.initialized;
		}

	public:
		SdEffectRegistry() = default;
		~SdEffectRegistry()
		{
			Shutdown();
		}

		SdEffectRegistry(const SdEffectRegistry&) = delete;
		SdEffectRegistry& operator=(const SdEffectRegistry&) = delete;

		bool Initialize(Rhi::ISdGpuDevice& gpuDevice)
		{
			if (device == &gpuDevice)
				return true;

			Shutdown();
			device = &gpuDevice;

			for (Entry& entry : entries)
			{
				if (entry.occupied && entry.effector && !InitializeEntry(entry))
				{
					Shutdown();
					return false;
				}
			}
			return true;
		}

		void Shutdown() noexcept
		{
			if (!device)
				return;

			for (Entry& entry : entries)
			{
				if (entry.occupied && entry.initialized && entry.effector)
					entry.effector->Shutdown(*device);
				entry.initialized = false;
			}
			device = nullptr;
		}

		SdEffectHandle Register(std::unique_ptr<ISdEffector> effector)
		{
			if (!effector)
				return {};

			const SdEffectHandle handle = Allocate(entries);
			Entry& entry = entries[handle.index];
			entry.effector = std::move(effector);
			entry.initialized = false;

			if (device && !InitializeEntry(entry))
			{
				Unregister(handle);
				return {};
			}
			return handle;
		}

		template<class TEffect, class... TArgs>
		SdEffectHandle Register(TArgs&&... args)
		{
			return Register(std::make_unique<TEffect>(std::forward<TArgs>(args)...));
		}

		void Unregister(SdEffectHandle handle) noexcept
		{
			Entry* entry = FindEntry(handle);
			if (!entry)
				return;

			if (device && entry->initialized && entry->effector)
				entry->effector->Shutdown(*device);
			entry->effector.reset();
			entry->occupied = false;
			entry->initialized = false;
			entry->generation = handle.generation + 1;
		}

		ISdEffector* Find(SdEffectHandle handle) noexcept
		{
			Entry* entry = FindEntry(handle);
			return entry ? entry->effector.get() : nullptr;
		}

		const ISdEffector* Find(SdEffectHandle handle) const noexcept
		{
			const Entry* entry = FindEntry(handle);
			return entry ? entry->effector.get() : nullptr;
		}

		ISdEffector* FindMutable(SdEffectHandle handle) const noexcept
		{
			const Entry* entry = FindEntry(handle);
			return entry ? entry->effector.get() : nullptr;
		}

		bool IsInitialized() const noexcept
		{
			return device != nullptr;
		}

		Rhi::ISdGpuDevice* GetDevice() const noexcept
		{
			return device;
		}
	};
}
