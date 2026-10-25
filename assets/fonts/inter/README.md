# Inter 4.1 (vendored)

All on-screen text uses Inter, an open-source typeface in the spirit of modern TV interfaces (Apple's own
SF fonts are licensed only for Apple platforms, so they are not used). The TV never reads these files:
`make fonts` pre-renders them into 4-bit anti-aliased glyph tables in `ui/font_ui_data.c` (ASCII only,
with kerning from the font's GPOS table), and `make check-fonts` fails if that file no longer matches.

- Source: https://github.com/rsms/inter/releases/download/v4.1/Inter-4.1.zip
  (SHA-256 `9883fdd4a49d4fb66bd8177ba6625ef9a64aa45899767dde3d36aa425756b11e`). The release does not publish
  checksums and has a single official source, so this hash records what was used rather than verifying it.
- Files copied unmodified from `extras/ttf/` and the archive root:
  - `InterDisplay-Bold.ttf` `b74c8e0dd744b3347faca4c96bc7b2e32f7d6f62300a79b1d1a99331e44a5bc4` (46 px titles)
  - `InterDisplay-SemiBold.ttf` `0310d7a325896129730c6c8cf9a6e0f81ee258bedf77b1ff059b2a7b75f74e02` (30 px)
  - `Inter-SemiBold.ttf` `78a843fade9d4612a5567302fb595b56976eb5fcebf4fea5a5912d638bafcde3` (24 px)
  - `Inter-Medium.ttf` `97ad806f526e41546d46365bb3a393145f75b7b1568913db74549ad8b8dba872` (20 and 18 px)
  - `LICENSE.txt` `262481e844521b326f5ecd053e59b98c8b2da78c8ee1bdbb6e8174305e54935a`
- Licence: SIL Open Font License 1.1, no Reserved Font Name (see LICENSE.txt).
