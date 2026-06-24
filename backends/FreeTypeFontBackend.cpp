#include "FreeTypeFontBackend.h"

#include "../SdDefaultFont.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <limits>

namespace Sodium::Backends
{
	namespace
	{
		constexpr SdUInt32 kAtlasMinSize = 128;
		constexpr SdUInt32 kAtlasWhitePixelX = 1;
		constexpr SdUInt32 kAtlasWhitePixelY = 1;
		constexpr SdUInt32 kAtlasLineStartX = 2;
		constexpr SdUInt32 kAtlasLineStartY = 2;
		constexpr SdUInt32 kAtlasLineRegionWidth = SdRenderSharedData::BakedLineTextureCount + 2;
		constexpr SdUInt32 kAtlasReservedHeight = kAtlasLineStartY + SdRenderSharedData::BakedLineTextureCount + 2;

		SdUInt32 AlphaBlendColor(SdUInt32 destination, const FT_Color& sourceColor, SdUInt8 coverage)
		{
			const SdUInt32 sourceAlpha = (static_cast<SdUInt32>(sourceColor.alpha) * coverage + 127u) / 255u;
			if (sourceAlpha == 0)
				return destination;
			if (sourceAlpha == 255)
				return SdColor(sourceColor.red, sourceColor.green, sourceColor.blue, 255).Pack();

			const SdUInt32 destinationRed = destination & 0xFFu;
			const SdUInt32 destinationGreen = (destination >> 8) & 0xFFu;
			const SdUInt32 destinationBlue = (destination >> 16) & 0xFFu;
			const SdUInt32 destinationAlpha = (destination >> 24) & 0xFFu;
			const SdUInt32 inverseAlpha = 255u - sourceAlpha;
			const SdUInt32 outputAlpha = sourceAlpha + ((destinationAlpha * inverseAlpha + 127u) / 255u);
			if (outputAlpha == 0)
				return 0;

			const SdUInt32 sourceRedPremultiplied = static_cast<SdUInt32>(sourceColor.red) * sourceAlpha;
			const SdUInt32 sourceGreenPremultiplied = static_cast<SdUInt32>(sourceColor.green) * sourceAlpha;
			const SdUInt32 sourceBluePremultiplied = static_cast<SdUInt32>(sourceColor.blue) * sourceAlpha;
			const SdUInt32 destinationRedPremultiplied = destinationRed * destinationAlpha;
			const SdUInt32 destinationGreenPremultiplied = destinationGreen * destinationAlpha;
			const SdUInt32 destinationBluePremultiplied = destinationBlue * destinationAlpha;
			const SdUInt32 outputRedPremultiplied = sourceRedPremultiplied + ((destinationRedPremultiplied * inverseAlpha + 127u) / 255u);
			const SdUInt32 outputGreenPremultiplied = sourceGreenPremultiplied + ((destinationGreenPremultiplied * inverseAlpha + 127u) / 255u);
			const SdUInt32 outputBluePremultiplied = sourceBluePremultiplied + ((destinationBluePremultiplied * inverseAlpha + 127u) / 255u);
			const SdUInt8 outputRed = static_cast<SdUInt8>((outputRedPremultiplied + outputAlpha / 2u) / outputAlpha);
			const SdUInt8 outputGreen = static_cast<SdUInt8>((outputGreenPremultiplied + outputAlpha / 2u) / outputAlpha);
			const SdUInt8 outputBlue = static_cast<SdUInt8>((outputBluePremultiplied + outputAlpha / 2u) / outputAlpha);
			return SdColor(outputRed, outputGreen, outputBlue, static_cast<SdUInt8>(outputAlpha)).Pack();
		}
	}

	SdSize SdFreeTypeFontBackend::GlyphKeyHash::operator()(const GlyphKey& key) const noexcept
	{
		SdUInt64 value = key.font.index;
		value = (value * 16777619ull) ^ key.font.generation;
		value = (value * 16777619ull) ^ key.codepoint;
		value = (value * 16777619ull) ^ key.pixelSize;
		return static_cast<SdSize>(value);
	}

	bool SdFreeTypeFontBackend::Initialize(const Config& backendConfig)
	{
		config = backendConfig;
		if (FT_Init_FreeType(&library) != 0)
			return false;

		InitializeAtlas(config.atlasWidth, config.atlasHeight);
		SdUtf8String defaultFont = DefaultFont::GetProggyCleanTtf();
		if (!defaultFont.empty())
		{
			std::span<const std::byte> bytes(
				reinterpret_cast<const std::byte*>(defaultFont.data()),
				defaultFont.size());
			const SdFontFamilyHandle family = RegisterFontFromMemory(SODIUM_STRING("ProggyClean"), bytes, SODIUM_STRING("ProggyClean.ttf"));
			const std::vector<SdFontHandle> facesInFamily = GetFamilyFaces(family);
			if (!facesInFamily.empty())
				fallbackFont = facesInFamily.front();
		}
		return fallbackFont.IsValid();
	}

	void SdFreeTypeFontBackend::Shutdown()
	{
		for (FontFace& face : faces)
		{
			if (face.face)
				FT_Done_Face(face.face);
			face.face = nullptr;
		}
		faces.clear();
		families.clear();
		fallbackFonts.clear();
		glyphCache.clear();
		paragraphCache.clear();
		paragraphCacheClock = 0;
		atlas = {};
		fallbackFont = {};
		if (library)
		{
			FT_Done_FreeType(library);
			library = nullptr;
		}
	}

