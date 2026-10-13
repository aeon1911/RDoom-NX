// RDoom launcher - image decode/encode (stb, kept private to the launcher).
#pragma once
#include <cstddef>

namespace rdoom {
// RGBA8 pixels or nullptr; free with RdFreeImage.
unsigned char *RdDecodeImage(const void *data, size_t size, int *w, int *h);
void RdFreeImage(unsigned char *px);
bool RdWritePng(const char *path, int w, int h, const void *rgba);
}
