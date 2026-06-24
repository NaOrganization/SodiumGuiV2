#pragma once

#include "Rhi/SdRhi.h"

#include <algorithm>
#include <vector>

namespace Sodium::Rhi
{
	struct SdTransientTextureKey final
	{
		SdUInt32 width = 0;
		SdUInt32 height = 0;
		SdTextureFormat format = SdTextureFormat::Unknown;
		SdTextureUsageFlags usage = 0;
		SdUInt32 sampleCount = 1;

		friend constexpr bool operator==(const SdTransientTextureKey&, const SdTransientTextureKey&) = default;
	};

	class SdTransientTexturePool final
	{
	private:
		struct Entry final
		{
			SdTransientTextureKey key = {};
			SdTextureHandle texture = {};
			SdUInt64 lastUsedFrame = 0;
			bool inUse = false;
		};

		ISdGpuDevice* device = nullptr;
		std::vector<Entry> entries = {};
		SdUInt64 frameIndex = 0;

		static SdTransientTextureKey MakeKey(const SdTextureDesc& desc) noexcept
		{
			return {
				desc.width,
				desc.height,
				desc.format,
				desc.usage,
				desc.sampleCount
			};
		}

	public:
		explicit SdTransientTexturePool(ISdGpuDevice& gpuDevice)
			: device(&gpuDevice) {}

		SdTransientTexturePool(const SdTransientTexturePool&) = delete;
		SdTransientTexturePool& operator=(const SdTransientTexturePool&) = delete;

		void BeginFrame()
		{
			++frameIndex;
		}

		void EndFrame()
		{
			for (Entry& entry : entries)
				entry.inUse = false;
		}

		SdTextureHandle Acquire(const SdTextureDesc& desc)
		{
			if (!device || desc.width == 0 || desc.height == 0)
				return {};

			const SdTransientTextureKey key = MakeKey(desc);
			for (Entry& entry : entries)
			{
				if (!entry.inUse && entry.key == key)
				{
					entry.inUse = true;
					entry.lastUsedFrame = frameIndex;
					return entry.texture;
				}
			}

			SdTextureDesc textureDesc = desc;
			textureDesc.isTransient = true;
			SdTextureHandle texture = device->CreateTexture(textureDesc);
			if (!texture.IsValid())
				return {};

			entries.push_back({ key, texture, frameIndex, true });
			return texture;
		}

		void Release(SdTextureHandle texture)
		{
			if (!texture.IsValid())
				return;

			for (Entry& entry : entries)
			{
				if (entry.texture == texture)
				{
					entry.inUse = false;
					entry.lastUsedFrame = frameIndex;
					return;
				}
			}
		}

		void TrimUnused(SdUInt32 maxIdleFrameCount)
		{
			if (!device)
				return;

			const SdUInt64 maxIdle = maxIdleFrameCount;
			auto write = entries.begin();
			for (auto read = entries.begin(); read != entries.end(); ++read)
			{
				const bool expired = !read->inUse && frameIndex >= read->lastUsedFrame && (frameIndex - read->lastUsedFrame) > maxIdle;
				if (expired)
				{
					device->DestroyTexture(read->texture);
					continue;
				}

				if (write != read)
					*write = *read;
				++write;
			}
			entries.erase(write, entries.end());
		}

		SdSize GetCachedTextureCount() const noexcept { return entries.size(); }
	};
}
