/* Bakes TrueType fonts into 4-bit anti-aliased glyph tables for ui/font.h.
 * Runs on the development computer only (uses stb_truetype); the TV never
 * parses font files. Output is deterministic for a given toolchain.
 *
 * Usage: fontbake name=path.ttf:pixels [name=path.ttf:pixels ...] > ui/font_ui_data.c
 */
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FIRST 32
#define LAST 126
#define COUNT (LAST - FIRST + 1)

static unsigned char *load(const char *path, long *size)
{
    FILE *f = fopen(path, "rb");
    unsigned char *buf;
    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    *size = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = malloc((size_t)*size);
    if (buf && fread(buf, 1, (size_t)*size, f) != (size_t)*size) {
        free(buf);
        buf = NULL;
    }
    fclose(f);
    return buf;
}

/* ---- GPOS pair kerning ---------------------------------------------------
 * stb_truetype reads only plain PairPos lookups; Inter keeps most of its
 * kerning inside Extension lookups. This reads the 'kern' feature's PairPos
 * lookups (direct or via Extension), formats 1 and 2, X advance only. */

static unsigned u16(const unsigned char *p) { return (unsigned)(p[0] << 8 | p[1]); }
static unsigned long u32(const unsigned char *p) { return (unsigned long)u16(p) << 16 | u16(p + 2); }

static const unsigned char *gpos;
static int kern_lookups[64], nkern_lookups;

static int coverage_index(const unsigned char *cov, int glyph)
{
    unsigned i, n = u16(cov + 2);
    if (u16(cov) == 1) {
        for (i = 0; i < n; i++)
            if ((int)u16(cov + 4 + 2 * i) == glyph)
                return (int)i;
    } else if (u16(cov) == 2) {
        for (i = 0; i < n; i++) {
            const unsigned char *r = cov + 4 + 6 * i;
            if (glyph >= (int)u16(r) && glyph <= (int)u16(r + 2))
                return (int)(u16(r + 4) + (unsigned)glyph - u16(r));
        }
    }
    return -1;
}

static int class_of(const unsigned char *cd, int glyph)
{
    unsigned i;
    if (u16(cd) == 1) {
        unsigned start = u16(cd + 2), n = u16(cd + 4);
        if (glyph >= (int)start && glyph < (int)(start + n))
            return (int)u16(cd + 6 + 2 * ((unsigned)glyph - start));
    } else if (u16(cd) == 2) {
        unsigned n = u16(cd + 2);
        for (i = 0; i < n; i++) {
            const unsigned char *r = cd + 4 + 6 * i;
            if (glyph >= (int)u16(r) && glyph <= (int)u16(r + 2))
                return (int)u16(r + 4);
        }
    }
    return 0;
}

static int value_size(unsigned fmt)
{
    int n = 0;
    for (; fmt; fmt >>= 1)
        n += (int)(fmt & 1u);
    return 2 * n;
}

static int x_advance(const unsigned char *v, unsigned fmt)
{
    int off = ((fmt & 1u) ? 2 : 0) + ((fmt & 2u) ? 2 : 0);
    return (fmt & 4u) ? (short)u16(v + off) : 0;
}

/* Returns 1 if the subtable applies to the pair (and stores the adjustment). */
static int pair_subtable(const unsigned char *st, int g1, int g2, int *adjust)
{
    int idx = coverage_index(st + u16(st + 2), g1);
    unsigned vf1 = u16(st + 4), vf2 = u16(st + 6);
    int s1 = value_size(vf1), s2 = value_size(vf2);
    *adjust = 0;
    if (idx < 0)
        return 0;
    if (u16(st) == 1) {
        const unsigned char *set = st + u16(st + 10 + 2 * idx);
        unsigned i, n = u16(set);
        for (i = 0; i < n; i++) {
            const unsigned char *rec = set + 2 + i * (unsigned)(2 + s1 + s2);
            if ((int)u16(rec) == g2) {
                *adjust = x_advance(rec + 2, vf1);
                return 1;
            }
        }
        return 0; /* pair not listed: the next subtable may apply */
    }
    if (u16(st) == 2) {
        int c1 = class_of(st + u16(st + 8), g1), c2 = class_of(st + u16(st + 10), g2);
        unsigned n2 = u16(st + 14);
        if (c1 < (int)u16(st + 12) && c2 < (int)n2)
            *adjust = x_advance(st + 16 + ((unsigned)c1 * n2 + (unsigned)c2) * (unsigned)(s1 + s2), vf1);
        return 1;
    }
    return 0;
}

