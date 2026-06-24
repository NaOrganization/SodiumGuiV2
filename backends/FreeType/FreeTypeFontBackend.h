#pragma once

#include "Render/SdRenderCore.h"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_COLOR_H

#include <unordered_map>

namespace Sodium::Backends
{
	struct SdFreeTypeFontBackendConfig final
	{
		SdUInt32 atlasWidth = 1024;
		SdUInt32 atlasHeight = 1024;
	};

	class SdFreeTypeFontBackend final : public ISdFontBackend
	{
	private:
		struct FontFace final
		{
			SdFontHandle handle = {};
			SdFontFamilyHandle family = {};
			SdUtf8String debugName = {};
			std::vector<std::byte> bytes = {};
			FT_Face face = nullptr;
		};

		struct FontFamily final
		{
			SdFontFamilyHandle handle = {};
			SdUtf8String name = {};
			std::vector<SdFontHandle> faces = {};
		};

		struct GlyphKey final
		{
			SdFontHandle font = {};
			SdUInt32 codepoint = 0;
			SdUInt32 pixelSize = 0;

			friend bool operator==(const GlyphKey&, const GlyphKey&) = default;
		};

		struct GlyphKeyHash final
		{
			SdSize operator()(const GlyphKey& key) const noexcept;
		};

		struct GlyphInfo final
		{
			SdTextureHandle texture = {};
			SdRect uvRect = {};
			SdVec2 bearing = {};
			SdVec2 size = {};
			SdUInt32 atlasX = 0;
			SdUInt32 atlasY = 0;
			SdUInt32 atlasWidth = 0;
			SdUInt32 atlasHeight = 0;
			float advanceX = 0.0f;
			bool visible = false;
			bool colored = false;
		};

		struct SkylineNode final
		{
			SdUInt32 x = 0;
			SdUInt32 y = 0;
			SdUInt32 width = 0;
		};

		struct Atlas final
		{
			SdTextureHandle texture = SdTextureHandle(64, 1);
			SdUInt32 width = 0;
			SdUInt32 height = 0;
			SdRect whitePixelUv = {};
			std::array<SdVec4, SdRenderSharedData::BakedLineTextureCount> bakedLineUvs = {};
			std::vector<SkylineNode> skyline = {};
			bool dirty = false;
			std::vector<SdUInt32> pixels = {};
		};

		struct ParagraphCacheEntry final
		{
			SdUtf8String text = {};
			SdTextStyle style = {};
			float maxWidth = 0.0f;
			SdColor color = SdColorWhite;
			SdParagraphLayout layout{ std::pmr::get_default_resource() };
			SdUInt64 lastUsed = 0;
		};

		static constexpr SdSize kMaxParagraphCacheEntries = 128;

		FT_Library library = nullptr;
		SdFreeTypeFontBackendConfig config = {};
		SdUInt32 nextFontIndex = 1;
		SdUInt32 nextFamilyIndex = 1;
		SdUInt64 paragraphCacheClock = 0;
		SdFontHandle fallbackFont = {};
		std::vector<FontFace> faces = {};
		std::vector<FontFamily> families = {};
		std::vector<SdFontHandle> fallbackFonts = {};
		std::unordered_map<GlyphKey, GlyphInfo, GlyphKeyHash> glyphCache = {};
		std::vector<ParagraphCacheEntry> paragraphCache = {};
		Atlas atlas = {};

