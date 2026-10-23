# Spleen 2.1.0 bitmap fonts (vendored)

Used for all on-screen text (`ui/font_data.c` is generated from these files by `make fonts`;
`make check-fonts` fails if the generated file no longer matches).

- Source: https://github.com/fcambus/spleen/releases/download/2.1.0/spleen-2.1.0.tar.gz
  (SHA-256 `8b47c56f1a6eb858fbcf9e34530557404b02fbb3455e38e64fb84473fd0c372f`)
- Each file below was cross-checked byte-for-byte against the `2.1.0` git tag on GitHub:
  - `spleen-12x24.bdf` `eb690d1dd190e21ae31f800e63fe1188409e3d94a11da222196de0777ed8c9e8`
  - `spleen-16x32.bdf` `3b6ae73ef1dd3c78bad873697c18b15df82d8e5c0258114fda401556e01d0e67`
  - `spleen-32x64.bdf` `8a6fbd024d11f57ec973291959a19c020465ccbdc0836d938e31e27d68e47e4b`
  - `LICENSE` `3cb3f3f5a795547d329828df881c80d923bfc75f65065c44ffa703ad78c678bd`
- Licence: BSD-2-Clause, Copyright (c) 2018-2024 Frederic Cambus (see LICENSE). Only ASCII 32-126 is
  converted.
