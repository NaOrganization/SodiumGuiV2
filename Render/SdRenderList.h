#pragma once

#include "Render/SdDrawData.h"
#include "Render/SdRenderStats.h"
#include "Core/SdText.h"

#include <stack>

namespace Sodium
{
	class SdRenderList final
	{
	private:
		enum class ListFlag : SdUInt32
		{
			None = 0,
			UseAntiAliasing = 1 << 0
		};

		SdRenderData drawData = {};

		std::stack<SdRect> clipStack = {};
		SdRect currentClipRect = {};

		SdRenderStats* stats = nullptr;
		SdRenderSharedData* sharedData = nullptr;
		SdTextureHandle currentTexture = {};
		std::vector<SdVec2> path = {};
		std::vector<SdVec2> scratchNormals = {};
		std::vector<SdVec2> scratchPoints2 = {};
		std::vector<SdVec2> scratchPoints4 = {};
		SdUInt32 listFlags = static_cast<SdUInt32>(ListFlag::UseAntiAliasing);
		bool forceNewBatch = false;

		void PathReserve(SdUInt32 pointCount);
		bool EnsureBatch(const SdRect& clipRect, SdTextureHandle texture, SdUInt32 vertexReserve);
		bool CanAddVertices(SdUInt32 vertexCount) const;
		void TrackReserveGrowth(SdUInt32 vertexReserve, SdUInt32 indexReserve, SdUInt32 batchReserve = 0);
		void TrackCurrentBatchVertices(SdUInt32 vertexCount);
		SdVertex* AppendVertices(SdUInt32 vertexCount);
		SdUInt32* AppendIndices(SdUInt32 indexCount);
		bool UseAntiAliasing() const noexcept;
		void EnsureScratch(std::vector<SdVec2>& scratch, SdSize count);
		void PathClear() { path.clear(); }
		void PathLineTo(const SdVec2& point) { path.push_back(point); }
		void PathArcTo(const SdVec2& center, float radius, float angleMin, float angleMax, SdUInt32 segmentCount);
		void PathArcToSkipFirst(const SdVec2& center, float radius, float angleMin, float angleMax, SdUInt32 segmentCount);
		void PathRect(const SdRect& rect, float rounding, SdUInt32 segmentCount);
		void PathFillConvex(const SdColor& color);
		void PathStroke(const SdColor& color, float thickness, bool closed);
		void PathFillConvexNoAA(const SdColor& color, SdTextureHandle texture, const SdVec2& opaqueUv);
		void PathFillConvexAA(const SdColor& color, SdTextureHandle texture, const SdVec2& opaqueUv);
		void PathStrokeNoAA(const SdColor& color, SdTextureHandle texture, const SdVec2& opaqueUv, float thickness, bool closed);
		void PathStrokeAAThinTextured(const SdColor& color, SdTextureHandle texture, float thickness, bool closed, SdUInt32 integerThickness);
		void PathStrokeAAThinGeometry(const SdColor& color, SdTextureHandle texture, const SdVec2& opaqueUv, float thickness, bool closed);
		void PathStrokeAAThick(const SdColor& color, SdTextureHandle texture, const SdVec2& opaqueUv, float thickness, bool closed);
		void BuildPolylineNormals(bool closed);
		void PrimRectWithUV(const SdRect& rect, const SdRect& uvRect, const SdColor& color, SdTextureHandle texture);
		static SdUInt32 ResolveCircleSegmentCount(float radius, SdUInt32 requestedSegmentCount);

	public:
		explicit SdRenderList(SdRenderStats* stats = nullptr, SdRenderSharedData* sharedData = nullptr);

		void Reset();
		void SetStats(SdRenderStats* value) noexcept { stats = value; }
		void SetSharedData(SdRenderSharedData* value) noexcept { sharedData = value; }
		void SetWhiteTexture(SdTextureHandle texture);
		void Reserve(SdUInt32 vertexCount, SdUInt32 indexCount, SdUInt32 batchCount = 0);
		void RequireSpace(SdUInt32 vertexCount, SdUInt32 indexCount);

		void PushClipRect(const SdRect& rect);
		void PopClip();

