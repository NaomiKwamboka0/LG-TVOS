# Family TV

A TV-style home for a laptop: Kenyan TV channels from their YouTube live streams, three family
games, and parent controls (PIN, children's play time). The look follows TCL's Google TV launcher:
tabs along the top, a wide banner, rows of cards, a channel guide, and while watching an info banner,
a channel list and volume. Runs on Windows 10/11 (the family laptop) and macOS (development).

![For you](docs/screens/01-for-you.png)

More screens: [docs/screens](docs/screens/README.md). What is tested and what is not:
[docs/STATUS.md](docs/STATUS.md).

## Using it

| Key | Home | Watching a channel |
|---|---|---|
| Arrows | move | Up/Down: channel, Left/Right: volume |
| Enter | choose | channel list (on an error: try again) |
| Backspace / Esc | up to the tabs | close the list, else back home (the stream stops) |
| F11 or F | full screen | full screen |
| Q | quit | quit |

- **Channels need internet.** Each one plays the station's YouTube live stream, through
  [yt-dlp](https://github.com/yt-dlp/yt-dlp) (with [Deno](https://deno.com)) and
  [libmpv](https://mpv.io). A channel takes about 10-15 s to start. Some stations stream only at
  certain times; Family TV then says the channel is not live right now.
- **Data use** is set in Parent settings, "Live TV picture": 360p about 0.5 GB an hour, 480p about
  0.6 GB (the default), 720p about 1.4 GB. Nothing streams while the home screen is showing.
- **Channel list:** `channels.txt` (next to the program, or in the data folder to override it), one
  channel per line: `number|name|https://www.youtube.com/channel/<ID>/live|#colour`. Use the channel
  ID address, not the `@name` one: look-alike names belong to other channels.
  `tools/verify-channels.sh` checks addresses and prints channel IDs.
- **Parent settings** (gear, top right): language (English/Kiswahili), children's daily play time,
  which games children may play, live TV picture quality, parent PIN.
- **Data folder** (settings, saves, `familytv.log`): `%APPDATA%\FamilyTV\FamilyTV` on Windows,
  `~/Library/Application Support/FamilyTV/FamilyTV` on a Mac.

## Building

On the Mac (development):

```sh
brew install sdl2 mpv pkgconf   # mpv brings libmpv, yt-dlp and deno
make            # tests (sanitizers), font check, scripted walks, headless window check
make run        # Family TV in a window
make screens    # draw every screen to docs/screens/*.png
make bench      # milliseconds per frame per screen
```

Windows zip, cross-built on the Mac:

```sh
brew install mingw-w64 sevenzip
make win-deps   # SDL2, libmpv, yt-dlp, deno: pinned versions, SHA-256 checked (tools/win-deps.lock)
make win-zip    # out/win/FamilyTV-<commit>.zip: unzip on the laptop, run FamilyTV.exe
make win-test   # out/win/test-core.exe: the unit tests as a Windows program
```

Native Windows build ([MSYS2](https://www.msys2.org), UCRT64 shell), for checking on the laptop
itself:

```sh
pacman -S make python mingw-w64-ucrt-x86_64-{gcc,pkgconf,SDL2,mpv}
make test window   # unit tests (no sanitizers on Windows) and out/host/familytv.exe
```

Handy switches: `--player stub` (test card instead of streams), `--stats` (frame cost overlay while
watching), `--verbose` (player messages on the terminal), `--data DIR`, `--fullscreen`,
`--keys OK,W60,...` (scripted keys; add `--player mpv` to try real streams headlessly with
`SDL_VIDEODRIVER=dummy`). `FAMILYTV_MPV="hwdec=no;..."` passes extra mpv options.

## Layout

| Folder | What |
|---|---|
| `ui/` | software canvas, fonts, artwork, the home state machine (`shell`), live TV (`live`), views |
| `media/` | player interface, libmpv player, stub player, error classifier |
| `services/` | settings (with PIN), play time, channels, viewer prefs, translations, session |
| `games/` | Family Racer, Penalty Cup, Block Garden, checksummed saves |
| `sim/` | the program: main loop, SDL window, terminal frontend, screenshot and bench tools |
| `platform/` | randomness, clocks and files on macOS and Windows |
| `packaging/windows/` | manifest, icon, README and licence notices for the zip |
| `tools/` | font and icon bakers, channel checker, Windows dependency fetcher |

The LG TV version (hardware evidence, probes, gate checks) is at git tag `lg-tv-final`.
