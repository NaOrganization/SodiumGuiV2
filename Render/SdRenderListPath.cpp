#include "Render/SdRenderList.h"

#include <algorithm>
#include <cmath>

namespace Sodium
{
	namespace
	{
		SdVertex MakeVertex(const SdVec2& position, const SdVec2& uv, const SdColor& color)
		{
			return { position, uv, color.Pack() };
		}

		SdVec2 Normalize(const SdVec2& value)
		{
			const float length = std::sqrt((value.x * value.x) + (value.y * value.y));
			if (length <= 0.0f)
				return {};
			return { value.x / length, value.y / length };
		}

		SdVec2 AverageNormal(const SdVec2& left, const SdVec2& right)
		{
			float x = (left.x + right.x) * 0.5f;
			float y = (left.y + right.y) * 0.5f;
			float d2 = (x * x) + (y * y);
			if (d2 < 0.5f)
				d2 = 0.5f;
			const float invLenSq = 1.0f / d2;
			return { x * invLenSq, y * invLenSq };
		}
	}
	void SdRenderList::PathArcTo(const SdVec2& center, float radius, float angleMin, float angleMax, SdUInt32 segmentCount)
	{
		if (radius <= 0.0f)
			return;
		segmentCount = std::max<SdUInt32>(segmentCount, 3u);
		const float range = angleMax - angleMin;
		const SdSize start = path.size();
		PathReserve(segmentCount + 1);
		path.resize(start + segmentCount + 1);
		for (SdUInt32 index = 0; index <= segmentCount; ++index)
		{
			const float t = static_cast<float>(index) / static_cast<float>(segmentCount);
			const float angle = angleMin + (range * t);
			path[start + index] = { center.x + (std::cos(angle) * radius), center.y + (std::sin(angle) * radius) };
		}
	}

	void SdRenderList::PathArcToSkipFirst(const SdVec2& center, float radius, float angleMin, float angleMax, SdUInt32 segmentCount)
	{
		if (radius <= 0.0f)
			return;
		segmentCount = std::max<SdUInt32>(segmentCount, 3u);
		const float range = angleMax - angleMin;
		const SdSize start = path.size();
		PathReserve(segmentCount);
		path.resize(start + segmentCount);
		for (SdUInt32 index = 1; index <= segmentCount; ++index)
		{
			const float t = static_cast<float>(index) / static_cast<float>(segmentCount);
			const float angle = angleMin + (range * t);
			path[start + (index - 1)] = { center.x + (std::cos(angle) * radius), center.y + (std::sin(angle) * radius) };
		}
	}

	void SdRenderList::PathRect(const SdRect& rect, float rounding, SdUInt32 segmentCount)
	{
		PathClear();
		if (rounding <= 0.0f)
		{
			PathReserve(4);
			PathLineTo({ rect.min.x, rect.min.y });
			PathLineTo({ rect.max.x, rect.min.y });
			PathLineTo({ rect.max.x, rect.max.y });
			PathLineTo({ rect.min.x, rect.max.y });
			return;
		}

		const float radius = std::min({ rounding, rect.Width() * 0.5f, rect.Height() * 0.5f });
		segmentCount = std::max<SdUInt32>(segmentCount == 0 ? ResolveCircleSegmentCount(radius, 0) / 4 : segmentCount, 3u);
		PathReserve((segmentCount * 4) + 1);

		PathArcTo({ rect.max.x - radius, rect.min.y + radius }, radius, -SdPi * 0.5f, 0.0f, segmentCount);
		PathArcToSkipFirst({ rect.max.x - radius, rect.max.y - radius }, radius, 0.0f, SdPi * 0.5f, segmentCount);
		PathArcToSkipFirst({ rect.min.x + radius, rect.max.y - radius }, radius, SdPi * 0.5f, SdPi, segmentCount);
		PathArcToSkipFirst({ rect.min.x + radius, rect.min.y + radius }, radius, SdPi, SdPi * 1.5f, segmentCount);
	}

	void SdRenderList::PathFillConvex(const SdColor& color)
	{
		if (path.size() < 3)
			return;
		const SdTextureHandle texture = sharedData ? sharedData->whiteTexture : SdTextureHandle{};
		const SdVec2 opaqueUv = sharedData ? sharedData->whitePixelUv.min : SdVec2(0.0f, 0.0f);
		if (!UseAntiAliasing())
			PathFillConvexNoAA(color, texture, opaqueUv);
		else
			PathFillConvexAA(color, texture, opaqueUv);
	}