		void AddLine(const SdVec2& a, const SdVec2& b, const SdColor& color, float thickness = 1.0f);
		void AddTriangle(const SdVec2& a, const SdVec2& b, const SdVec2& c, const SdColor& color, float thickness = 1.0f);
		void AddTriangleFilled(const SdVec2& a, const SdVec2& b, const SdVec2& c, const SdColor& color);
		void AddQuad(const SdVec2& a, const SdVec2& b, const SdVec2& c, const SdVec2& d, const SdColor& color, float thickness = 1.0f);
		void AddQuadFilled(const SdVec2& a, const SdVec2& b, const SdVec2& c, const SdVec2& d, const SdColor& color);
		void AddRect(const SdRect& rect, const SdColor& color, float thickness = 1.0f, float rounding = 0.0f, SdUInt32 roundingSegments = 0);
		void AddRectFilled(const SdRect& rect, const SdColor& color, float rounding = 0.0f, SdUInt32 roundingSegments = 0);
		void AddRectFilledMultiColor(const SdRect& rect, const SdColor& colorUpperLeft, const SdColor& colorUpperRight, const SdColor& colorBottomRight, const SdColor& colorBottomLeft);
		void AddBackdropBlur(const SdRect& rect, float radius, float cornerRadius = 0.0f);
		void AddBackdropBlur(const SdRect& rect, float radius, const SdCornerRadii& cornerRadii);
		void AddDropShadow(const SdRect& rect, const SdVec2& offset, const SdColor& color, float radius, float spread = 0.0f, float cornerRadius = 0.0f);
		void AddDropShadow(const SdRect& rect, const SdVec2& offset, const SdColor& color, float radius, float spread, const SdCornerRadii& cornerRadii);
		void AddInnerShadow(const SdRect& rect, const SdVec2& offset, const SdColor& color, float radius, float spread = 0.0f, float cornerRadius = 0.0f);
		void AddInnerShadow(const SdRect& rect, const SdVec2& offset, const SdColor& color, float radius, float spread, const SdCornerRadii& cornerRadii);
		void AddCircle(const SdVec2& center, float radius, const SdColor& color, float thickness = 1.0f, SdUInt32 segmentCount = 0);
		void AddCircleFilled(const SdVec2& center, float radius, const SdColor& color, SdUInt32 segmentCount = 0);
		void AddImage(SdTextureHandle texture, const SdRect& rect, const SdRect& uvRect, const SdColor& color);

		void AddText(SdUtf8StringView text, const SdVec2& position, const SdColor& color)
		{
			if (!sharedData)
				return;
			AddText(text, sharedData->defaultTextStyle, position, color);
		}

		void AddText(SdUtf8StringView text, const SdTextStyle& style, const SdVec2& position, const SdColor& color)
		{
			if (!sharedData || !sharedData->fontBackend || text.empty())
				return;

			SdParagraphLayout layout = sharedData->fontBackend->BuildParagraphLayout(text, style, 0.0f, color);
			sharedData->fontBackend->DrainPendingUploads(drawData.uploadRequests);
			sharedData->fontBackend->ConfigureRenderSharedData(*sharedData);
			for (SdTextGlyph& glyph : layout.glyphs)
			{
				glyph.rect.min.x += position.x;
				glyph.rect.max.x += position.x;
				glyph.rect.min.y += position.y;
				glyph.rect.max.y += position.y;
			}
			AddTextLayout(layout, color);
		}

		void AddParagraph(SdUtf8StringView text, const SdTextStyle& style, const SdVec2& position, float maxWidth, const SdColor& color)
		{
			if (!sharedData || !sharedData->fontBackend || text.empty())
				return;

			SdParagraphLayout layout = sharedData->fontBackend->BuildParagraphLayout(text, style, maxWidth, color);
			sharedData->fontBackend->DrainPendingUploads(drawData.uploadRequests);
			sharedData->fontBackend->ConfigureRenderSharedData(*sharedData);
			for (SdTextGlyph& glyph : layout.glyphs)
			{
				glyph.rect.min.x += position.x;
				glyph.rect.max.x += position.x;
				glyph.rect.min.y += position.y;
				glyph.rect.max.y += position.y;
			}
			AddTextLayout(layout, color);
		}

		void AddTextLayout(const SdParagraphLayout& layout, const SdColor& color);

		SdRenderData& GetDrawData() noexcept { return drawData; }
		const SdRenderData& GetDrawData() const noexcept { return drawData; }
		SdRenderData& GetData() noexcept { return drawData; }
		const SdRenderData& GetData() const noexcept { return drawData; }
		SdRenderPacket BuildPacket(SdUInt32 packetVersion = 0) const noexcept { return drawData.BuildPacket(packetVersion); }
	};

	using SdDrawList = SdRenderList;
}
