#define STB_TRUETYPE_IMPLEMENTATION
#include "StbFontBackend.h"

#include "Core/SdDefaultFont.h"

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

		float GetScale(const stbtt_fontinfo& info, float pixelSize)
		{
			if (pixelSize <= 0.0f)
				return 0.0f;
			return stbtt_ScaleForPixelHeight(&info, pixelSize);
		}
	}

	SdSize SdStbFontBackend::GlyphKeyHash::operator()(const GlyphKey& key) const noexcept
	{
		SdUInt64 value = key.font.index;
		value = (value * 16777619ull) ^ key.font.generation;
		value = (value * 16777619ull) ^ key.codepoint;
		value = (value * 16777619ull) ^ key.pixelSize;
		return static_cast<SdSize>(value);
	}

	bool SdStbFontBackend::Initialize(const Config& backendConfig)
	{
		config = backendConfig;
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

	void SdStbFontBackend::Shutdown()
	{
		faces.clear();
		families.clear();
		fallbackFonts.clear();
		glyphCache.clear();
		paragraphCache.clear();
		paragraphCacheClock = 0;
		atlas = {};
		fallbackFont = {};
	}

	SdStbFontBackend::FontFace* SdStbFontBackend::TryGetFace(SdFontHandle handle) noexcept
	{
		for (FontFace& face : faces)
		{
			if (face.handle == handle)
				return &face;
		}
		return nullptr;
	}

	const SdStbFontBackend::FontFace* SdStbFontBackend::TryGetFace(SdFontHandle handle) const noexcept
	{
		for (const FontFace& face : faces)
		{
			if (face.handle == handle)
				return &face;
		}
		return nullptr;
	}

	SdStbFontBackend::FontFamily* SdStbFontBackend::TryGetFamily(SdFontFamilyHandle handle) noexcept
	{
		for (FontFamily& family : families)
		{
			if (family.handle == handle)
				return &family;
		}
		return nullptr;
	}

	const SdStbFontBackend::FontFamily* SdStbFontBackend::TryGetFamily(SdFontFamilyHandle handle) const noexcept
	{
		for (const FontFamily& family : families)
		{
			if (family.handle == handle)
				return &family;
		}
		return nullptr;
	}

	SdFontFamilyHandle SdStbFontBackend::RegisterFontFromPath(SdUtf8StringView familyName, const SdPath& path)
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

	SdFontFamilyHandle SdStbFontBackend::RegisterFontFromMemory(SdUtf8StringView familyName, std::span<const std::byte> bytes, SdUtf8StringView debugName)
	{
		if (bytes.empty())
			return {};

		FontFace face = {};
		face.handle = SdFontHandle(nextFontIndex++);
		face.bytes.assign(bytes.begin(), bytes.end());
		face.debugName = debugName.empty() ? SdUtf8String(familyName) : SdUtf8String(debugName);

		const unsigned char* data = reinterpret_cast<const unsigned char*>(face.bytes.data());
		const int fontOffset = stbtt_GetFontOffsetForIndex(data, 0);
		if (fontOffset < 0 || stbtt_InitFont(&face.info, data, fontOffset) == 0)
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

	std::vector<SdFontHandle> SdStbFontBackend::GetFamilyFaces(SdFontFamilyHandle family) const
	{
		if (const FontFamily* entry = TryGetFamily(family))
			return entry->faces;
		return {};
	}

	SdFontHandle SdStbFontBackend::LoadFont(const SdFontRequest& request)
	{
		const SdFontFamilyHandle family = RegisterFontFromPath(request.familyName, request.path);
		const std::vector<SdFontHandle> facesInFamily = GetFamilyFaces(family);
		return facesInFamily.empty() ? SdFontHandle{} : facesInFamily.front();
	}

	SdGlyphRun SdStbFontBackend::ShapeText(SdFontHandle font, std::u32string_view text, const SdTextShapeOptions& options)
	{
		SdGlyphRun run = {};
		if (text.empty())
			return run;

		float penX = 0.0f;
		for (char32_t codepoint : text)
		{
			const SdFontHandle resolvedFont = ResolveFontForCodepoint(font, static_cast<SdUInt32>(codepoint));
			FontFace* face = TryGetFace(resolvedFont);
			if (!face)
				continue;

			int glyphIndex = stbtt_FindGlyphIndex(&face->info, static_cast<int>(codepoint));
			if (glyphIndex == 0)
				glyphIndex = stbtt_FindGlyphIndex(&face->info, '?');
			if (glyphIndex == 0)
				continue;

			int advance = 0;
			int leftSideBearing = 0;
			stbtt_GetGlyphHMetrics(&face->info, glyphIndex, &advance, &leftSideBearing);
			run.glyphIds.push_back(static_cast<SdGlyphId>(glyphIndex));
			run.positions.push_back({ penX, 0.0f });
			penX += static_cast<float>(advance) * GetScale(face->info, options.pixelSize);
		}
		run.advance = { penX, 0.0f };
		return run;
	}

	SdTextMetrics SdStbFontBackend::MeasureText(SdFontHandle font, std::u32string_view text, const SdTextMeasureOptions& options)
	{
		SdTextMetrics metrics = {};
		FontFace* face = TryGetFace(font.IsValid() ? font : fallbackFont);
		if (!face)
			return metrics;

		const SdGlyphRun run = ShapeText(face->handle, text, SdTextShapeOptions{ options.pixelSize });
		const float scale = GetScale(face->info, options.pixelSize);
		int ascent = 0;
		int descent = 0;
		int lineGap = 0;
		stbtt_GetFontVMetrics(&face->info, &ascent, &descent, &lineGap);
		metrics.width = run.advance.x;
		metrics.height = static_cast<float>(ascent - descent + lineGap) * scale;
		metrics.ascender = static_cast<float>(ascent) * scale;
		metrics.descender = static_cast<float>(descent) * scale;
		return metrics;
	}

	SdGlyphBitmap SdStbFontBackend::RasterizeGlyph(SdFontHandle font, SdGlyphId glyphId, float size)
	{
		SdGlyphBitmap bitmap = {};
		FontFace* face = TryGetFace(font.IsValid() ? font : fallbackFont);
		if (!face || glyphId == 0 || size <= 0.0f)
			return bitmap;

		int width = 0;
		int height = 0;
		int offsetX = 0;
		int offsetY = 0;
		unsigned char* source = stbtt_GetGlyphBitmap(&face->info, 0.0f, GetScale(face->info, size), static_cast<int>(glyphId.value), &width, &height, &offsetX, &offsetY);
		if (!source || width <= 0 || height <= 0)
		{
			if (source)
				stbtt_FreeBitmap(source, nullptr);
			return bitmap;
		}

		bitmap.width = static_cast<SdUInt32>(width);
		bitmap.height = static_cast<SdUInt32>(height);
		bitmap.pitch = width;
		const SdSize byteCount = static_cast<SdSize>(width) * static_cast<SdSize>(height);
		bitmap.buffer.assign(reinterpret_cast<const std::byte*>(source), reinterpret_cast<const std::byte*>(source) + byteCount);
		stbtt_FreeBitmap(source, nullptr);
		return bitmap;
	}

	SdFontHandle SdStbFontBackend::ResolveFont(const SdTextStyle& style) const noexcept
	{
		for (SdFontHandle font : style.fontStack.fonts)
		{
			if (font.IsValid() && TryGetFace(font))
				return font;
		}
		return fallbackFont;
	}

	SdFontHandle SdStbFontBackend::ResolveFontForCodepoint(const SdTextStyle& style, SdUInt32 codepoint) const noexcept
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

	SdFontHandle SdStbFontBackend::ResolveFontForCodepoint(SdFontHandle preferredFont, SdUInt32 codepoint) const noexcept
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

	bool SdStbFontBackend::HasGlyph(SdFontHandle font, SdUInt32 codepoint) const noexcept
	{
		const FontFace* face = TryGetFace(font);
		return face && stbtt_FindGlyphIndex(&face->info, static_cast<int>(codepoint)) != 0;
	}

	void SdStbFontBackend::AddFallbackFont(SdFontHandle font)
	{
		if (!font.IsValid())
			return;
		if (std::find(fallbackFonts.begin(), fallbackFonts.end(), font) != fallbackFonts.end())
			return;
		fallbackFonts.push_back(font);
	}

	void SdStbFontBackend::InitializeAtlas(SdUInt32 width, SdUInt32 height)
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

	void SdStbFontBackend::BakeAtlasDrawPrimitives()
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

	void SdStbFontBackend::RebuildAtlasDrawPrimitiveUvs()
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

	bool SdStbFontBackend::GrowAtlas(SdUInt32 minWidth, SdUInt32 minHeight)
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

	bool SdStbFontBackend::CalculateSkylinePlacement(SdSize nodeIndex, SdUInt32 width, SdUInt32 height, SdUInt32& outY, SdUInt32& outWaste) const
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

	void SdStbFontBackend::InsertSkylineNode(SdSize nodeIndex, SdUInt32 x, SdUInt32 y, SdUInt32 width, SdUInt32 height)
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

	bool SdStbFontBackend::AllocateAtlasRegion(SdUInt32 width, SdUInt32 height, SdUInt32& outX, SdUInt32& outY)
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

	void SdStbFontBackend::RebuildGlyphUvs()
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

	const SdStbFontBackend::GlyphInfo* SdStbFontBackend::EnsureGlyph(SdFontHandle font, SdUInt32 codepoint, float pixelSize)
	{
		font = font.IsValid() ? font : fallbackFont;
		const SdUInt32 roundedPixelSize = static_cast<SdUInt32>(std::ceil(pixelSize));
		const GlyphKey key{ font, codepoint, roundedPixelSize };
		if (const auto it = glyphCache.find(key); it != glyphCache.end())
			return &it->second;

		FontFace* face = TryGetFace(font);
		if (!face)
			return nullptr;

		int glyphIndex = stbtt_FindGlyphIndex(&face->info, static_cast<int>(codepoint));
		if (glyphIndex == 0)
			glyphIndex = stbtt_FindGlyphIndex(&face->info, '?');
		if (glyphIndex == 0)
			return nullptr;

		const float scale = GetScale(face->info, pixelSize);
		int advance = 0;
		int leftSideBearing = 0;
		stbtt_GetGlyphHMetrics(&face->info, glyphIndex, &advance, &leftSideBearing);

		GlyphInfo info = {};
		info.texture = atlas.texture;
		info.advanceX = static_cast<float>(advance) * scale;

		int width = 0;
		int height = 0;
		int offsetX = 0;
		int offsetY = 0;
		unsigned char* bitmap = stbtt_GetGlyphBitmap(&face->info, 0.0f, scale, glyphIndex, &width, &height, &offsetX, &offsetY);
		info.bearing = { static_cast<float>(offsetX), static_cast<float>(-offsetY) };
		info.size = { static_cast<float>(std::max(width, 0)), static_cast<float>(std::max(height, 0)) };
		info.visible = bitmap && width > 0 && height > 0;
		if (info.visible)
		{
			SdUInt32 atlasX = 0;
			SdUInt32 atlasY = 0;
			if (!AllocateAtlasRegion(static_cast<SdUInt32>(width), static_cast<SdUInt32>(height), atlasX, atlasY))
			{
				stbtt_FreeBitmap(bitmap, nullptr);
				return nullptr;
			}

			info.atlasX = atlasX;
			info.atlasY = atlasY;
			info.atlasWidth = static_cast<SdUInt32>(width);
			info.atlasHeight = static_cast<SdUInt32>(height);
			for (int y = 0; y < height; ++y)
			{
				for (int x = 0; x < width; ++x)
				{
					const SdUInt8 alpha = bitmap[static_cast<SdSize>(y) * static_cast<SdSize>(width) + static_cast<SdSize>(x)];
					atlas.pixels[static_cast<SdSize>(atlasY + static_cast<SdUInt32>(y)) * atlas.width + atlasX + static_cast<SdUInt32>(x)] = SdColor(255, 255, 255, alpha).Pack();
				}
			}

			info.uvRect = {
				static_cast<float>(atlasX) / static_cast<float>(atlas.width),
				static_cast<float>(atlasY) / static_cast<float>(atlas.height),
				static_cast<float>(atlasX + static_cast<SdUInt32>(width)) / static_cast<float>(atlas.width),
				static_cast<float>(atlasY + static_cast<SdUInt32>(height)) / static_cast<float>(atlas.height)
			};
			atlas.dirty = true;
		}
		if (bitmap)
			stbtt_FreeBitmap(bitmap, nullptr);

		auto [it, inserted] = glyphCache.emplace(key, info);
		paragraphCache.clear();
		return &it->second;
	}

	float SdStbFontBackend::ResolveLineHeight(SdFontHandle font, const SdTextStyle& style)
	{
		if (style.lineHeight > 0.0f)
			return style.lineHeight;
		float lineHeight = style.pixelSize;
		auto includeFont = [&](SdFontHandle candidate)
		{
			const FontFace* face = TryGetFace(candidate);
			if (!face)
				return;

			int ascent = 0;
			int descent = 0;
			int lineGap = 0;
			stbtt_GetFontVMetrics(&face->info, &ascent, &descent, &lineGap);
			lineHeight = std::max(lineHeight, static_cast<float>(ascent - descent + lineGap) * GetScale(face->info, style.pixelSize));
		};

		includeFont(font);
		for (SdFontHandle fallback : fallbackFonts)
			includeFont(fallback);
		return lineHeight;
	}

	float SdStbFontBackend::ResolveAscender(SdFontHandle font, float pixelSize)
	{
		float ascender = pixelSize;
		auto includeFont = [&](SdFontHandle candidate)
		{
			const FontFace* face = TryGetFace(candidate);
			if (!face)
				return;

			int ascent = 0;
			int descent = 0;
			int lineGap = 0;
			stbtt_GetFontVMetrics(&face->info, &ascent, &descent, &lineGap);
			(void)descent;
			(void)lineGap;
			ascender = std::max(ascender, static_cast<float>(ascent) * GetScale(face->info, pixelSize));
		};

		includeFont(font);
		for (SdFontHandle fallback : fallbackFonts)
			includeFont(fallback);
		return ascender;
	}

	SdVec2 SdStbFontBackend::MeasureText(SdUtf8StringView text, const SdTextStyle& style)
	{
		return BuildParagraphLayout(text, style, 0.0f, SdColorWhite).size;
	}

	void SdStbFontBackend::ConfigureRenderSharedData(SdRenderSharedData& sharedData) const
	{
		sharedData.whiteTexture = atlas.texture;
		sharedData.whitePixelUv = atlas.whitePixelUv;
		sharedData.bakedLineUvs = atlas.bakedLineUvs;
	}

	bool SdStbFontBackend::MatchesStyle(const SdTextStyle& left, const SdTextStyle& right)
	{
		return left.pixelSize == right.pixelSize
			&& left.lineHeight == right.lineHeight
			&& left.align == right.align
			&& left.wrap == right.wrap
			&& left.fontStack.fonts == right.fontStack.fonts;
	}

	SdParagraphLayout SdStbFontBackend::CopyParagraphLayout(const SdParagraphLayout& layout, std::pmr::memory_resource* resource)
	{
		SdParagraphLayout copy(resource);
		copy.glyphs.assign(layout.glyphs.begin(), layout.glyphs.end());
		copy.lines.assign(layout.lines.begin(), layout.lines.end());
		copy.size = layout.size;
		return copy;
	}

	SdStbFontBackend::ParagraphCacheEntry* SdStbFontBackend::FindParagraphCacheEntry(
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

	SdStbFontBackend::ParagraphCacheEntry& SdStbFontBackend::StoreParagraphCacheEntry(
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

	SdParagraphLayout SdStbFontBackend::BuildParagraphLayout(
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

	SdParagraphLayout SdStbFontBackend::BuildParagraphLayoutUncached(
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
				textGlyph.color = color;
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

	void SdStbFontBackend::DrainPendingUploads(std::vector<SdUploadRequest>& uploads)
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