	SdFreeTypeFontBackend::FontFace* SdFreeTypeFontBackend::TryGetFace(SdFontHandle handle) noexcept
	{
		for (FontFace& face : faces)
		{
			if (face.handle == handle)
				return &face;
		}
		return nullptr;
	}

	const SdFreeTypeFontBackend::FontFace* SdFreeTypeFontBackend::TryGetFace(SdFontHandle handle) const noexcept
	{
		for (const FontFace& face : faces)
		{
			if (face.handle == handle)
				return &face;
		}
		return nullptr;
	}

	SdFreeTypeFontBackend::FontFamily* SdFreeTypeFontBackend::TryGetFamily(SdFontFamilyHandle handle) noexcept
	{
		for (FontFamily& family : families)
		{
			if (family.handle == handle)
				return &family;
		}
		return nullptr;
	}

	const SdFreeTypeFontBackend::FontFamily* SdFreeTypeFontBackend::TryGetFamily(SdFontFamilyHandle handle) const noexcept
	{
		for (const FontFamily& family : families)
		{
			if (family.handle == handle)
				return &family;
		}
		return nullptr;
	}

	SdFontFamilyHandle SdFreeTypeFontBackend::RegisterFontFromPath(SdUtf8StringView familyName, const SdPath& path)
	{
		FILE* file = nullptr;
		if (_wfopen_s(&file, path.c_str(), SODIUM_STRING(L"rb")) != 0 || !file)
			return {};

		std::fseek(file, 0, SEEK_END);
		const long fileSize = std::ftell(file);
		std::rewind(file);
		if (fileSize <= 0)
		{
			std::fclose(file);
			return {};
		}

		std::vector<std::byte> bytes(static_cast<SdSize>(fileSize));
		const size_t readBytes = std::fread(bytes.data(), 1, bytes.size(), file);
		std::fclose(file);
		if (readBytes != bytes.size())
			return {};

		return RegisterFontFromMemory(familyName, bytes, path.filename().string());
	}

	SdFontFamilyHandle SdFreeTypeFontBackend::RegisterFontFromMemory(SdUtf8StringView familyName, std::span<const std::byte> bytes, SdUtf8StringView debugName)
	{
		if (!library || bytes.empty())
			return {};

		FontFace face = {};
		face.handle = SdFontHandle(nextFontIndex++);
		face.bytes.assign(bytes.begin(), bytes.end());
		face.debugName = debugName.empty() ? SdUtf8String(familyName) : SdUtf8String(debugName);
		if (FT_New_Memory_Face(library, reinterpret_cast<const FT_Byte*>(face.bytes.data()), static_cast<FT_Long>(face.bytes.size()), 0, &face.face) != 0)
			return {};

		SdFontFamilyHandle familyHandle = {};
		for (const FontFamily& existing : families)
		{
			if (existing.name == familyName)
			{
				familyHandle = existing.handle;
				break;
			}
		}
		if (!familyHandle.IsValid())
		{
			FontFamily family = {};
			family.handle = SdFontFamilyHandle(nextFamilyIndex++);
			family.name = familyName;
			familyHandle = family.handle;
			families.push_back(std::move(family));
		}

		face.family = familyHandle;
		faces.push_back(std::move(face));
		const SdFontHandle font = faces.back().handle;
		if (FontFamily* family = TryGetFamily(familyHandle))
			family->faces.push_back(font);
		AddFallbackFont(font);
		return familyHandle;
	}

	std::vector<SdFontHandle> SdFreeTypeFontBackend::GetFamilyFaces(SdFontFamilyHandle family) const
	{
		if (const FontFamily* entry = TryGetFamily(family))
			return entry->faces;
		return {};
	}

	SdFontHandle SdFreeTypeFontBackend::LoadFont(const SdFontRequest& request)
	{
		const SdFontFamilyHandle family = RegisterFontFromPath(request.familyName, request.path);
		const std::vector<SdFontHandle> facesInFamily = GetFamilyFaces(family);
		return facesInFamily.empty() ? SdFontHandle{} : facesInFamily.front();
	}

	SdGlyphRun SdFreeTypeFontBackend::ShapeText(SdFontHandle font, std::u32string_view text, const SdTextShapeOptions& options)
	{
		SdGlyphRun run = {};
		if (text.empty())
			return run;

		float penX = 0.0f;
		for (char32_t codepoint : text)
		{
			const SdFontHandle resolvedFont = ResolveFontForCodepoint(font, static_cast<SdUInt32>(codepoint));
			FontFace* face = TryGetFace(resolvedFont);
			if (!face || !face->face)
				continue;

			FT_Set_Pixel_Sizes(face->face, 0, static_cast<FT_UInt>(std::ceil(options.pixelSize)));
			FT_UInt glyphIndex = FT_Get_Char_Index(face->face, static_cast<FT_ULong>(codepoint));
			if (glyphIndex == 0)
				glyphIndex = FT_Get_Char_Index(face->face, static_cast<FT_ULong>('?'));
			if (glyphIndex == 0 || FT_Load_Glyph(face->face, glyphIndex, FT_LOAD_DEFAULT) != 0)
				continue;
			run.glyphIds.push_back(static_cast<SdGlyphId>(glyphIndex));
			run.positions.push_back({ penX, 0.0f });
			penX += static_cast<float>(face->face->glyph->advance.x) / 64.0f;
		}
		run.advance = { penX, 0.0f };
		return run;
	}

