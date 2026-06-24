#include "Render/SdDrawList.h"

#include <cmath>

namespace Sodium
{
	namespace
	{
		SdVertex MakeVertex(const SdVec2& position, const SdVec2& uv, const SdColor& color)
		{
			return { position, uv, color.Pack() };
		}
	}
	void SdRenderList::PrimRectWithUV(const SdRect& rect, const SdRect& uvRect, const SdColor& color, const SdRect& clipRect, SdTextureHandle texture)
	{
		if (!EnsureBatch(clipRect, texture, 4))
			return;
		RequireSpace(4, 6);

		const SdUInt32 baseIndex = static_cast<SdUInt32>(drawData.vertices.size());
		SdVertex* vertices = AppendVertices(4);
		vertices[0] = MakeVertex({ rect.min.x, rect.min.y }, { uvRect.min.x, uvRect.min.y }, color);
		vertices[1] = MakeVertex({ rect.max.x, rect.min.y }, { uvRect.max.x, uvRect.min.y }, color);
		vertices[2] = MakeVertex({ rect.max.x, rect.max.y }, { uvRect.max.x, uvRect.max.y }, color);
		vertices[3] = MakeVertex({ rect.min.x, rect.max.y }, { uvRect.min.x, uvRect.max.y }, color);

		SdUInt32* indices = AppendIndices(6);
		indices[0] = baseIndex + 0;
		indices[1] = baseIndex + 1;
		indices[2] = baseIndex + 2;
		indices[3] = baseIndex + 0;
		indices[4] = baseIndex + 2;
		indices[5] = baseIndex + 3;
	}

	void SdRenderList::AddLine(const SdVec2& a, const SdVec2& b, const SdColor& color, const SdRect& clipRect, float thickness)
	{
		PathClear();
		PathLineTo(a);
		PathLineTo(b);
		PathStroke(color, clipRect, thickness, false);
	}

	void SdRenderList::AddTriangle(const SdVec2& a, const SdVec2& b, const SdVec2& c, const SdColor& color, const SdRect& clipRect, float thickness)
	{
		PathClear();
		PathLineTo(a);
		PathLineTo(b);
		PathLineTo(c);
		PathStroke(color, clipRect, thickness, true);
	}

	void SdRenderList::AddTriangleFilled(const SdVec2& a, const SdVec2& b, const SdVec2& c, const SdColor& color, const SdRect& clipRect)
	{
		PathClear();
		PathLineTo(a);
		PathLineTo(b);
		PathLineTo(c);
		PathFillConvex(color, clipRect);
	}

	void SdRenderList::AddQuad(const SdVec2& a, const SdVec2& b, const SdVec2& c, const SdVec2& d, const SdColor& color, const SdRect& clipRect, float thickness)
	{
		PathClear();
		PathLineTo(a);
		PathLineTo(b);
		PathLineTo(c);
		PathLineTo(d);
		PathStroke(color, clipRect, thickness, true);
	}

	void SdRenderList::AddQuadFilled(const SdVec2& a, const SdVec2& b, const SdVec2& c, const SdVec2& d, const SdColor& color, const SdRect& clipRect)
	{
		PathClear();
		PathLineTo(a);
		PathLineTo(b);
		PathLineTo(c);
		PathLineTo(d);
		PathFillConvex(color, clipRect);
	}

	void SdRenderList::AddRect(const SdRect& rect, const SdColor& color, const SdRect& clipRect, float thickness, float rounding, SdUInt32 roundingSegments)
	{
		PathRect(rect, rounding, roundingSegments);
		PathStroke(color, clipRect, thickness, true);
	}

	void SdRenderList::AddRectFilled(const SdRect& rect, const SdColor& color, const SdRect& clipRect, float rounding, SdUInt32 roundingSegments)
	{
		PathRect(rect, rounding, roundingSegments);
		PathFillConvex(color, clipRect);
	}

	void SdRenderList::AddRectFilledMultiColor(const SdRect& rect, const SdColor& colorUpperLeft, const SdColor& colorUpperRight, const SdColor& colorBottomRight, const SdColor& colorBottomLeft, const SdRect& clipRect)
	{
		const SdTextureHandle texture = sharedData ? sharedData->whiteTexture : SdTextureHandle{};
		if (!EnsureBatch(clipRect, texture, 4))
			return;
		RequireSpace(4, 6);

		const SdVec2 uv = sharedData ? sharedData->whitePixelUv.min : SdVec2(0.0f, 0.0f);
		const SdUInt32 baseIndex = static_cast<SdUInt32>(drawData.vertices.size());
		SdUInt32* indices = AppendIndices(6);
		indices[0] = baseIndex + 0;
		indices[1] = baseIndex + 1;
		indices[2] = baseIndex + 2;
		indices[3] = baseIndex + 0;
		indices[4] = baseIndex + 2;
		indices[5] = baseIndex + 3;
		SdVertex* vertices = AppendVertices(4);
		vertices[0] = MakeVertex({ rect.min.x, rect.min.y }, uv, colorUpperLeft);
		vertices[1] = MakeVertex({ rect.max.x, rect.min.y }, uv, colorUpperRight);
		vertices[2] = MakeVertex({ rect.max.x, rect.max.y }, uv, colorBottomRight);
		vertices[3] = MakeVertex({ rect.min.x, rect.max.y }, uv, colorBottomLeft);
	}

	void SdRenderList::AddCircle(const SdVec2& center, float radius, const SdColor& color, const SdRect& clipRect, float thickness, SdUInt32 segmentCount)
	{
		if (radius <= 0.0f)
			return;
		PathClear();
		segmentCount = ResolveCircleSegmentCount(radius, segmentCount);
		PathReserve(segmentCount);
		for (SdUInt32 segment = 0; segment < segmentCount; ++segment)
		{
			const float angle = (static_cast<float>(segment) / static_cast<float>(segmentCount)) * SdPi * 2.0f;
			PathLineTo({ center.x + (std::cos(angle) * radius), center.y + (std::sin(angle) * radius) });
		}
		PathStroke(color, clipRect, thickness, true);
	}

	void SdRenderList::AddCircleFilled(const SdVec2& center, float radius, const SdColor& color, const SdRect& clipRect, SdUInt32 segmentCount)
	{
		if (radius <= 0.0f)
			return;
		PathClear();
		segmentCount = ResolveCircleSegmentCount(radius, segmentCount);
		PathReserve(segmentCount);
		for (SdUInt32 segment = 0; segment < segmentCount; ++segment)
		{
			const float angle = (static_cast<float>(segment) / static_cast<float>(segmentCount)) * SdPi * 2.0f;
			PathLineTo({ center.x + (std::cos(angle) * radius), center.y + (std::sin(angle) * radius) });
		}
		PathFillConvex(color, clipRect);
	}

	void SdRenderList::AddImage(SdTextureHandle texture, const SdRect& rect, const SdRect& uvRect, const SdColor& color, const SdRect& clipRect)
	{
		PrimRectWithUV(rect, uvRect, color, clipRect, texture);
	}
}
