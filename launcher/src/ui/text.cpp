#include "text.h"

#include <cmath>
#include <unordered_map>
#include <vector>

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"

#include "fonts/rd_font_bebas.h"
#include "fonts/rd_font_poppins_bold.h"
#include "fonts/rd_font_poppins_medium.h"

namespace rdoom {

namespace {

struct FontData
{
	stbtt_fontinfo info;
	bool ok = false;
};

struct Glyph
{
	int w = 0, h = 0, xoff = 0, yoff = 0;
	float advance = 0;
	std::vector<uint8_t> bmp;
};

FontData g_fonts[3];
std::unordered_map<uint64_t, Glyph> g_cache;

FontData &F(Font f) { return g_fonts[(int)f]; }

float Scale(Font f, int px)
{
	return stbtt_ScaleForPixelHeight(&F(f).info, (float)px);
}

const Glyph &GetGlyph(Font f, int px, uint32_t cp)
{
	const uint64_t key = ((uint64_t)f << 48) | ((uint64_t)px << 32) | cp;
	auto it = g_cache.find(key);
	if (it != g_cache.end())
		return it->second;
	Glyph g;
	FontData &fd = F(f);
	if (fd.ok)
	{
		float s = Scale(f, px);
		int adv, lsb;
		stbtt_GetCodepointHMetrics(&fd.info, (int)cp, &adv, &lsb);
		g.advance = adv * s;
		int x0, y0, x1, y1;
		stbtt_GetCodepointBitmapBox(&fd.info, (int)cp, s, s, &x0, &y0, &x1, &y1);
		g.w = x1 - x0;
		g.h = y1 - y0;
		g.xoff = x0;
		g.yoff = y0;
		if (g.w > 0 && g.h > 0)
		{
			g.bmp.resize((size_t)g.w * g.h);
			stbtt_MakeCodepointBitmap(&fd.info, g.bmp.data(), g.w, g.h, g.w, s, s, (int)cp);
		}
	}
	return g_cache.emplace(key, std::move(g)).first->second;
}

uint32_t NextCodepoint(const std::string &s, size_t &i)
{
	unsigned char c = (unsigned char)s[i++];
	if (c < 0x80)
		return c;
	int extra = (c >= 0xF0) ? 3 : (c >= 0xE0) ? 2 : (c >= 0xC0) ? 1 : 0;
	uint32_t cp = c & (0x3F >> extra);
	for (int k = 0; k < extra && i < s.size(); k++)
		cp = (cp << 6) | ((unsigned char)s[i++] & 0x3F);
	return cp;
}

int Ascent(Font f, int px)
{
	int asc, desc, gap;
	stbtt_GetFontVMetrics(&F(f).info, &asc, &desc, &gap);
	return (int)std::lround(asc * Scale(f, px));
}

} // namespace

void TextInit()
{
	const unsigned char *data[3] = { rd_font_bebas, rd_font_poppins_medium, rd_font_poppins_bold };
	for (int i = 0; i < 3; i++)
		g_fonts[i].ok = stbtt_InitFont(&g_fonts[i].info, data[i], stbtt_GetFontOffsetForIndex(data[i], 0)) != 0;
}

void TextShutdown()
{
	g_cache.clear();
	g_cache.rehash(0);
}

int TextLineHeight(Font f, int px)
{
	int asc, desc, gap;
	stbtt_GetFontVMetrics(&F(f).info, &asc, &desc, &gap);
	return (int)std::lround((asc - desc + gap) * Scale(f, px));
}

int TextWidth(Font f, int px, const std::string &s)
{
	if (!F(f).ok)
		return 0;
	float x = 0;
	uint32_t prev = 0;
	const float sc = Scale(f, px);
	for (size_t i = 0; i < s.size();)
	{
		uint32_t cp = NextCodepoint(s, i);
		if (prev)
			x += stbtt_GetCodepointKernAdvance(&F(f).info, (int)prev, (int)cp) * sc;
		x += GetGlyph(f, px, cp).advance;
		prev = cp;
	}
	return (int)std::ceil(x);
}

std::string Ellipsize(Font f, int px, const std::string &s, int maxWidth)
{
	if (maxWidth <= 0 || TextWidth(f, px, s) <= maxWidth)
		return s;
	const std::string dots = "…";
	std::string cur;
	for (size_t i = 0; i < s.size();)
	{
		size_t start = i;
		NextCodepoint(s, i);
		std::string next = cur + s.substr(start, i - start);
		if (TextWidth(f, px, next + dots) > maxWidth)
			break;
		cur = next;
	}
	return cur + dots;
}

int DrawText(Canvas &c, Font f, int px, int x, int y, uint32_t color, const std::string &text, int maxWidth)
{
	if (!F(f).ok)
		return 0;
	const std::string s = maxWidth > 0 ? Ellipsize(f, px, text, maxWidth) : text;
	const int base = y + Ascent(f, px);
	const float sc = Scale(f, px);
	float pen = (float)x;
	uint32_t prev = 0;
	for (size_t i = 0; i < s.size();)
	{
		uint32_t cp = NextCodepoint(s, i);
		if (prev)
			pen += stbtt_GetCodepointKernAdvance(&F(f).info, (int)prev, (int)cp) * sc;
		const Glyph &g = GetGlyph(f, px, cp);
		if (!g.bmp.empty())
			c.Mask(g.bmp.data(), g.w, g.h, g.w, (int)std::lround(pen) + g.xoff, base + g.yoff, color);
		pen += g.advance;
		prev = cp;
	}
	return (int)std::ceil(pen) - x;
}

int DrawTextCentered(Canvas &c, Font f, int px, int cx, int y, uint32_t color, const std::string &s)
{
	return DrawText(c, f, px, cx - TextWidth(f, px, s) / 2, y, color, s);
}

int DrawTextRight(Canvas &c, Font f, int px, int rx, int y, uint32_t color, const std::string &s)
{
	return DrawText(c, f, px, rx - TextWidth(f, px, s), y, color, s);
}

int DrawTextWrapped(Canvas &c, Font f, int px, int x, int y, int width, uint32_t color, const std::string &text,
                    int maxLines, int lineGap)
{
	const int lh = TextLineHeight(f, px) + lineGap;
	std::vector<std::string> lines;
	std::string line;
	size_t i = 0;
	while (i <= text.size())
	{
		size_t sp = text.find_first_of(" \n", i);
		std::string word = text.substr(i, sp == std::string::npos ? std::string::npos : sp - i);
		std::string cand = line.empty() ? word : line + " " + word;
		if (!line.empty() && TextWidth(f, px, cand) > width)
		{
			lines.push_back(line);
			line = word;
		}
		else
			line = cand;
		if (sp != std::string::npos && text[sp] == '\n')
		{
			lines.push_back(line);
			line.clear();
		}
		if (sp == std::string::npos)
			break;
		i = sp + 1;
	}
	if (!line.empty())
		lines.push_back(line);
	if (maxLines > 0 && (int)lines.size() > maxLines)
	{
		lines.resize(maxLines);
		lines.back() = Ellipsize(f, px, lines.back() + " …", width);
	}
	for (size_t k = 0; k < lines.size(); k++)
		DrawText(c, f, px, x, y + (int)k * lh, color, lines[k], width);
	return (int)lines.size() * lh;
}

} // namespace rdoom