	SdTextMetrics SdFreeTypeFontBackend::MeasureText(SdFontHandle font, std::u32string_view text, const SdTextMeasureOptions& options)
	{
		SdTextMetrics metrics = {};
		FontFace* face = TryGetFace(font.IsValid() ? font : fallbackFont);
		if (!face || !face->face)
			return metrics;

		const SdGlyphRun run = ShapeText(face->handle, text, SdTextShapeOptions{ options.pixelSize });
		FT_Set_Pixel_Sizes(face->face, 0, static_cast<FT_UInt>(std::ceil(options.pixelSize)));
		metrics.width = run.advance.x;
		metrics.height = static_cast<float>(face->face->size->metrics.height) / 64.0f;
		metrics.ascender = static_cast<float>(face->face->size->metrics.ascender) / 64.0f;
		metrics.descender = static_cast<float>(face->face->size->metrics.descender) / 64.0f;
		return metrics;
	}

	SdGlyphBitmap SdFreeTypeFontBackend::RasterizeGlyph(SdFontHandle font, SdGlyphId glyphId, float size)
	{
		SdGlyphBitmap bitmap = {};
		FontFace* face = TryGetFace(font.IsValid() ? font : fallbackFont);
		if (!face || !face->face || glyphId == 0 || size <= 0.0f)
			return bitmap;

		FT_Set_Pixel_Sizes(face->face, 0, static_cast<FT_UInt>(std::ceil(size)));
		if (FT_Load_Glyph(face->face, glyphId, FT_LOAD_DEFAULT) != 0)
			return bitmap;
		if (FT_Render_Glyph(face->face->glyph, FT_RENDER_MODE_NORMAL) != 0)
			return bitmap;

		const FT_Bitmap& source = face->face->glyph->bitmap;
		bitmap.width = source.width;
		bitmap.height = source.rows;
		bitmap.pitch = source.pitch;
		if (source.buffer && source.rows > 0)
		{
			const SdSize byteCount = static_cast<SdSize>(std::abs(source.pitch)) * source.rows;
			bitmap.buffer.assign(
				reinterpret_cast<const std::byte*>(source.buffer),
				reinterpret_cast<const std::byte*>(source.buffer) + byteCount);
		}
		return bitmap;
	}

	SdFontHandle SdFreeTypeFontBackend::ResolveFont(const SdTextStyle& style) const noexcept
	{
		for (SdFontHandle font : style.fontStack.fonts)
		{
			if (font.IsValid() && TryGetFace(font))
				return font;
		}
		return fallbackFont;
	}

	SdFontHandle SdFreeTypeFontBackend::ResolveFontForCodepoint(const SdTextStyle& style, SdUInt32 codepoint) const noexcept
	{
		for (SdFontHandle font : style.fontStack.fonts)
		{
			if (HasGlyph(font, codepoint))
				return font;
		}
		for (SdFontHandle font : fallbackFonts)
		{
			if (HasGlyph(font, codepoint))
				return font;
		}
		return ResolveFont(style);
	}

	SdFontHandle SdFreeTypeFontBackend::ResolveFontForCodepoint(SdFontHandle preferredFont, SdUInt32 codepoint) const noexcept
	{
		if (HasGlyph(preferredFont, codepoint))
			return preferredFont;
		for (SdFontHandle font : fallbackFonts)
		{
			if (HasGlyph(font, codepoint))
				return font;
		}
		return preferredFont.IsValid() ? preferredFont : fallbackFont;
	}

	bool SdFreeTypeFontBackend::HasGlyph(SdFontHandle font, SdUInt32 codepoint) const noexcept
	{
		const FontFace* face = TryGetFace(font);
		return face && face->face && FT_Get_Char_Index(face->face, static_cast<FT_ULong>(codepoint)) != 0;
	}

	void SdFreeTypeFontBackend::AddFallbackFont(SdFontHandle font)
	{
		if (!font.IsValid())
			return;
		if (std::find(fallbackFonts.begin(), fallbackFonts.end(), font) != fallbackFonts.end())
			return;
		fallbackFonts.push_back(font);
	}

	void SdFreeTypeFontBackend::InitializeAtlas(SdUInt32 width, SdUInt32 height)
	{
		atlas = {};
		atlas.texture = SdTextureHandle(64, 1);
		atlas.width = std::max(width, kAtlasMinSize);
		atlas.height = std::max(std::max(height, kAtlasMinSize), kAtlasReservedHeight + 2u);
		atlas.skyline.push_back({ 1, kAtlasReservedHeight, atlas.width - 2 });
		atlas.dirty = true;
		atlas.pixels.assign(static_cast<SdSize>(atlas.width) * atlas.height, 0u);
		BakeAtlasDrawPrimitives();
	}