		FontFace* TryGetFace(SdFontHandle handle) noexcept;
		const FontFace* TryGetFace(SdFontHandle handle) const noexcept;
		FontFamily* TryGetFamily(SdFontFamilyHandle handle) noexcept;
		const FontFamily* TryGetFamily(SdFontFamilyHandle handle) const noexcept;
		SdFontHandle ResolveFont(const SdTextStyle& style) const noexcept;
		SdFontHandle ResolveFontForCodepoint(const SdTextStyle& style, SdUInt32 codepoint) const noexcept;
		SdFontHandle ResolveFontForCodepoint(SdFontHandle preferredFont, SdUInt32 codepoint) const noexcept;
		bool HasGlyph(SdFontHandle font, SdUInt32 codepoint) const noexcept;
		void AddFallbackFont(SdFontHandle font);
		void InitializeAtlas(SdUInt32 width, SdUInt32 height);
		void BakeAtlasDrawPrimitives();
		void RebuildAtlasDrawPrimitiveUvs();
		bool GrowAtlas(SdUInt32 minWidth, SdUInt32 minHeight);
		bool CalculateSkylinePlacement(SdSize nodeIndex, SdUInt32 width, SdUInt32 height, SdUInt32& outY, SdUInt32& outWaste) const;
		void InsertSkylineNode(SdSize nodeIndex, SdUInt32 x, SdUInt32 y, SdUInt32 width, SdUInt32 height);
		bool AllocateAtlasRegion(SdUInt32 width, SdUInt32 height, SdUInt32& outX, SdUInt32& outY);
		void RebuildGlyphUvs();
		const GlyphInfo* EnsureGlyph(SdFontHandle font, SdUInt32 codepoint, float pixelSize);
		float ResolveLineHeight(SdFontHandle font, const SdTextStyle& style);
		float ResolveAscender(SdFontHandle font, float pixelSize);
		SdParagraphLayout BuildParagraphLayoutUncached(
			SdUtf8StringView text,
			const SdTextStyle& style,
			float maxWidth,
			const SdColor& color,
			std::pmr::memory_resource* resource);
		ParagraphCacheEntry* FindParagraphCacheEntry(SdUtf8StringView text, const SdTextStyle& style, float maxWidth, const SdColor& color);
		ParagraphCacheEntry& StoreParagraphCacheEntry(SdUtf8StringView text, const SdTextStyle& style, float maxWidth, const SdColor& color, SdParagraphLayout&& layout);
		static bool MatchesStyle(const SdTextStyle& left, const SdTextStyle& right);
		static SdParagraphLayout CopyParagraphLayout(const SdParagraphLayout& layout, std::pmr::memory_resource* resource);

	public:
		using Config = SdFreeTypeFontBackendConfig;

		bool Initialize(const Config& backendConfig = {});
		void Shutdown();
		bool IsInitialized() const noexcept override { return library && fallbackFont.IsValid(); }
		SdTextureHandle GetAtlasTexture() const noexcept { return atlas.texture; }
		void ConfigureRenderSharedData(SdRenderSharedData& sharedData) const override;

		SdFontFamilyHandle RegisterFontFromPath(SdUtf8StringView familyName, const SdPath& path);
		SdFontFamilyHandle RegisterFontFromMemory(SdUtf8StringView familyName, std::span<const std::byte> bytes, SdUtf8StringView debugName = {});
		std::vector<SdFontHandle> GetFamilyFaces(SdFontFamilyHandle family) const;

		SdFontHandle LoadFont(const SdFontRequest& request);
		SdTextMetrics MeasureText(SdFontHandle font, std::u32string_view text, const SdTextMeasureOptions& options);
		SdGlyphRun ShapeText(SdFontHandle font, std::u32string_view text, const SdTextShapeOptions& options);
		SdGlyphBitmap RasterizeGlyph(SdFontHandle font, SdGlyphId glyphId, float size);

		SdFontHandle GetFallbackFont() const noexcept override { return fallbackFont; }
		SdVec2 MeasureText(SdUtf8StringView text, const SdTextStyle& style) override;
		SdParagraphLayout BuildParagraphLayout(
			SdUtf8StringView text,
			const SdTextStyle& style,
			float maxWidth,
			const SdColor& color,
			std::pmr::memory_resource* resource = std::pmr::get_default_resource()) override;
		void DrainPendingUploads(std::vector<SdUploadRequest>& uploads) override;
	};

	using SdFontBackend = SdFreeTypeFontBackend;
}