	void SdRenderList::PathFillConvexNoAA(const SdColor& color, SdTextureHandle texture, const SdVec2& opaqueUv)
	{
		if (!EnsureBatch(currentClipRect, texture, static_cast<SdUInt32>(path.size())))
			return;
		RequireSpace(static_cast<SdUInt32>(path.size()), static_cast<SdUInt32>((path.size() - 2) * 3));

		const SdUInt32 baseIndex = static_cast<SdUInt32>(drawData.vertices.size());
		SdVertex* vertices = AppendVertices(static_cast<SdUInt32>(path.size()));
		for (SdUInt32 index = 0; index < path.size(); ++index)
			vertices[index] = MakeVertex(path[index], opaqueUv, color);

		SdUInt32* indices = AppendIndices(static_cast<SdUInt32>((path.size() - 2) * 3));
		for (SdUInt32 index = 1; index + 1 < path.size(); ++index)
		{
			const SdUInt32 writeIndex = (index - 1u) * 3u;
			indices[writeIndex + 0] = baseIndex;
			indices[writeIndex + 1] = static_cast<SdUInt32>(baseIndex + index);
			indices[writeIndex + 2] = static_cast<SdUInt32>(baseIndex + index + 1);
		}
	}

	void SdRenderList::BuildPolylineNormals(bool closed)
	{
		const SdSize pointCount = path.size();
		const SdSize segmentCount = closed ? pointCount : pointCount - 1;
		EnsureScratch(scratchNormals, pointCount);
		for (SdSize i1 = 0; i1 < segmentCount; ++i1)
		{
			const SdSize i2 = (i1 + 1) == pointCount ? 0 : i1 + 1;
			const SdVec2 direction = Normalize(path[i2] - path[i1]);
			scratchNormals[i1] = { direction.y, -direction.x };
		}
		if (!closed && pointCount > 1)
			scratchNormals[pointCount - 1] = scratchNormals[pointCount - 2];
	}

	void SdRenderList::PathFillConvexAA(const SdColor& color, SdTextureHandle texture, const SdVec2& opaqueUv)
	{
		const SdUInt32 pointCount = static_cast<SdUInt32>(path.size());
		if (!EnsureBatch(currentClipRect, texture, pointCount * 2))
			return;
		RequireSpace(pointCount * 2, static_cast<SdUInt32>(((pointCount - 2) * 3) + (pointCount * 6)));

		const SdColor fringeColor(color.r, color.g, color.b, 0);
		BuildPolylineNormals(true);

		const SdUInt32 baseIndex = static_cast<SdUInt32>(drawData.vertices.size());
		SdUInt32* fillIndices = AppendIndices(static_cast<SdUInt32>((pointCount - 2) * 3));
		for (SdUInt32 i = 2; i < pointCount; ++i)
		{
			const SdUInt32 writeIndex = (i - 2u) * 3u;
			fillIndices[writeIndex + 0] = baseIndex;
			fillIndices[writeIndex + 1] = static_cast<SdUInt32>(baseIndex + ((i - 1u) << 1u));
			fillIndices[writeIndex + 2] = static_cast<SdUInt32>(baseIndex + (i << 1u));
		}

		SdUInt32* fringeIndices = AppendIndices(pointCount * 6);
		SdVertex* vertices = AppendVertices(pointCount * 2);
		for (SdSize i0 = path.size() - 1, i1 = 0; i1 < path.size(); i0 = i1++)
		{
			const SdVec2 normal = Normalize(scratchNormals[i0] + scratchNormals[i1]) * 0.5f;
			const SdVec2 inner = path[i1] - normal;
			const SdVec2 outer = path[i1] + normal;

			const SdUInt32 fringeIndex = static_cast<SdUInt32>(i1) * 6u;
			fringeIndices[fringeIndex + 0] = static_cast<SdUInt32>(baseIndex + (i1 << 1u));
			fringeIndices[fringeIndex + 1] = static_cast<SdUInt32>(baseIndex + (i0 << 1u));
			fringeIndices[fringeIndex + 2] = static_cast<SdUInt32>(baseIndex + ((i0 << 1u) + 1u));
			fringeIndices[fringeIndex + 3] = static_cast<SdUInt32>(baseIndex + ((i0 << 1u) + 1u));
			fringeIndices[fringeIndex + 4] = static_cast<SdUInt32>(baseIndex + ((i1 << 1u) + 1u));
			fringeIndices[fringeIndex + 5] = static_cast<SdUInt32>(baseIndex + (i1 << 1u));

			const SdUInt32 vertexIndex = static_cast<SdUInt32>(i1) * 2u;
			vertices[vertexIndex + 0] = MakeVertex(inner, opaqueUv, color);
			vertices[vertexIndex + 1] = MakeVertex(outer, opaqueUv, fringeColor);
		}
	}