	void SdFreeTypeFontBackend::BakeAtlasDrawPrimitives()
	{
		if (atlas.pixels.empty())
			return;

		atlas.pixels[static_cast<SdSize>(kAtlasWhitePixelY) * atlas.width + kAtlasWhitePixelX] = SdColorWhite.Pack();
		for (SdUInt32 lineWidth = 0; lineWidth < SdRenderSharedData::BakedLineTextureCount; ++lineWidth)
		{
			const SdUInt32 padLeft = (kAtlasLineRegionWidth - lineWidth) / 2u;
			const SdUInt32 y = kAtlasLineStartY + lineWidth;
			for (SdUInt32 x = 0; x < kAtlasLineRegionWidth; ++x)
			{
				const bool insideLine = x >= padLeft && x < padLeft + lineWidth;
				const SdUInt8 alpha = insideLine ? 255 : 0;
				atlas.pixels[static_cast<SdSize>(y) * atlas.width + kAtlasLineStartX + x] = SdColor(255, 255, 255, alpha).Pack();
			}
		}

		RebuildAtlasDrawPrimitiveUvs();
		atlas.dirty = true;
	}

	void SdFreeTypeFontBackend::RebuildAtlasDrawPrimitiveUvs()
	{
		const float invWidth = atlas.width > 0 ? 1.0f / static_cast<float>(atlas.width) : 0.0f;
		const float invHeight = atlas.height > 0 ? 1.0f / static_cast<float>(atlas.height) : 0.0f;
		const SdVec2 whitePixelUv = {
			(static_cast<float>(kAtlasWhitePixelX) + 0.5f) * invWidth,
			(static_cast<float>(kAtlasWhitePixelY) + 0.5f) * invHeight
		};
		atlas.whitePixelUv = { whitePixelUv, whitePixelUv };

		for (SdUInt32 lineWidth = 0; lineWidth < SdRenderSharedData::BakedLineTextureCount; ++lineWidth)
		{
			const SdUInt32 padLeft = (kAtlasLineRegionWidth - lineWidth) / 2u;
			const SdUInt32 uvMinX = kAtlasLineStartX + padLeft - 1u;
			const SdUInt32 uvMaxX = kAtlasLineStartX + padLeft + lineWidth;
			const float y = (static_cast<float>(kAtlasLineStartY + lineWidth) + 0.5f) * invHeight;
			atlas.bakedLineUvs[lineWidth] = {
				(static_cast<float>(uvMinX) + 0.5f) * invWidth,
				y,
				(static_cast<float>(uvMaxX) + 0.5f) * invWidth,
				y
			};
		}
	}

	bool SdFreeTypeFontBackend::GrowAtlas(SdUInt32 minWidth, SdUInt32 minHeight)
	{
		const SdUInt32 oldWidth = atlas.width;
		const SdUInt32 oldHeight = atlas.height;
		SdUInt32 newWidth = atlas.width;
		SdUInt32 newHeight = atlas.height;
		while (newWidth < minWidth)
			newWidth *= 2u;
		while (newHeight < minHeight)
			newHeight *= 2u;
		if (newWidth == oldWidth && newHeight == oldHeight)
			return false;

		std::vector<SdUInt32> newPixels(static_cast<SdSize>(newWidth) * newHeight, 0u);
		for (SdUInt32 y = 0; y < oldHeight; ++y)
		{
			std::memcpy(
				newPixels.data() + static_cast<SdSize>(y) * newWidth,
				atlas.pixels.data() + static_cast<SdSize>(y) * oldWidth,
				static_cast<SdSize>(oldWidth) * sizeof(SdUInt32));
		}
		atlas.width = newWidth;
		atlas.height = newHeight;
		atlas.pixels = std::move(newPixels);
		if (newWidth > oldWidth)
			InsertSkylineNode(atlas.skyline.size(), oldWidth - 1u, kAtlasReservedHeight, newWidth - oldWidth, 0);
		RebuildAtlasDrawPrimitiveUvs();
		RebuildGlyphUvs();
		atlas.dirty = true;
		return true;
	}

	bool SdFreeTypeFontBackend::CalculateSkylinePlacement(SdSize nodeIndex, SdUInt32 width, SdUInt32 height, SdUInt32& outY, SdUInt32& outWaste) const
	{
		if (nodeIndex >= atlas.skyline.size())
			return false;

		const SkylineNode& firstNode = atlas.skyline[nodeIndex];
		if (firstNode.x + width > atlas.width - 1u)
			return false;

		SdUInt32 y = firstNode.y;
		SdUInt32 remainingWidth = width;
		for (SdSize index = nodeIndex; remainingWidth > 0; ++index)
		{
			if (index >= atlas.skyline.size())
				return false;

			const SkylineNode& node = atlas.skyline[index];
			y = std::max(y, node.y);
			if (y + height > atlas.height - 1u)
				return false;

			const SdUInt32 usedWidth = std::min(remainingWidth, node.width);
			remainingWidth -= usedWidth;
		}

		SdUInt32 waste = 0;
		remainingWidth = width;
		for (SdSize index = nodeIndex; remainingWidth > 0; ++index)
		{
			const SkylineNode& node = atlas.skyline[index];
			const SdUInt32 usedWidth = std::min(remainingWidth, node.width);
			waste += (y - node.y) * usedWidth;
			remainingWidth -= usedWidth;
		}

		outY = y;
		outWaste = waste;
		return true;
	}

