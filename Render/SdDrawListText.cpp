#include "Render/SdDrawList.h"

namespace Sodium
{
	void SdRenderList::AddTextLayout(const SdParagraphLayout& layout, const SdColor& color, const SdRect& clipRect)
	{
		(void)color;
		for (const SdTextGlyph& glyph : layout.glyphs)
		{
			if (!glyph.texture.IsValid())
				continue;
			PrimRectWithUV(glyph.rect, glyph.uvRect, glyph.color, clipRect, glyph.texture);
		}
	}
}