	void SdRenderList::PathStroke(const SdColor& color, float thickness, bool closed)
	{
		if (path.size() < 2 || thickness <= 0.0f)
			return;

		const SdTextureHandle texture = sharedData ? sharedData->whiteTexture : SdTextureHandle{};
		const SdVec2 opaqueUv = sharedData ? sharedData->whitePixelUv.min : SdVec2(0.0f, 0.0f);
		const bool useAA = UseAntiAliasing();
		const bool thickLine = thickness > 1.0f;
		const SdUInt32 integerThickness = static_cast<SdUInt32>(thickness);
		const float fractionalThickness = thickness - static_cast<float>(integerThickness);
		const bool useTexture = integerThickness < SdRenderSharedData::BakedLineTextureCount && fractionalThickness <= 0.00001f;

		if (!useAA)
			PathStrokeNoAA(color, texture, opaqueUv, thickness, closed);
		else if (useTexture)
			PathStrokeAAThinTextured(color, texture, thickness, closed, integerThickness);
		else if (!thickLine)
			PathStrokeAAThinGeometry(color, texture, opaqueUv, thickness, closed);
		else
			PathStrokeAAThick(color, texture, opaqueUv, thickness, closed);
	}

	void SdRenderList::PathStrokeNoAA(const SdColor& color, SdTextureHandle texture, const SdVec2& opaqueUv, float thickness, bool closed)
	{
		const SdSize pointCount = path.size();
		const SdSize segmentCount = closed ? pointCount : pointCount - 1;
		if (!EnsureBatch(currentClipRect, texture, static_cast<SdUInt32>(segmentCount * 4)))
			return;
		RequireSpace(static_cast<SdUInt32>(segmentCount * 4), static_cast<SdUInt32>(segmentCount * 6));
		const SdUInt32 firstBaseIndex = static_cast<SdUInt32>(drawData.vertices.size());
		SdVertex* vertices = AppendVertices(static_cast<SdUInt32>(segmentCount * 4));
		SdUInt32* indices = AppendIndices(static_cast<SdUInt32>(segmentCount * 6));
		for (SdSize i1 = 0; i1 < segmentCount; ++i1)
		{
			const SdSize i2 = (i1 + 1) == pointCount ? 0 : i1 + 1;
			SdVec2 diff = path[i2] - path[i1];
			const float lengthSq = (diff.x * diff.x) + (diff.y * diff.y);
			if (lengthSq > 0.0f)
			{
				const float invLength = 1.0f / std::sqrt(lengthSq);
				diff = diff * invLength;
			}
			diff = diff * (thickness * 0.5f);
			const SdUInt32 baseIndex = static_cast<SdUInt32>(firstBaseIndex + (i1 * 4));
			const SdUInt32 vertexIndex = static_cast<SdUInt32>(i1 * 4);
			const SdUInt32 indexIndex = static_cast<SdUInt32>(i1 * 6);
			indices[indexIndex + 0] = baseIndex + 0;
			indices[indexIndex + 1] = baseIndex + 1;
			indices[indexIndex + 2] = baseIndex + 2;
			indices[indexIndex + 3] = baseIndex + 0;
			indices[indexIndex + 4] = baseIndex + 2;
			indices[indexIndex + 5] = baseIndex + 3;
			vertices[vertexIndex + 0] = MakeVertex({ path[i1].x + diff.y, path[i1].y - diff.x }, opaqueUv, color);
			vertices[vertexIndex + 1] = MakeVertex({ path[i2].x + diff.y, path[i2].y - diff.x }, opaqueUv, color);
			vertices[vertexIndex + 2] = MakeVertex({ path[i2].x - diff.y, path[i2].y + diff.x }, opaqueUv, color);
			vertices[vertexIndex + 3] = MakeVertex({ path[i1].x - diff.y, path[i1].y + diff.x }, opaqueUv, color);
		}
	}

