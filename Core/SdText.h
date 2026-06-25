#pragma once

#include <Core/SdCore.h>
#include <Rhi/SdRhi.h>

#include <memory_resource>
#include <vector>

namespace Sodium
{
	struct SdRenderSharedData;
	struct SdUploadRequest;

	enum class SdTextAlign : SdUInt8
	{
		Left,
		Center,
		Right
	};

	struct SdFontStack final
	{
		std::vector<SdFontHandle> fonts = {};
	};

	struct SdTextStyle final
	{
		SdFontStack fontStack = {};
		float pixelSize = 16.0f;
		float lineHeight = 0.0f;
		SdTextAlign align = SdTextAlign::Left;
		bool wrap = false;
	};

	using SdGlyphId = SdUInt32;

	struct SdFontRequest final
	{
		SdPath path = {};
		SdUtf8StringView familyName = {};
	};

	struct SdTextMeasureOptions final
	{
		float pixelSize = 16.0f;
	};

	struct SdTextShapeOptions final
	{
		float pixelSize = 16.0f;
	};

	struct SdTextMetrics final
	{
		float width = 0.0f;
		float height = 0.0f;
		float ascender = 0.0f;
		float descender = 0.0f;
	};

	struct SdGlyphRun final
	{
		std::vector<SdGlyphId> glyphIds = {};
		std::vector<SdVec2> positions = {};
		SdVec2 advance = {};
	};

	struct SdGlyphBitmap final
	{
		SdUInt32 width = 0;
		SdUInt32 height = 0;
		SdInt32 pitch = 0;
		std::vector<std::byte> buffer = {};
	};

	struct SdTextGlyph final
	{
		SdFontHandle font = {};
		SdTextureHandle texture = {};
		SdRect rect = {};
		SdRect uvRect = {};
		SdColor color = SdColorWhite;
		bool colored = false;
		SdUInt32 codepoint = 0;
	};

	struct SdTextLine final
	{
		SdUInt32 glyphOffset = 0;
		SdUInt32 glyphCount = 0;
		float width = 0.0f;
		float y = 0.0f;
	};

	struct SdParagraphLayout final
	{
		std::pmr::vector<SdTextGlyph> glyphs = {};
		std::pmr::vector<SdTextLine> lines = {};
		SdVec2 size = {};

		explicit SdParagraphLayout(std::pmr::memory_resource* resource = std::pmr::get_default_resource())
			: glyphs(resource), lines(resource) {}
	};

	class ISdFontBackend
	{
	public:
		virtual ~ISdFontBackend() = default;

		virtual bool IsInitialized() const noexcept = 0;

		// Returns the font used when a text style does not resolve a face.
		virtual SdFontHandle GetFallbackFont() const noexcept = 0;

		// Fast measurement path used during layout. Implementations may cache
		// glyph metrics but must not require renderer access.
		virtual SdVec2 MeasureText(SdUtf8StringView text, const SdTextStyle& style) = 0;

		// Builds positioned glyphs for paint. Any atlas resource changes must
		// be reported later through DrainPendingUploads.
		virtual SdParagraphLayout BuildParagraphLayout(
			SdUtf8StringView text,
			const SdTextStyle& style,
			float maxWidth,
			const SdColor& color,
			std::pmr::memory_resource* resource = std::pmr::get_default_resource()) = 0;

		// Publishes shared atlas handles/UVs used by SdRenderList.
		virtual void ConfigureRenderSharedData(SdRenderSharedData& sharedData) const = 0;

		// Moves pending atlas uploads into the frame draw packet. The renderer
		// consumes these updates during ISdRendererBackend::Render.
		virtual void DrainPendingUploads(std::vector<SdUploadRequest>& uploads) = 0;
	};
}
