// RDoom launcher - TrueType text (stb_truetype) with a glyph cache.
#pragma once

#include <string>
#include "canvas.h"

namespace rdoom {

enum class Font { Display, Body, Bold };  // Bebas Neue, Poppins Medium, Poppins Bold

void TextInit();
void TextShutdown();
int TextWidth(Font f, int px, const std::string &utf8);
int TextLineHeight(Font f, int px);
// y is the top of the line box. maxWidth > 0 truncates with an ellipsis.
int DrawText(Canvas &c, Font f, int px, int x, int y, uint32_t color, const std::string &utf8, int maxWidth = 0);
// Word-wrapped paragraph; returns the height used. maxLines > 0 limits it.
int DrawTextWrapped(Canvas &c, Font f, int px, int x, int y, int width, uint32_t color, const std::string &utf8,
                    int maxLines = 0, int lineGap = 2);
int DrawTextCentered(Canvas &c, Font f, int px, int cx, int y, uint32_t color, const std::string &utf8);
int DrawTextRight(Canvas &c, Font f, int px, int rx, int y, uint32_t color, const std::string &utf8);
std::string Ellipsize(Font f, int px, const std::string &utf8, int maxWidth);

} // namespace rdoom