	void SdRenderList::PathStrokeAAThinTextured(const SdColor& color, SdTextureHandle texture, float thickness, bool closed, SdUInt32 integerThickness)
	{
		const SdSize pointCount = path.size();
		const SdSize segmentCount = closed ? pointCount : pointCount - 1;
		const float halfDrawSize = (thickness * 0.5f) + 1.0f;
		const SdUInt32 vertexCount = static_cast<SdUInt32>(pointCount * 2);
		const SdUInt32 indexCount = static_cast<SdUInt32>(segmentCount * 6);
		if (!EnsureBatch(currentClipRect, texture, vertexCount))
			return;
		RequireSpace(vertexCount, indexCount);

		BuildPolylineNormals(closed);
		EnsureScratch(scratchPoints2, pointCount * 2);
		if (!closed)
		{
			scratchPoints2[0] = path[0] + (scratchNormals[0] * halfDrawSize);
			scratchPoints2[1] = path[0] - (scratchNormals[0] * halfDrawSize);
			scratchPoints2[(pointCount - 1) * 2 + 0] = path[pointCount - 1] + (scratchNormals[pointCount - 1] * halfDrawSize);
			scratchPoints2[(pointCount - 1) * 2 + 1] = path[pointCount - 1] - (scratchNormals[pointCount - 1] * halfDrawSize);
		}

		SdUInt32 idx1 = static_cast<SdUInt32>(drawData.vertices.size());
		SdUInt32* indices = AppendIndices(indexCount);
		for (SdSize i1 = 0; i1 < segmentCount; ++i1)
		{
			const SdSize i2 = (i1 + 1) == pointCount ? 0 : i1 + 1;
			const SdUInt32 idx2 = ((i1 + 1) == pointCount) ? static_cast<SdUInt32>(drawData.vertices.size()) : static_cast<SdUInt32>(idx1 + 2);
			const SdVec2 average = AverageNormal(scratchNormals[i1], scratchNormals[i2]) * halfDrawSize;
			scratchPoints2[i2 * 2 + 0] = path[i2] + average;
			scratchPoints2[i2 * 2 + 1] = path[i2] - average;

			const SdUInt32 writeIndex = static_cast<SdUInt32>(i1) * 6u;
			indices[writeIndex + 0] = static_cast<SdUInt32>(idx2 + 0);
			indices[writeIndex + 1] = static_cast<SdUInt32>(idx1 + 0);
			indices[writeIndex + 2] = static_cast<SdUInt32>(idx1 + 1);
			indices[writeIndex + 3] = static_cast<SdUInt32>(idx2 + 1);
			indices[writeIndex + 4] = static_cast<SdUInt32>(idx1 + 1);
			indices[writeIndex + 5] = static_cast<SdUInt32>(idx2 + 0);
			idx1 = idx2;
		}

		const SdVec4 uvRange = sharedData ? sharedData->bakedLineUvs[integerThickness] : SdVec4(0.0f, 0.0f, 1.0f, 1.0f);
		const SdVec2 uv0(uvRange.x, uvRange.y);
		const SdVec2 uv1(uvRange.z, uvRange.w);
		SdVertex* vertices = AppendVertices(vertexCount);
		for (SdSize i = 0; i < pointCount; ++i)
		{
			const SdUInt32 vertexIndex = static_cast<SdUInt32>(i) * 2u;
			vertices[vertexIndex + 0] = MakeVertex(scratchPoints2[i * 2 + 0], uv0, color);
			vertices[vertexIndex + 1] = MakeVertex(scratchPoints2[i * 2 + 1], uv1, color);
		}
	}