static int gpos_kern(int g1, int g2)
{
    const unsigned char *list = gpos + u16(gpos + 8);
    int total = 0, i;
    for (i = 0; i < nkern_lookups; i++) {
        const unsigned char *lk = list + u16(list + 2 + 2 * kern_lookups[i]);
        unsigned type = u16(lk), n = u16(lk + 4), j;
        for (j = 0; j < n; j++) {
            const unsigned char *st = lk + u16(lk + 6 + 2 * j);
            int adjust;
            if (type == 9) {
                if (u16(st + 2) != 2)
                    continue;
                st = st + u32(st + 4);
            } else if (type != 2) {
                continue;
            }
            if (pair_subtable(st, g1, g2, &adjust)) {
                total += adjust;
                break;
            }
        }
    }
    return total;
}

static void find_kern_lookups(const unsigned char *ttf)
{
    unsigned n = u16(ttf + 4), i, f, k;
    gpos = NULL;
    nkern_lookups = 0;
    for (i = 0; i < n; i++)
        if (memcmp(ttf + 12 + 16 * i, "GPOS", 4) == 0)
            gpos = ttf + u32(ttf + 12 + 16 * i + 8);
    if (!gpos)
        return;
    {
        const unsigned char *features = gpos + u16(gpos + 6);
        unsigned nf = u16(features);
        for (f = 0; f < nf; f++) {
            const unsigned char *rec = features + 2 + 6 * f;
            const unsigned char *feature = features + u16(rec + 4);
            if (memcmp(rec, "kern", 4) != 0)
                continue;
            for (k = 0; k < u16(feature + 2); k++) {
                int index = (int)u16(feature + 4 + 2 * k), dup = 0, m;
                for (m = 0; m < nkern_lookups; m++)
                    dup |= kern_lookups[m] == index;
                if (!dup && nkern_lookups < 64)
                    kern_lookups[nkern_lookups++] = index;
            }
        }
    }
}

static long round_q6(float v)
{
    return (long)(v * 64.0f + (v >= 0 ? 0.5f : -0.5f));
}

