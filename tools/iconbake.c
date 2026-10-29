/* Renders the Family TV app icon (the Live TV tile) and writes it as raw
 * RGBA to stdout, one block per size: a 4-byte little-endian size, then
 * size*size*4 bytes. Sizes come from the arguments (default: the Windows
 * ones). tools/mkico.py packs them into packaging/windows/familytv.ico and
 * tools/mkicns.py into an icon set for macOS (make icon).
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

#define MAX_SIZE 1024

int main(int argc, char **argv)
{
    static const int windows_sizes[] = {256, 48, 32, 24, 16};
    static uint32_t black[MAX_SIZE * MAX_SIZE], white[MAX_SIZE * MAX_SIZE];
    static unsigned char rgba[MAX_SIZE * MAX_SIZE * 4];
    int k, count = argc > 1 ? argc - 1 : (int)(sizeof windows_sizes / sizeof windows_sizes[0]);
    for (k = 0; k < count; k++) {
        int n = argc > 1 ? atoi(argv[k + 1]) : windows_sizes[k], i;
        if (n < 16 || n > MAX_SIZE)
            continue;
        ft_canvas cb, cw;
        unsigned char hdr[4];
        hdr[0] = (unsigned char)n;
        hdr[1] = (unsigned char)(n >> 8);
        hdr[2] = hdr[3] = 0;
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