	void SdRenderList::PathStrokeAAThinGeometry(const SdColor& color, SdTextureHandle texture, const SdVec2& opaqueUv, float thickness, bool closed)
	{
		const SdSize pointCount = path.size();
		const SdSize segmentCount = closed ? pointCount : pointCount - 1;
		const float halfDrawSize = std::max(1.0f, thickness * 0.5f);
		const SdUInt32 vertexCount = static_cast<SdUInt32>(pointCount * 3);
		const SdUInt32 indexCount = static_cast<SdUInt32>(segmentCount * 12);
		if (!EnsureBatch(currentClipRect, texture, vertexCount))
			return;
		RequireSpace(vertexCount, indexCount);

		BuildPolylineNormals(closed);
		EnsureScratch(scratchPoints2, pointCount * 2);
		if (!closed)
		{
			scratchPoints2[0] = path[0] + (scratchNormals[0] * halfDrawSize);
			scratchPoints2[1] = path[0] - (scratchNormals[0] * halfDrawSize);
			scratchPoints2[(pointCount - 1) * 2 + 0] = path[pointCount - 1] + (scratchNormals[pointCount - 1] * halfDrawSize);
			scratchPoints2[(pointCount - 1) * 2 + 1] = path[pointCount - 1] - (scratchNormals[pointCount - 1] * halfDrawSize);
		}

		SdUInt32 idx1 = static_cast<SdUInt32>(drawData.vertices.size());
		SdUInt32* indices = AppendIndices(indexCount);
		for (SdSize i1 = 0; i1 < segmentCount; ++i1)
		{
			const SdSize i2 = (i1 + 1) == pointCount ? 0 : i1 + 1;
			const SdUInt32 idx2 = ((i1 + 1) == pointCount) ? static_cast<SdUInt32>(drawData.vertices.size()) : static_cast<SdUInt32>(idx1 + 3);
			const SdVec2 average = AverageNormal(scratchNormals[i1], scratchNormals[i2]) * halfDrawSize;
			scratchPoints2[i2 * 2 + 0] = path[i2] + average;
			scratchPoints2[i2 * 2 + 1] = path[i2] - average;

			const SdUInt32 writeIndex = static_cast<SdUInt32>(i1) * 12u;
			indices[writeIndex + 0] = static_cast<SdUInt32>(idx2 + 0);
			indices[writeIndex + 1] = static_cast<SdUInt32>(idx1 + 0);
			indices[writeIndex + 2] = static_cast<SdUInt32>(idx1 + 2);
			indices[writeIndex + 3] = static_cast<SdUInt32>(idx1 + 2);
			indices[writeIndex + 4] = static_cast<SdUInt32>(idx2 + 2);
			indices[writeIndex + 5] = static_cast<SdUInt32>(idx2 + 0);
			indices[writeIndex + 6] = static_cast<SdUInt32>(idx2 + 1);
			indices[writeIndex + 7] = static_cast<SdUInt32>(idx1 + 1);
			indices[writeIndex + 8] = static_cast<SdUInt32>(idx1 + 0);
			indices[writeIndex + 9] = static_cast<SdUInt32>(idx1 + 0);
			indices[writeIndex + 10] = static_cast<SdUInt32>(idx2 + 0);
			indices[writeIndex + 11] = static_cast<SdUInt32>(idx2 + 1);
			idx1 = idx2;
		}

		const SdColor fringeColor(color.r, color.g, color.b, 0);
		SdVertex* vertices = AppendVertices(vertexCount);
		for (SdSize i = 0; i < pointCount; ++i)
		{
			const SdUInt32 vertexIndex = static_cast<SdUInt32>(i) * 3u;
			vertices[vertexIndex + 0] = MakeVertex(path[i], opaqueUv, color);
			vertices[vertexIndex + 1] = MakeVertex(scratchPoints2[i * 2 + 0], opaqueUv, fringeColor);
			vertices[vertexIndex + 2] = MakeVertex(scratchPoints2[i * 2 + 1], opaqueUv, fringeColor);
		}
	}