	void SdFreeTypeFontBackend::InsertSkylineNode(SdSize nodeIndex, SdUInt32 x, SdUInt32 y, SdUInt32 width, SdUInt32 height)
	{
		if (width == 0)
			return;

		nodeIndex = std::min(nodeIndex, atlas.skyline.size());
		const SdUInt32 nodeRight = x + width;
		atlas.skyline.insert(atlas.skyline.begin() + static_cast<std::ptrdiff_t>(nodeIndex), SkylineNode{ x, y + height, width });
		for (SdSize index = nodeIndex + 1; index < atlas.skyline.size();)
		{
			SkylineNode& node = atlas.skyline[index];
			if (node.x >= nodeRight)
				break;

			const SdUInt32 overlap = nodeRight - node.x;
			if (overlap >= node.width)
			{
				atlas.skyline.erase(atlas.skyline.begin() + static_cast<std::ptrdiff_t>(index));
				continue;
			}

			node.x += overlap;
			node.width -= overlap;
			break;
		}

		for (SdSize index = 0; index + 1 < atlas.skyline.size();)
		{
			SkylineNode& node = atlas.skyline[index];
			SkylineNode& nextNode = atlas.skyline[index + 1];
			if (node.y == nextNode.y && node.x + node.width == nextNode.x)
			{
				node.width += nextNode.width;
				atlas.skyline.erase(atlas.skyline.begin() + static_cast<std::ptrdiff_t>(index + 1));
				continue;
			}

			++index;
		}
	}

	bool SdFreeTypeFontBackend::AllocateAtlasRegion(SdUInt32 width, SdUInt32 height, SdUInt32& outX, SdUInt32& outY)
	{
		width += 2;
		height += 2;
		for (;;)
		{
			if ((width > atlas.width - 2u && !GrowAtlas(width + 2u, atlas.height))
				|| (height > atlas.height - 2u && !GrowAtlas(atlas.width, height + 2u)))
				return false;

			SdSize bestIndex = std::numeric_limits<SdSize>::max();
			SdUInt32 bestX = 0;
			SdUInt32 bestY = 0;
			SdUInt32 bestTop = std::numeric_limits<SdUInt32>::max();
			SdUInt32 bestWaste = std::numeric_limits<SdUInt32>::max();
			for (SdSize index = 0; index < atlas.skyline.size(); ++index)
			{
				SdUInt32 candidateY = 0;
				SdUInt32 candidateWaste = 0;
				if (!CalculateSkylinePlacement(index, width, height, candidateY, candidateWaste))
					continue;

				const SkylineNode& node = atlas.skyline[index];
				const SdUInt32 candidateTop = candidateY + height;
				if (candidateTop < bestTop
					|| (candidateTop == bestTop && candidateWaste < bestWaste)
					|| (candidateTop == bestTop && candidateWaste == bestWaste && node.x < bestX))
				{
					bestIndex = index;
					bestX = node.x;
					bestY = candidateY;
					bestTop = candidateTop;
					bestWaste = candidateWaste;
				}
			}

			if (bestIndex != std::numeric_limits<SdSize>::max())
			{
				InsertSkylineNode(bestIndex, bestX, bestY, width, height);
				outX = bestX + 1u;
				outY = bestY + 1u;
				return true;
			}

			if (!GrowAtlas(atlas.width, atlas.height * 2u))
				return false;
		}
	}

	void SdFreeTypeFontBackend::RebuildGlyphUvs()
	{
		for (auto& [key, glyph] : glyphCache)
		{
			(void)key;
			if (!glyph.visible || glyph.atlasWidth == 0 || glyph.atlasHeight == 0)
				continue;

			glyph.uvRect = {
				static_cast<float>(glyph.atlasX) / static_cast<float>(atlas.width),
				static_cast<float>(glyph.atlasY) / static_cast<float>(atlas.height),
				static_cast<float>(glyph.atlasX + glyph.atlasWidth) / static_cast<float>(atlas.width),
				static_cast<float>(glyph.atlasY + glyph.atlasHeight) / static_cast<float>(atlas.height)
			};
		}
	}

