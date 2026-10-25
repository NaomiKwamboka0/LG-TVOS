# stb_truetype v1.26 (vendored, computer-side only)

Used only by `tools/fontbake.c`, which runs on the development computer to pre-render fonts. It is not
compiled into the TV build. `tools/fontbake.c` adds its own reader for GPOS pair kerning inside Extension
lookups, which stb_truetype does not follow.

- Source: https://raw.githubusercontent.com/nothings/stb/003501f6d7a8668ce8b6d875e34dd97ad0d5d5b5/stb_truetype.h
- SHA-256: `ecd30b05e0dd4fea3a13c26810dd9e1992dc379049482c393d5a19e6b5090aab`
- Licence: public domain (Unlicense) or MIT, at your choice (see the end of the file).
