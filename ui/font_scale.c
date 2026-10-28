/* Sharper fonts for big screens: the five UI fonts rasterised from the
 * Inter TTFs at 1.5x, 2x or 3x, on first use, with the same 4-bit packing
 * as tools/fontbake.c. Kerning and advances stay those of the 1x fonts
 * (the canvas lays text out at 1x), so only glyph pictures are made here. */
#include "font.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "canvas.h"

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wcast-qual"
#endif
#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_STATIC
#include "stb_truetype.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

static const struct {
    const ft_font *font;
    const char *file;
} SOURCES[] = {
    {&font_display, "InterDisplay-Bold.ttf"},
    {&font_title, "InterDisplay-SemiBold.ttf"},
    {&font_label, "Inter-SemiBold.ttf"},
    {&font_body, "Inter-Medium.ttf"},
    {&font_caption, "Inter-Medium.ttf"},
};
#define NFONTS (sizeof SOURCES / sizeof SOURCES[0])
#define NSCALES 3 /* 1.5x, 2x, 3x */

static unsigned char *ttf[NFONTS];
static stbtt_fontinfo info[NFONTS];
static int ready;
static ft_font made[NFONTS][NSCALES];
static int tried[NFONTS][NSCALES];

static unsigned char *load(const char *path)
{
    FILE *f = fopen(path, "rb");
    unsigned char *buf = NULL;
    long size;
    if (!f)
        return NULL;
    if (fseek(f, 0, SEEK_END) == 0 && (size = ftell(f)) > 0 && fseek(f, 0, SEEK_SET) == 0) {
        buf = (unsigned char *)malloc((size_t)size);
        if (buf && fread(buf, 1, (size_t)size, f) != (size_t)size) {
            free(buf);
            buf = NULL;
        }
    }
    fclose(f);
    return buf;
}

int font_scale_init(const char *dir)
{
    size_t i, j;
    char path[600];
    ready = 0;
    for (i = 0; i < NFONTS; i++) {
        for (j = 0; j < i && strcmp(SOURCES[j].file, SOURCES[i].file) != 0; j++)
            ;
        if (j < i) { /* same file as an earlier font */
            ttf[i] = ttf[j];
            info[i] = info[j];
            continue;
        }
        snprintf(path, sizeof path, "%s%s", dir, SOURCES[i].file);
        ttf[i] = load(path);
        if (!ttf[i] || !stbtt_InitFont(&info[i], ttf[i], stbtt_GetFontOffsetForIndex(ttf[i], 0)))
            return -1;
    }
    ready = 1;
    return 0;
}

int font_scale_ready(void)
{
    return ready;
}

static int scale_slot(int scale)
{
    return scale == CANVAS_1_5X ? 0 : scale == CANVAS_2X ? 1 : scale == CANVAS_3X ? 2 : -1;
}

/* Returns 0 if the font could not be made (then 1x is used). */
static int make(size_t i, int slot, int scale)
{
    const ft_font *base = SOURCES[i].font;
    stbtt_fontinfo *fi = &info[i];
    float px = stbtt_ScaleForMappingEmToPixels(fi, (float)base->size * (float)scale / 2.0f);
    ft_glyph *glyphs = (ft_glyph *)calloc((size_t)base->count, sizeof *glyphs);
    unsigned char *bm[256];
    size_t total = 0;
    uint8_t *alpha;
    int c, ascent, descent, gap, w, h, xo, yo;

    if (!glyphs)
        return 0;
    for (c = 0; c < base->count; c++) {
        int glyph = stbtt_FindGlyphIndex(fi, base->first + c);
        bm[c] = stbtt_GetGlyphBitmap(fi, px, px, glyph, &w, &h, &xo, &yo);
        if (bm[c] && (w > 255 || h > 255)) {
            stbtt_FreeBitmap(bm[c], NULL);
            bm[c] = NULL;
        }
        glyphs[c].x0 = (int16_t)xo;
        glyphs[c].y0 = (int16_t)yo;
        glyphs[c].w = (uint8_t)(bm[c] ? w : 0);
        glyphs[c].h = (uint8_t)(bm[c] ? h : 0);
        glyphs[c].offset = (uint32_t)total;
        total += (size_t)((glyphs[c].w + 1) / 2) * glyphs[c].h;
    }
    alpha = (uint8_t *)calloc(total ? total : 1, 1);
    for (c = 0; c < base->count; c++) {
        int row, col;
        uint8_t *out = alpha ? alpha + glyphs[c].offset : NULL;
        for (row = 0; out && row < glyphs[c].h; row++)
            for (col = 0; col < glyphs[c].w; col += 2) {
                const unsigned char *src = bm[c] + row * glyphs[c].w;
                int hi = (src[col] * 15 + 127) / 255;
                int lo = col + 1 < glyphs[c].w ? (src[col + 1] * 15 + 127) / 255 : 0;
                *out++ = (uint8_t)((hi << 4) | lo);
            }
        if (bm[c])
            stbtt_FreeBitmap(bm[c], NULL);
    }
    if (!alpha) {
        free(glyphs);
        return 0;
    }
    stbtt_GetFontVMetrics(fi, &ascent, &descent, &gap);
    made[i][slot] = *base;
    made[i][slot].size = base->size * scale / 2;
    made[i][slot].ascent = (int)((float)ascent * px + 0.999f);
    made[i][slot].line_height = (int)((float)(ascent - descent + gap) * px + 0.5f);
    made[i][slot].glyphs = glyphs;
    made[i][slot].alpha = alpha;
    return 1;
}

const ft_font *font_scaled(const ft_font *f, int scale)
{
    int slot = scale_slot(scale);
    size_t i;
    if (!ready || slot < 0)
        return f;
    for (i = 0; i < NFONTS; i++)
        if (SOURCES[i].font == f) {
            if (!tried[i][slot])
                tried[i][slot] = 1 + make(i, slot, scale);
            return tried[i][slot] == 2 ? &made[i][slot] : f;
        }
    return f;
}