	void SdRenderList::PathStrokeAAThick(const SdColor& color, SdTextureHandle texture, const SdVec2& opaqueUv, float thickness, bool closed)
	{
		const SdSize pointCount = path.size();
		const SdSize segmentCount = closed ? pointCount : pointCount - 1;
		const float halfInnerThickness = (thickness - 1.0f) * 0.5f;
		const SdUInt32 vertexCount = static_cast<SdUInt32>(pointCount * 4);
		const SdUInt32 indexCount = static_cast<SdUInt32>(segmentCount * 18);
		if (!EnsureBatch(currentClipRect, texture, vertexCount))
			return;
		RequireSpace(vertexCount, indexCount);

		BuildPolylineNormals(closed);
		EnsureScratch(scratchPoints4, pointCount * 4);
		if (!closed)
		{
			const SdSize last = pointCount - 1;
			scratchPoints4[0] = path[0] + (scratchNormals[0] * (halfInnerThickness + 1.0f));
			scratchPoints4[1] = path[0] + (scratchNormals[0] * halfInnerThickness);
			scratchPoints4[2] = path[0] - (scratchNormals[0] * halfInnerThickness);
			scratchPoints4[3] = path[0] - (scratchNormals[0] * (halfInnerThickness + 1.0f));
			scratchPoints4[last * 4 + 0] = path[last] + (scratchNormals[last] * (halfInnerThickness + 1.0f));
			scratchPoints4[last * 4 + 1] = path[last] + (scratchNormals[last] * halfInnerThickness);
			scratchPoints4[last * 4 + 2] = path[last] - (scratchNormals[last] * halfInnerThickness);
			scratchPoints4[last * 4 + 3] = path[last] - (scratchNormals[last] * (halfInnerThickness + 1.0f));
		}

		SdUInt32 idx1 = static_cast<SdUInt32>(drawData.vertices.size());
		SdUInt32* indices = AppendIndices(indexCount);
		for (SdSize i1 = 0; i1 < segmentCount; ++i1)
		{
			const SdSize i2 = (i1 + 1) == pointCount ? 0 : i1 + 1;
			const SdUInt32 idx2 = (i1 + 1) == pointCount ? static_cast<SdUInt32>(drawData.vertices.size()) : static_cast<SdUInt32>(idx1 + 4);
			const SdVec2 average = AverageNormal(scratchNormals[i1], scratchNormals[i2]);
			const SdVec2 outerOffset = average * (halfInnerThickness + 1.0f);
			const SdVec2 innerOffset = average * halfInnerThickness;

			scratchPoints4[i2 * 4 + 0] = path[i2] + outerOffset;
			scratchPoints4[i2 * 4 + 1] = path[i2] + innerOffset;
			scratchPoints4[i2 * 4 + 2] = path[i2] - innerOffset;
			scratchPoints4[i2 * 4 + 3] = path[i2] - outerOffset;

			const SdUInt32 writeIndex = static_cast<SdUInt32>(i1) * 18u;
			indices[writeIndex + 0] = static_cast<SdUInt32>(idx2 + 1);
			indices[writeIndex + 1] = static_cast<SdUInt32>(idx1 + 1);
			indices[writeIndex + 2] = static_cast<SdUInt32>(idx1 + 2);
			indices[writeIndex + 3] = static_cast<SdUInt32>(idx1 + 2);
			indices[writeIndex + 4] = static_cast<SdUInt32>(idx2 + 2);
			indices[writeIndex + 5] = static_cast<SdUInt32>(idx2 + 1);
			indices[writeIndex + 6] = static_cast<SdUInt32>(idx2 + 1);
			indices[writeIndex + 7] = static_cast<SdUInt32>(idx1 + 1);
			indices[writeIndex + 8] = static_cast<SdUInt32>(idx1 + 0);
			indices[writeIndex + 9] = static_cast<SdUInt32>(idx1 + 0);
			indices[writeIndex + 10] = static_cast<SdUInt32>(idx2 + 0);
			indices[writeIndex + 11] = static_cast<SdUInt32>(idx2 + 1);
			indices[writeIndex + 12] = static_cast<SdUInt32>(idx2 + 2);
			indices[writeIndex + 13] = static_cast<SdUInt32>(idx1 + 2);
			indices[writeIndex + 14] = static_cast<SdUInt32>(idx1 + 3);
			indices[writeIndex + 15] = static_cast<SdUInt32>(idx1 + 3);
			indices[writeIndex + 16] = static_cast<SdUInt32>(idx2 + 3);
			indices[writeIndex + 17] = static_cast<SdUInt32>(idx2 + 2);
			idx1 = idx2;
		}

		const SdColor fringeColor(color.r, color.g, color.b, 0);
		SdVertex* vertices = AppendVertices(vertexCount);
		for (SdSize i = 0; i < pointCount; ++i)
		{
			const SdUInt32 vertexIndex = static_cast<SdUInt32>(i) * 4u;
			vertices[vertexIndex + 0] = MakeVertex(scratchPoints4[i * 4 + 0], opaqueUv, fringeColor);
			vertices[vertexIndex + 1] = MakeVertex(scratchPoints4[i * 4 + 1], opaqueUv, color);
			vertices[vertexIndex + 2] = MakeVertex(scratchPoints4[i * 4 + 2], opaqueUv, color);
			vertices[vertexIndex + 3] = MakeVertex(scratchPoints4[i * 4 + 3], opaqueUv, fringeColor);
		}
	}
}
