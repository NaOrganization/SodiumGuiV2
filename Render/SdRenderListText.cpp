#include "Render/SdRenderList.h"

#include <cmath>

namespace Sodium
{
	namespace
	{
		SdRect SnapGlyphRect(const SdRect& rect) noexcept
		{
			const SdVec2 snappedMin = { std::round(rect.min.x), std::round(rect.min.y) };
			const SdVec2 offset = snappedMin - rect.min;
			return { snappedMin, rect.max + offset };
		}
	}

	void SdRenderList::AddTextLayout(const SdParagraphLayout& layout, const SdColor& color)
	{
		(void)color;
		for (const SdTextGlyph& glyph : layout.glyphs)
		{
			if (!glyph.texture.IsValid())
				continue;
			PrimRectWithUV(SnapGlyphRect(glyph.rect), glyph.uvRect, glyph.color, glyph.texture);
		}
	}
}