	const SdFreeTypeFontBackend::GlyphInfo* SdFreeTypeFontBackend::EnsureGlyph(SdFontHandle font, SdUInt32 codepoint, float pixelSize)
	{
		font = font.IsValid() ? font : fallbackFont;
		const SdUInt32 roundedPixelSize = static_cast<SdUInt32>(std::ceil(pixelSize));
		const GlyphKey key{ font, codepoint, roundedPixelSize };
		if (const auto it = glyphCache.find(key); it != glyphCache.end())
			return &it->second;

		FontFace* face = TryGetFace(font);
		if (!face || !face->face)
			return nullptr;

		FT_Set_Pixel_Sizes(face->face, 0, roundedPixelSize);
		FT_UInt glyphIndex = FT_Get_Char_Index(face->face, static_cast<FT_ULong>(codepoint));
		if (glyphIndex == 0)
			glyphIndex = FT_Get_Char_Index(face->face, static_cast<FT_ULong>('?'));
		if (glyphIndex == 0 || FT_Load_Glyph(face->face, glyphIndex, FT_LOAD_DEFAULT) != 0)
			return nullptr;

		const FT_GlyphSlot slot = face->face->glyph;
		GlyphInfo info = {};
		info.texture = atlas.texture;
		info.bearing = { static_cast<float>(slot->bitmap_left), static_cast<float>(slot->bitmap_top) };
		info.advanceX = static_cast<float>(slot->advance.x) / 64.0f;

		FT_Color* palette = nullptr;
		FT_LayerIterator iterator = {};
		FT_UInt layerGlyphIndex = 0;
		FT_UInt layerColorIndex = 0;
		if (FT_Palette_Select(face->face, 0, &palette) == 0
			&& palette
			&& FT_Get_Color_Glyph_Layer(face->face, glyphIndex, &layerGlyphIndex, &layerColorIndex, &iterator))
		{
			struct RenderedLayer final
			{
				std::vector<SdUInt8> coverage = {};
				FT_Color color = {};
				SdInt32 left = 0;
				SdInt32 top = 0;
				SdUInt32 width = 0;
				SdUInt32 height = 0;
			};

			std::vector<RenderedLayer> layers = {};
			SdInt32 minX = std::numeric_limits<SdInt32>::max();
			SdInt32 minY = std::numeric_limits<SdInt32>::max();
			SdInt32 maxX = std::numeric_limits<SdInt32>::min();
			SdInt32 maxY = std::numeric_limits<SdInt32>::min();
			do
			{
				if (FT_Load_Glyph(face->face, layerGlyphIndex, FT_LOAD_DEFAULT) != 0)
					continue;
				if (FT_Render_Glyph(face->face->glyph, FT_RENDER_MODE_NORMAL) != 0)
					continue;

				const FT_GlyphSlot layerSlot = face->face->glyph;
				const FT_Bitmap& layerBitmap = layerSlot->bitmap;
				if (layerBitmap.width == 0 || layerBitmap.rows == 0 || !layerBitmap.buffer || layerBitmap.pixel_mode != FT_PIXEL_MODE_GRAY)
					continue;

				RenderedLayer layer = {};
				layer.left = layerSlot->bitmap_left;
				layer.top = layerSlot->bitmap_top;
				layer.width = layerBitmap.width;
				layer.height = layerBitmap.rows;
				layer.color = layerColorIndex == 0xFFFFu ? FT_Color{ 255, 255, 255, 255 } : palette[layerColorIndex];
				layer.coverage.resize(static_cast<SdSize>(layer.width) * layer.height);
				const SdSize sourcePitch = static_cast<SdSize>(std::abs(layerBitmap.pitch));
				for (SdUInt32 y = 0; y < layer.height; ++y)
				{
					std::memcpy(
						layer.coverage.data() + static_cast<SdSize>(y) * layer.width,
						layerBitmap.buffer + static_cast<SdSize>(y) * sourcePitch,
						layer.width);
				}

				minX = std::min(minX, layer.left);
				minY = std::min(minY, layer.top - static_cast<SdInt32>(layer.height));
				maxX = std::max(maxX, layer.left + static_cast<SdInt32>(layer.width));
				maxY = std::max(maxY, layer.top);
				layers.push_back(std::move(layer));
			} while (FT_Get_Color_Glyph_Layer(face->face, glyphIndex, &layerGlyphIndex, &layerColorIndex, &iterator));

			if (!layers.empty() && minX < maxX && minY < maxY)
			{
				const SdUInt32 width = static_cast<SdUInt32>(maxX - minX);
				const SdUInt32 height = static_cast<SdUInt32>(maxY - minY);
				SdUInt32 atlasX = 0;
				SdUInt32 atlasY = 0;
				if (!AllocateAtlasRegion(width, height, atlasX, atlasY))
					return nullptr;

				info.bearing = { static_cast<float>(minX), static_cast<float>(maxY) };
				info.size = { static_cast<float>(width), static_cast<float>(height) };
				info.visible = true;
				info.colored = true;
				info.atlasX = atlasX;
				info.atlasY = atlasY;
				info.atlasWidth = width;
				info.atlasHeight = height;

				for (const RenderedLayer& layer : layers)
				{
					const SdUInt32 destinationX = static_cast<SdUInt32>(layer.left - minX);
					const SdUInt32 destinationY = static_cast<SdUInt32>(maxY - layer.top);
					for (SdUInt32 y = 0; y < layer.height; ++y)
					{
						for (SdUInt32 x = 0; x < layer.width; ++x)
						{
							const SdUInt8 coverage = layer.coverage[static_cast<SdSize>(y) * layer.width + x];
							SdUInt32& destination = atlas.pixels[static_cast<SdSize>(atlasY + destinationY + y) * atlas.width + atlasX + destinationX + x];
							destination = AlphaBlendColor(destination, layer.color, coverage);
						}
					}
				}

				info.uvRect = {
					static_cast<float>(atlasX) / static_cast<float>(atlas.width),
					static_cast<float>(atlasY) / static_cast<float>(atlas.height),
					static_cast<float>(atlasX + width) / static_cast<float>(atlas.width),
					static_cast<float>(atlasY + height) / static_cast<float>(atlas.height)
				};
				atlas.dirty = true;
				auto [it, inserted] = glyphCache.emplace(key, info);
				paragraphCache.clear();
				return &it->second;
			}
		}

		if (FT_Load_Glyph(face->face, glyphIndex, FT_LOAD_DEFAULT) != 0)
			return nullptr;

		const FT_GlyphSlot monochromeSlot = face->face->glyph;
		if (FT_Render_Glyph(monochromeSlot, FT_RENDER_MODE_NORMAL) != 0)
			return nullptr;

		const FT_Bitmap& bitmap = monochromeSlot->bitmap;
		info.size = { static_cast<float>(bitmap.width), static_cast<float>(bitmap.rows) };
		info.visible = bitmap.width > 0 && bitmap.rows > 0 && bitmap.buffer;
		if (info.visible)
		{
			SdUInt32 atlasX = 0;
			SdUInt32 atlasY = 0;
			if (!AllocateAtlasRegion(bitmap.width, bitmap.rows, atlasX, atlasY))
				return nullptr;

			info.atlasX = atlasX;
			info.atlasY = atlasY;
			info.atlasWidth = bitmap.width;
			info.atlasHeight = bitmap.rows;
			for (SdUInt32 y = 0; y < bitmap.rows; ++y)
			{
				for (SdUInt32 x = 0; x < bitmap.width; ++x)
				{
					const SdUInt8 alpha = bitmap.buffer[static_cast<SdSize>(y) * std::abs(bitmap.pitch) + x];
					atlas.pixels[static_cast<SdSize>(atlasY + y) * atlas.width + atlasX + x] = SdColor(255, 255, 255, alpha).Pack();
				}
			}

			info.uvRect = {
				static_cast<float>(atlasX) / static_cast<float>(atlas.width),
				static_cast<float>(atlasY) / static_cast<float>(atlas.height),
				static_cast<float>(atlasX + bitmap.width) / static_cast<float>(atlas.width),
				static_cast<float>(atlasY + bitmap.rows) / static_cast<float>(atlas.height)
			};
			atlas.dirty = true;
		}

		auto [it, inserted] = glyphCache.emplace(key, info);
		paragraphCache.clear();
		return &it->second;
	}

