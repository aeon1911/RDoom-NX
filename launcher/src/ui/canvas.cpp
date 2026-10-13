#include "canvas.h"

#include <algorithm>
#include <cmath>
#include <cstring>

#include "rd_image.h"
#include "fsutil.h"

namespace rdoom {

uint32_t LerpColor(uint32_t a, uint32_t b, float t)
{
	t = t < 0 ? 0 : (t > 1 ? 1 : t);
	uint32_t out = 0;
	for (int s = 0; s < 32; s += 8)
	{
		int ca = (a >> s) & 255, cb = (b >> s) & 255;
		out |= (uint32_t)(int)(ca + (cb - ca) * t + 0.5f) << s;
	}
	return out;
}

static void FinishImage(Image &out)
{
	out.opaque = true;
	for (uint32_t p : out.px)
		if ((p >> 24) != 255)
		{
			out.opaque = false;
			break;
		}
}

bool LoadImageMemory(const void *data, size_t size, Image &out)
{
	int w, h;
	unsigned char *px = RdDecodeImage(data, size, &w, &h);
	if (!px)
		return false;
	out.w = w;
	out.h = h;
	out.px.assign((const uint32_t *)px, (const uint32_t *)px + (size_t)w * h);
	RdFreeImage(px);
	FinishImage(out);
	return true;
}

bool LoadImageFile(const std::string &path, Image &out)
{
	std::string data;
	if (!ReadFile(path, data))
		return false;
	return LoadImageMemory(data.data(), data.size(), out);
}

bool SavePng(const std::string &path, const Image &img)
{
	return RdWritePng(path.c_str(), img.w, img.h, img.px.data());
}

Image ScaleImage(const Image &src, int w, int h)
{
	Image out;
	if (!src.Valid() || w <= 0 || h <= 0)
		return out;
	out.w = w;
	out.h = h;
	out.px.resize((size_t)w * h);
	const float sx = (float)src.w / w, sy = (float)src.h / h;
	for (int y = 0; y < h; y++)
	{
		for (int x = 0; x < w; x++)
		{
			if (sx > 1.0f || sy > 1.0f)
			{
				// box filter over the source footprint
				int x0 = (int)(x * sx), x1 = std::max(x0 + 1, (int)((x + 1) * sx));
				int y0 = (int)(y * sy), y1 = std::max(y0 + 1, (int)((y + 1) * sy));
				x1 = std::min(x1, src.w);
				y1 = std::min(y1, src.h);
				uint32_t acc[4] = {};
				int n = 0;
				for (int yy = y0; yy < y1; yy++)
					for (int xx = x0; xx < x1; xx++)
					{
						uint32_t p = src.px[(size_t)yy * src.w + xx];
						for (int c = 0; c < 4; c++)
							acc[c] += (p >> (c * 8)) & 255;
						n++;
					}
				uint32_t p = 0;
				for (int c = 0; c < 4; c++)
					p |= (acc[c] / (uint32_t)n) << (c * 8);
				out.px[(size_t)y * w + x] = p;
			}
			else
			{
				float fx = (x + 0.5f) * sx - 0.5f, fy = (y + 0.5f) * sy - 0.5f;
				int ix = std::max(0, (int)std::floor(fx)), iy = std::max(0, (int)std::floor(fy));
				int ix1 = std::min(src.w - 1, ix + 1), iy1 = std::min(src.h - 1, iy + 1);
				float tx = std::max(0.0f, fx - ix), ty = std::max(0.0f, fy - iy);
				uint32_t a = LerpColor(src.px[(size_t)iy * src.w + ix], src.px[(size_t)iy * src.w + ix1], tx);
				uint32_t b = LerpColor(src.px[(size_t)iy1 * src.w + ix], src.px[(size_t)iy1 * src.w + ix1], tx);
				out.px[(size_t)y * w + x] = LerpColor(a, b, ty);
			}
		}
	}
	out.opaque = src.opaque;
	return out;
}

Image CoverImage(const Image &src, int w, int h)
{
	if (!src.Valid())
		return Image();
	float s = std::max((float)w / src.w, (float)h / src.h);
	int sw = std::max(w, (int)std::ceil(src.w * s)), sh = std::max(h, (int)std::ceil(src.h * s));
	Image scaled = ScaleImage(src, sw, sh);
	Image out;
	out.w = w;
	out.h = h;
	out.px.resize((size_t)w * h);
	int ox = (sw - w) / 2, oy = (sh - h) / 2;
	for (int y = 0; y < h; y++)
		memcpy(&out.px[(size_t)y * w], &scaled.px[(size_t)(y + oy) * sw + ox], (size_t)w * 4);
	out.opaque = src.opaque;
	return out;
}

Canvas::Canvas(int w, int h) : own_((size_t)w * h), px_(own_.data()), w_(w), h_(h), stride_(w)
{
	ResetClip();
}

Canvas::Canvas(uint32_t *pixels, int w, int h, int stride) : px_(pixels), w_(w), h_(h), stride_(stride)
{
	ResetClip();
}

Image Canvas::Snapshot() const
{
	Image img;
	img.w = w_;
	img.h = h_;
	img.px.resize((size_t)w_ * h_);
	for (int y = 0; y < h_; y++)
		memcpy(&img.px[(size_t)y * w_], Row(y), (size_t)w_ * 4);
	return img;
}

void Canvas::SetClip(int x, int y, int w, int h)
{
	cx0_ = std::max(0, x);
	cy0_ = std::max(0, y);
	cx1_ = std::min(w_, x + w);
	cy1_ = std::min(h_, y + h);
}

void Canvas::ResetClip()
{
	cx0_ = cy0_ = 0;
	cx1_ = w_;
	cy1_ = h_;
}

inline void Canvas::Blend(uint32_t &dst, uint32_t src, int extraAlpha)
{
	int a = (int)(src >> 24) * extraAlpha / 255;
	if (a <= 0)
		return;
	if (a >= 255)
	{
		dst = src | 0xFF000000u;
		return;
	}
	const int ia = 255 - a;
	uint32_t rb = ((src & 0x00FF00FFu) * a + (dst & 0x00FF00FFu) * ia) >> 8;
	uint32_t g = ((src & 0x0000FF00u) * a + (dst & 0x0000FF00u) * ia) >> 8;
	dst = (rb & 0x00FF00FFu) | (g & 0x0000FF00u) | 0xFF000000u;
}

void Canvas::Clear(uint32_t c)
{
	for (int y = 0; y < h_; y++)
		std::fill(Row(y), Row(y) + w_, c);
}

void Canvas::Fill(int x, int y, int w, int h, uint32_t c)
{
	int x0 = std::max(x, cx0_), y0 = std::max(y, cy0_);
	int x1 = std::min(x + w, cx1_), y1 = std::min(y + h, cy1_);
	if (x0 >= x1 || y0 >= y1)
		return;
	if ((c >> 24) == 255)
	{
		for (int yy = y0; yy < y1; yy++)
			std::fill(Row(yy) + x0, Row(yy) + x1, c);
		return;
	}
	for (int yy = y0; yy < y1; yy++)
	{
		uint32_t *r = Row(yy);
		for (int xx = x0; xx < x1; xx++)
			Blend(r[xx], c, 255);
	}
}

void Canvas::FillRound(int x, int y, int w, int h, int r, uint32_t c)
{
	r = std::min(r, std::min(w, h) / 2);
	if (r <= 0)
	{
		Fill(x, y, w, h, c);
		return;
	}
	Fill(x, y + r, w, h - 2 * r, c);
	for (int i = 0; i < r; i++)
	{
		// anti-aliased edges: coverage of the outermost pixel
		float dy = r - i - 0.5f;
		float half = std::sqrt(std::max(0.0f, (float)r * r - dy * dy));
		int inner = (int)half;
		float frac = half - inner;
		int span = w - 2 * (r - inner);
		Fill(x + r - inner, y + i, span, 1, c);
		Fill(x + r - inner, y + h - 1 - i, span, 1, c);
		uint32_t edge = WithAlpha(c, (int)((c >> 24) * frac));
		Fill(x + r - inner - 1, y + i, 1, 1, edge);
		Fill(x + w - r + inner, y + i, 1, 1, edge);
		Fill(x + r - inner - 1, y + h - 1 - i, 1, 1, edge);
		Fill(x + w - r + inner, y + h - 1 - i, 1, 1, edge);
	}
}

void Canvas::GradientV(int x, int y, int w, int h, uint32_t top, uint32_t bottom)
{
	for (int i = 0; i < h; i++)
		Fill(x, y + i, w, 1, LerpColor(top, bottom, h > 1 ? (float)i / (h - 1) : 0));
}

void Canvas::GradientH(int x, int y, int w, int h, uint32_t left, uint32_t right)
{
	for (int i = 0; i < w; i++)
		Fill(x + i, y, 1, h, LerpColor(left, right, w > 1 ? (float)i / (w - 1) : 0));
}

void Canvas::Frame(int x, int y, int w, int h, uint32_t c, int t)
{
	Fill(x, y, w, t, c);
	Fill(x, y + h - t, w, t, c);
	Fill(x, y + t, t, h - 2 * t, c);
	Fill(x + w - t, y + t, t, h - 2 * t, c);
}

void Canvas::Brackets(int x, int y, int w, int h, int len, int t, uint32_t c)
{
	Fill(x, y, len, t, c);
	Fill(x, y, t, len, c);
	Fill(x + w - len, y, len, t, c);
	Fill(x + w - t, y, t, len, c);
	Fill(x, y + h - t, len, t, c);
	Fill(x, y + h - len, t, len, c);
	Fill(x + w - len, y + h - t, len, t, c);
	Fill(x + w - t, y + h - len, t, len, c);
}

void Canvas::Line(int x0, int y0, int x1, int y1, uint32_t c)
{
	int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
	int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
	int err = dx + dy;
	for (;;)
	{
		Fill(x0, y0, 1, 1, c);
		if (x0 == x1 && y0 == y1)
			break;
		int e2 = 2 * err;
		if (e2 >= dy) { err += dy; x0 += sx; }
		if (e2 <= dx) { err += dx; y0 += sy; }
	}
}

void Canvas::FillCircle(int cx, int cy, int r, uint32_t c)
{
	FillRound(cx - r, cy - r, 2 * r, 2 * r, r, c);
}

void Canvas::Blit(const Image &img, int x, int y, int alpha)
{
	BlitRegion(img, 0, 0, img.w, img.h, x, y, alpha);
}

void Canvas::BlitRegion(const Image &img, int sx, int sy, int w, int h, int x, int y, int alpha)
{
	if (!img.Valid() || alpha <= 0)
		return;
	int x0 = std::max(x, cx0_), y0 = std::max(y, cy0_);
	int x1 = std::min(x + w, cx1_), y1 = std::min(y + h, cy1_);
	if (x0 >= x1 || y0 >= y1)
		return;
	for (int yy = y0; yy < y1; yy++)
	{
		const uint32_t *s = &img.px[(size_t)(sy + yy - y) * img.w + sx + (x0 - x)];
		uint32_t *d = Row(yy) + x0;
		if (img.opaque && alpha >= 255)
		{
			memcpy(d, s, (size_t)(x1 - x0) * 4);
			continue;
		}
		for (int i = 0; i < x1 - x0; i++)
			Blend(d[i], s[i], alpha);
	}
}

void Canvas::Mask(const uint8_t *mask, int mw, int mh, int mstride, int x, int y, uint32_t c)
{
	int x0 = std::max(x, cx0_), y0 = std::max(y, cy0_);
	int x1 = std::min(x + mw, cx1_), y1 = std::min(y + mh, cy1_);
	const int ca = (int)(c >> 24);
	for (int yy = y0; yy < y1; yy++)
	{
		const uint8_t *m = mask + (size_t)(yy - y) * mstride + (x0 - x);
		uint32_t *d = Row(yy) + x0;
		for (int i = 0; i < x1 - x0; i++)
			if (m[i])
				Blend(d[i], WithAlpha(c, m[i] * ca / 255), 255);
	}
}

} // namespace rdoom
