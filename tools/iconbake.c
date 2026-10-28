/* Renders the Family TV app icon (the Live TV tile) at the usual Windows
 * icon sizes and writes them as raw RGBA to stdout, one block per size:
 * a 4-byte little-endian size, then size*size*4 bytes. tools/mkico.py packs
 * them into packaging/windows/familytv.ico (make icon).
 *
 * The canvas always writes opaque pixels, so each size is drawn twice, on
 * black and on white; the difference gives the coverage (alpha). */
#include <stdio.h>
#include <stdlib.h>

#include "art.h"
#include "canvas.h"

static void draw(ft_canvas *c, int size, ft_color bg)
{
    int pad = size / 16;
    ft_rect r = {pad, pad, size - 2 * pad, size - 2 * pad};
    canvas_clear(c, bg);
    art_tile(c, r, size / 5, ART_LIVE_TV, 1, NULL);
}

int main(void)
{
    static const int sizes[] = {256, 48, 32, 24, 16};
    static uint32_t black[256 * 256], white[256 * 256];
    static unsigned char rgba[256 * 256 * 4];
    size_t k;
    for (k = 0; k < sizeof sizes / sizeof sizes[0]; k++) {
        int n = sizes[k], i;
        ft_canvas cb, cw;
        unsigned char hdr[4] = {(unsigned char)n, (unsigned char)(n >> 8), 0, 0};
        canvas_init(&cb, black, n, n, n);
        canvas_init(&cw, white, n, n, n);
        draw(&cb, n, 0xFF000000u);
        draw(&cw, n, 0xFFFFFFFFu);
        for (i = 0; i < n * n; i++) {
            int a = 255 - (int)(((white[i] >> 8) & 0xFF) - ((black[i] >> 8) & 0xFF)), ch;
            for (ch = 0; ch < 3; ch++) {
                int v = (int)((black[i] >> (16 - 8 * ch)) & 0xFF);
                rgba[4 * i + ch] = (unsigned char)(a > 0 ? (v * 255 + a / 2) / a > 255 ? 255 : (v * 255 + a / 2) / a : 0);
            }
            rgba[4 * i + 3] = (unsigned char)(a < 0 ? 0 : a > 255 ? 255 : a);
        }
        fwrite(hdr, 1, 4, stdout);
        fwrite(rgba, 1, (size_t)n * n * 4, stdout);
    }
    return 0;
}