	float SdFreeTypeFontBackend::ResolveLineHeight(SdFontHandle font, const SdTextStyle& style)
	{
		if (style.lineHeight > 0.0f)
			return style.lineHeight;
		float lineHeight = style.pixelSize;
		auto includeFont = [&](SdFontHandle candidate)
		{
			const FontFace* face = TryGetFace(candidate);
			if (!face || !face->face)
				return;
			FT_Set_Pixel_Sizes(face->face, 0, static_cast<FT_UInt>(std::ceil(style.pixelSize)));
			lineHeight = std::max(lineHeight, static_cast<float>(face->face->size->metrics.height) / 64.0f);
		};

		includeFont(font);
		for (SdFontHandle fallback : fallbackFonts)
			includeFont(fallback);
		return lineHeight;
	}

	float SdFreeTypeFontBackend::ResolveAscender(SdFontHandle font, float pixelSize)
	{
		float ascender = pixelSize;
		auto includeFont = [&](SdFontHandle candidate)
		{
			const FontFace* face = TryGetFace(candidate);
			if (!face || !face->face)
				return;
			FT_Set_Pixel_Sizes(face->face, 0, static_cast<FT_UInt>(std::ceil(pixelSize)));
			ascender = std::max(ascender, static_cast<float>(face->face->size->metrics.ascender) / 64.0f);
		};

		includeFont(font);
		for (SdFontHandle fallback : fallbackFonts)
			includeFont(fallback);
		return ascender;
	}

	SdVec2 SdFreeTypeFontBackend::MeasureText(SdUtf8StringView text, const SdTextStyle& style)
	{
		return BuildParagraphLayout(text, style, 0.0f, SdColorWhite).size;
	}

	void SdFreeTypeFontBackend::ConfigureRenderSharedData(SdRenderSharedData& sharedData) const
	{
		sharedData.whiteTexture = atlas.texture;
		sharedData.whitePixelUv = atlas.whitePixelUv;
		sharedData.bakedLineUvs = atlas.bakedLineUvs;
	}

	bool SdFreeTypeFontBackend::MatchesStyle(const SdTextStyle& left, const SdTextStyle& right)
	{
		return left.pixelSize == right.pixelSize
			&& left.lineHeight == right.lineHeight
			&& left.align == right.align
			&& left.wrap == right.wrap
			&& left.fontStack.fonts == right.fontStack.fonts;
	}

	SdParagraphLayout SdFreeTypeFontBackend::CopyParagraphLayout(const SdParagraphLayout& layout, std::pmr::memory_resource* resource)
	{
		SdParagraphLayout copy(resource);
		copy.glyphs.assign(layout.glyphs.begin(), layout.glyphs.end());
		copy.lines.assign(layout.lines.begin(), layout.lines.end());
		copy.size = layout.size;
		return copy;
	}

	SdFreeTypeFontBackend::ParagraphCacheEntry* SdFreeTypeFontBackend::FindParagraphCacheEntry(
		SdUtf8StringView text,
		const SdTextStyle& style,
		float maxWidth,
		const SdColor& color)
	{
		for (ParagraphCacheEntry& entry : paragraphCache)
		{
			if (entry.text == text
				&& MatchesStyle(entry.style, style)
				&& entry.maxWidth == maxWidth
				&& entry.color == color)
			{
				entry.lastUsed = ++paragraphCacheClock;
				return &entry;
			}
		}
		return nullptr;
	}

