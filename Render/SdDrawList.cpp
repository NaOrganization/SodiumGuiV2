#include "Render/SdDrawList.h"

#include <algorithm>
#include <cmath>

namespace Sodium
{
	namespace
	{
		constexpr SdUInt64 SdMaxIndexValue = 0xFFFFFFFFull;

		bool RectEquals(const SdRect& left, const SdRect& right)
		{
			return left.min.x == right.min.x
				&& left.min.y == right.min.y
				&& left.max.x == right.max.x
				&& left.max.y == right.max.y;
		}
	}

	SdRenderList::SdRenderList(SdRenderStats* renderStats, SdRenderSharedData* renderSharedData)
		: stats(renderStats), sharedData(renderSharedData)
	{
	}

	void SdRenderList::Reset()
	{
		drawData.Clear();
		path.clear();
		currentTexture = sharedData ? sharedData->whiteTexture : SdTextureHandle{};
		listFlags = sharedData && (sharedData->flags & SdRenderSharedData::UseAntiAliasing)
			? static_cast<SdUInt32>(ListFlag::UseAntiAliasing)
			: static_cast<SdUInt32>(ListFlag::None);
		currentClipRect = {};
		if (stats)
		{
			stats->maxVertexCapacity = std::max(stats->maxVertexCapacity, static_cast<SdUInt32>(drawData.vertices.capacity()));
			stats->maxIndexCapacity = std::max(stats->maxIndexCapacity, static_cast<SdUInt32>(drawData.indices.capacity()));
		}
	}

	void SdRenderList::SetWhiteTexture(SdTextureHandle texture)
	{
		if (sharedData)
			sharedData->whiteTexture = texture;
		currentTexture = texture;
	}

	void SdRenderList::PathReserve(SdUInt32 pointCount)
	{
		if (pointCount != 0 && path.size() + pointCount > path.capacity())
			path.reserve(path.size() + pointCount);
	}

	void SdRenderList::EnsureScratch(std::vector<SdVec2>& scratch, SdSize count)
	{
		if (count > scratch.capacity())
		{
			if (stats)
				stats->aaScratchGrowCount++;
			scratch.reserve(count);
		}
		scratch.resize(count);
	}

	bool SdRenderList::UseAntiAliasing() const noexcept
	{
		return (listFlags & static_cast<SdUInt32>(ListFlag::UseAntiAliasing)) != 0;
	}

	void SdRenderList::TrackReserveGrowth(SdUInt32 vertexReserve, SdUInt32 indexReserve, SdUInt32 batchReserve)
	{
		if (vertexReserve != 0 && drawData.vertices.size() + vertexReserve > drawData.vertices.capacity())
		{
			if (stats)
				stats->renderListGrowCount++;
			drawData.vertices.reserve(drawData.vertices.size() + vertexReserve);
		}
		if (indexReserve != 0 && drawData.indices.size() + indexReserve > drawData.indices.capacity())
		{
			if (stats)
				stats->renderListGrowCount++;
			drawData.indices.reserve(drawData.indices.size() + indexReserve);
		}
		if (batchReserve != 0 && drawData.batches.size() + batchReserve > drawData.batches.capacity())
		{
			if (stats)
				stats->renderListGrowCount++;
			drawData.batches.reserve(drawData.batches.size() + batchReserve);
			drawData.commands.reserve(drawData.commands.size() + batchReserve);
		}
		if (stats)
		{
			stats->maxVertexCapacity = std::max(stats->maxVertexCapacity, static_cast<SdUInt32>(drawData.vertices.capacity()));
			stats->maxIndexCapacity = std::max(stats->maxIndexCapacity, static_cast<SdUInt32>(drawData.indices.capacity()));
		}
	}

	void SdRenderList::Reserve(SdUInt32 vertexCount, SdUInt32 indexCount, SdUInt32 batchCount)
	{
		TrackReserveGrowth(vertexCount, indexCount, batchCount);
	}

	void SdRenderList::TrackCurrentBatchVertices(SdUInt32 vertexCount)
	{
		if (!drawData.batches.empty())
			drawData.batches.back().vertexCount += vertexCount;
	}

	SdVertex* SdRenderList::AppendVertices(SdUInt32 vertexCount)
	{
		if (vertexCount == 0)
			return nullptr;
		const SdSize start = drawData.vertices.size();
		drawData.vertices.resize(start + vertexCount);
		TrackCurrentBatchVertices(vertexCount);
		return drawData.vertices.data() + start;
	}

	SdUInt32* SdRenderList::AppendIndices(SdUInt32 indexCount)
	{
		if (indexCount == 0)
			return nullptr;
		const SdSize start = drawData.indices.size();
		drawData.indices.resize(start + indexCount);
		if (!drawData.batches.empty())
			drawData.batches.back().indexCount += indexCount;
		return drawData.indices.data() + start;
	}

	void SdRenderList::RequireSpace(SdUInt32 vertexCount, SdUInt32 indexCount)
	{
		TrackReserveGrowth(vertexCount, indexCount, 0);
	}

	bool SdRenderList::CanAddVertices(SdUInt32 vertexCount) const
	{
		return static_cast<SdUInt64>(drawData.vertices.size()) + vertexCount <= SdMaxIndexValue;
	}

	bool SdRenderList::EnsureBatch(const SdRect& clipRect, SdTextureHandle texture, SdUInt32 vertexReserve)
	{
		if (!CanAddVertices(vertexReserve))
			return false;

		if (!drawData.batches.empty())
		{
			SdRenderBatch& current = drawData.batches.back();
			if (current.texture == texture && RectEquals(current.clipRect, clipRect))
			{
				return true;
			}
		}

		TrackReserveGrowth(vertexReserve, 0, 1);
		SdRenderBatch batch = {};
		batch.vertexOffset = static_cast<SdUInt32>(drawData.vertices.size());
		batch.indexOffset = static_cast<SdUInt32>(drawData.indices.size());
		batch.texture = texture;
		batch.clipRect = clipRect;
		drawData.batches.push_back(batch);
		drawData.commands.push_back(SdDrawCommand{
			SdDrawCommandKind::OwnedBatch,
			static_cast<SdUInt32>(drawData.batches.size() - 1)
		});
		return true;
	}

	SdUInt32 SdRenderList::ResolveCircleSegmentCount(float radius, SdUInt32 requestedSegmentCount)
	{
		if (requestedSegmentCount != 0)
			return std::clamp<SdUInt32>(requestedSegmentCount, 3, 256);
		return std::clamp<SdUInt32>(static_cast<SdUInt32>(std::ceil(radius * 0.35f)) + 10u, 12, 128);
	}
}
