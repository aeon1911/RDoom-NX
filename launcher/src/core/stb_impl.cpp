// Single translation unit for the stb code used by the launcher. The
// implementations are static (the engine links its own stb_image), and the
// rest of the launcher calls them through rd_image.h.
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#include "stb_image.h"
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "rd_image.h"

namespace rdoom {

unsigned char *RdDecodeImage(const void *data, size_t size, int *w, int *h)
{
	int n = 0;
	return stbi_load_from_memory((const unsigned char *)data, (int)size, w, h, &n, 4);
}

void RdFreeImage(unsigned char *px)
{
	stbi_image_free(px);
}

bool RdWritePng(const char *path, int w, int h, const void *rgba)
{
	return stbi_write_png(path, w, h, 4, rgba, w * 4) != 0;
}

} // namespace rdoom
