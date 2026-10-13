// RDoom launcher - software 2D canvas.
//
// Pixels are 32-bit with bytes R,G,B,A in memory (0xAABBGGRR as a
// little-endian uint32), which is both the libnx framebuffer format
// (PIXEL_FORMAT_RGBA_8888) and what stb_image returns, so images and the
// screen need no conversion.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace rdoom {

constexpr uint32_t Rgba(int r, int g, int b, int a = 255)
{
	return (uint32_t)(r & 255) | ((uint32_t)(g & 255) << 8) | ((uint32_t)(b & 255) << 16) | ((uint32_t)(a & 255) << 24);
}
constexpr uint32_t WithAlpha(uint32_t c, int a) { return (c & 0x00FFFFFFu) | ((uint32_t)(a & 255) << 24); }
uint32_t LerpColor(uint32_t a, uint32_t b, float t);

struct Image
{
	int w = 0, h = 0;
	std::vector<uint32_t> px;
	bool opaque = false;  // every alpha is 255 (enables row copies)
	bool Valid() const { return w > 0 && h > 0 && px.size() == (size_t)w * h; }
	void Release() { px.clear(); px.shrink_to_fit(); w = h = 0; }
};

bool LoadImageFile(const std::string &path, Image &out);
bool LoadImageMemory(const void *data, size_t size, Image &out);
// Box-filtered downscale / bilinear upscale.
Image ScaleImage(const Image &src, int w, int h);
// Scales to cover w x h (crop the overflow, centred).
Image CoverImage(const Image &src, int w, int h);
bool SavePng(const std::string &path, const Image &img);

class Canvas
{
public:
	Canvas(int w, int h);
	// Wraps external memory (the framebuffer); stride in pixels.
	Canvas(uint32_t *pixels, int w, int h, int stride);

	int W() const { return w_; }
	int H() const { return h_; }
	uint32_t *Row(int y) { return px_ + (size_t)y * stride_; }
	const uint32_t *Row(int y) const { return px_ + (size_t)y * stride_; }
	Image Snapshot() const;
	void Retarget(uint32_t *pixels, int stride) { px_ = pixels; stride_ = stride; }

	void SetClip(int x, int y, int w, int h);
	void ResetClip();

	void Clear(uint32_t c);
	void Fill(int x, int y, int w, int h, uint32_t c);
	void FillRound(int x, int y, int w, int h, int r, uint32_t c);
	void GradientV(int x, int y, int w, int h, uint32_t top, uint32_t bottom);
	void GradientH(int x, int y, int w, int h, uint32_t left, uint32_t right);
	void Frame(int x, int y, int w, int h, uint32_t c, int t = 1);
	// Corner brackets, the launcher's panel decoration.
	void Brackets(int x, int y, int w, int h, int len, int t, uint32_t c);
	void Line(int x0, int y0, int x1, int y1, uint32_t c);
	void FillCircle(int cx, int cy, int r, uint32_t c);
	void Blit(const Image &img, int x, int y, int alpha = 255);
	void BlitRegion(const Image &img, int sx, int sy, int w, int h, int x, int y, int alpha = 255);
	// 8-bit coverage mask tinted with c (text).
	void Mask(const uint8_t *mask, int mw, int mh, int mstride, int x, int y, uint32_t c);

private:
	void Blend(uint32_t &dst, uint32_t src, int extraAlpha);

	std::vector<uint32_t> own_;
	uint32_t *px_ = nullptr;
	int w_ = 0, h_ = 0, stride_ = 0;
	int cx0_ = 0, cy0_ = 0, cx1_ = 0, cy1_ = 0;  // clip, exclusive max
};

} // namespace rdoom