static int bake(const char *name, const char *path, int pixels)
{
    stbtt_fontinfo font;
    long size, total = 0, nkern = 0;
    unsigned char *ttf = load(path, &size);
    int ascent, descent, gap, c, a, b, column = 0;
    float scale;

    if (!ttf || !stbtt_InitFont(&font, ttf, stbtt_GetFontOffsetForIndex(ttf, 0))) {
        fprintf(stderr, "fontbake: cannot read %s\n", path);
        free(ttf);
        return -1;
    }
    scale = stbtt_ScaleForMappingEmToPixels(&font, (float)pixels);
    find_kern_lookups(ttf);
    stbtt_GetFontVMetrics(&font, &ascent, &descent, &gap);

    printf("/* %s: %s at %d px per em */\n", name, strrchr(path, '/') ? strrchr(path, '/') + 1 : path, pixels);
    printf("static const uint8_t %s_alpha[] = {\n", name);
    {
        long offsets[COUNT];
        int x0s[COUNT], y0s[COUNT], ws[COUNT], hs[COUNT], adv[COUNT];
        for (c = FIRST; c <= LAST; c++) {
            int glyph = stbtt_FindGlyphIndex(&font, c), w, h, xo, yo, advance, lsb, row, col;
            unsigned char *bm = stbtt_GetGlyphBitmap(&font, scale, scale, glyph, &w, &h, &xo, &yo);
            stbtt_GetGlyphHMetrics(&font, glyph, &advance, &lsb);
            if (w > 255 || h > 255) {
                fprintf(stderr, "fontbake: glyph %d too large\n", c);
                return -1;
            }
            offsets[c - FIRST] = total;
            x0s[c - FIRST] = xo;
            y0s[c - FIRST] = yo;
            ws[c - FIRST] = bm ? w : 0;
            hs[c - FIRST] = bm ? h : 0;
            adv[c - FIRST] = (int)round_q6((float)advance * scale);
            if (bm) {
                for (row = 0; row < h; row++)
                    for (col = 0; col < w; col += 2) {
                        int hi = (bm[row * w + col] * 15 + 127) / 255;
                        int lo = col + 1 < w ? (bm[row * w + col + 1] * 15 + 127) / 255 : 0;
                        printf("%s0x%02x,", column == 0 ? "    " : "", (hi << 4) | lo);
                        if (++column == 24) {
                            printf("\n");
                            column = 0;
                        }
                        total++;
                    }
                stbtt_FreeBitmap(bm, NULL);
            }
        }
        if (total == 0)
            printf("    0x00,");
        printf("%s};\n", column ? "\n" : "");

        printf("static const ft_glyph %s_glyphs[%d] = {\n", name, COUNT);
        for (c = 0; c < COUNT; c++)
            printf("    {%d, %d, %d, %d, %d, %ld}, /* %d */\n", x0s[c], y0s[c], ws[c], hs[c], adv[c], offsets[c],
                   c + FIRST);
        printf("};\n");
    }

    printf("static const ft_kern %s_kerns[] = {\n", name);
    for (a = FIRST; a <= LAST; a++)
        for (b = FIRST; b <= LAST; b++) {
            int k = gpos && nkern_lookups ? gpos_kern(stbtt_FindGlyphIndex(&font, a), stbtt_FindGlyphIndex(&font, b))
                                          : stbtt_GetCodepointKernAdvance(&font, a, b);
            long q = round_q6((float)k * scale);
            if (q != 0) {
                printf("    {0x%04x, %ld},\n", (a << 8) | b, q);
                nkern++;
            }
        }
    if (nkern == 0)
        printf("    {0, 0},\n");
    printf("};\n");
    printf("const ft_font %s = {\"%s\", %d, %d, %d, %d, %d, %s_glyphs, %s_alpha, %s_kerns, %ld};\n\n", name, name,
           pixels, (int)(ascent * scale + 0.999f), (int)((ascent - descent + gap) * scale + 0.5f), FIRST, COUNT,
           name, name, name, nkern);
    fprintf(stderr, "fontbake: %s %d px: %ld alpha bytes, %ld kerning pairs\n", name, pixels, total, nkern);
    free(ttf);
    return 0;
}

int main(int argc, char **argv)
{
    int i;
    if (argc < 2) {
        fprintf(stderr, "usage: fontbake name=path.ttf:pixels ...\n");
        return 2;
    }
    printf("/* Generated by tools/fontbake.c from Inter 4.1 (SIL OFL 1.1, see\n"
           " * assets/fonts/inter/LICENSE.txt). Do not edit by hand: run make fonts. */\n"
           "#include \"font.h\"\n\n");
    for (i = 1; i < argc; i++) {
        char spec[512], *eq, *colon;
        snprintf(spec, sizeof spec, "%s", argv[i]);
        eq = strchr(spec, '=');
        colon = strrchr(spec, ':');
        if (!eq || !colon || colon < eq) {
            fprintf(stderr, "fontbake: bad spec %s\n", argv[i]);
            return 2;
        }
        *eq = '\0';
        *colon = '\0';
        if (bake(spec, eq + 1, atoi(colon + 1)) != 0)
            return 1;
    }
    return 0;
}