	SdFreeTypeFontBackend::ParagraphCacheEntry& SdFreeTypeFontBackend::StoreParagraphCacheEntry(
		SdUtf8StringView text,
		const SdTextStyle& style,
		float maxWidth,
		const SdColor& color,
		SdParagraphLayout&& layout)
	{
		ParagraphCacheEntry* entry = nullptr;
		if (paragraphCache.size() < kMaxParagraphCacheEntries)
		{
			entry = &paragraphCache.emplace_back();
		}
		else
		{
			auto oldest = std::min_element(paragraphCache.begin(), paragraphCache.end(), [](const ParagraphCacheEntry& left, const ParagraphCacheEntry& right)
			{
				return left.lastUsed < right.lastUsed;
			});
			entry = &*oldest;
		}

		entry->text.assign(text.data(), text.size());
		entry->style = style;
		entry->maxWidth = maxWidth;
		entry->color = color;
		entry->layout.glyphs.assign(layout.glyphs.begin(), layout.glyphs.end());
		entry->layout.lines.assign(layout.lines.begin(), layout.lines.end());
		entry->layout.size = layout.size;
		entry->lastUsed = ++paragraphCacheClock;
		return *entry;
	}

	SdParagraphLayout SdFreeTypeFontBackend::BuildParagraphLayout(
		SdUtf8StringView text,
		const SdTextStyle& style,
		float maxWidth,
		const SdColor& color,
		std::pmr::memory_resource* resource)
	{
		if (ParagraphCacheEntry* entry = FindParagraphCacheEntry(text, style, maxWidth, color))
			return CopyParagraphLayout(entry->layout, resource);

		SdParagraphLayout layout = BuildParagraphLayoutUncached(text, style, maxWidth, color, std::pmr::get_default_resource());
		ParagraphCacheEntry& entry = StoreParagraphCacheEntry(text, style, maxWidth, color, std::move(layout));
		return CopyParagraphLayout(entry.layout, resource);
	}

	SdParagraphLayout SdFreeTypeFontBackend::BuildParagraphLayoutUncached(
		SdUtf8StringView text,
		const SdTextStyle& style,
		float maxWidth,
		const SdColor& color,
		std::pmr::memory_resource* resource)
	{
		SdParagraphLayout layout(resource);
		const SdFontHandle baseFont = ResolveFont(style);
		if (!baseFont.IsValid() || text.empty())
			return layout;

		const float lineHeight = ResolveLineHeight(baseFont, style);
		const float ascender = ResolveAscender(baseFont, style.pixelSize);
		const std::vector<SdUInt32> codepoints = Utf8::DecodeToCodepoints(text);
		float penX = 0.0f;
		float penY = 0.0f;
		float lineWidth = 0.0f;
		SdUInt32 lineStart = 0;

		auto pushLine = [&]()
		{
			SdTextLine line = {};
			line.glyphOffset = lineStart;
			line.glyphCount = static_cast<SdUInt32>(layout.glyphs.size()) - lineStart;
			line.width = lineWidth;
			line.y = penY;
			layout.lines.push_back(line);
			layout.size.x = std::max(layout.size.x, lineWidth);
			layout.size.y = std::max(layout.size.y, penY + lineHeight);
			lineStart = static_cast<SdUInt32>(layout.glyphs.size());
			lineWidth = 0.0f;
		};

		for (SdUInt32 codepoint : codepoints)
		{
			if (codepoint == '\r')
				continue;
			if (codepoint == '\n')
			{
				pushLine();
				penX = 0.0f;
				penY += lineHeight;
				continue;
			}

			const SdFontHandle font = ResolveFontForCodepoint(style, codepoint);
			const GlyphInfo* glyph = EnsureGlyph(font, codepoint, style.pixelSize);
			if (!glyph)
				continue;
			if (maxWidth > 0.0f && style.wrap && penX > 0.0f && penX + glyph->advanceX > maxWidth)
			{
				pushLine();
				penX = 0.0f;
				penY += lineHeight;
			}

			if (glyph->visible)
			{
				SdTextGlyph textGlyph = {};
				textGlyph.font = font;
				textGlyph.texture = glyph->texture;
				textGlyph.uvRect = glyph->uvRect;
				textGlyph.color = glyph->colored ? SdColorWhite : color;
				textGlyph.colored = glyph->colored;
				textGlyph.codepoint = codepoint;
				textGlyph.rect = {
					penX + glyph->bearing.x,
					penY + ascender - glyph->bearing.y,
					penX + glyph->bearing.x + glyph->size.x,
					penY + ascender - glyph->bearing.y + glyph->size.y
				};
				layout.glyphs.push_back(textGlyph);
			}
			penX += glyph->advanceX;
			lineWidth = std::max(lineWidth, penX);
		}

		pushLine();
		return layout;
	}

	void SdFreeTypeFontBackend::DrainPendingUploads(std::vector<SdUploadRequest>& uploads)
	{
		if (!atlas.dirty || atlas.pixels.empty())
			return;

		SdUploadRequest upload = {};
		upload.texture = atlas.texture;
		upload.width = atlas.width;
		upload.height = atlas.height;
		upload.pixels.resize(static_cast<SdSize>(atlas.pixels.size()) * sizeof(SdUInt32));
		std::memcpy(upload.pixels.data(), atlas.pixels.data(), upload.pixels.size());
		uploads.push_back(std::move(upload));
		atlas.dirty = false;
	}
}
