# Family TV

**Kenyan TV, live sport, YouTube, Netflix and games for the whole family, in one TV-style app for your laptop.**

[![Watch the 30-second tour](docs/promo/poster.png)](https://github.com/NaomiKwamboka0/family-tv/releases/latest/download/familytv-promo.mp4)

[▶ Watch the 30-second tour](https://github.com/NaomiKwamboka0/family-tv/releases/latest/download/familytv-promo.mp4)
(drawn by the app itself; colour bars stand in for the stations' live pictures)

Family TV turns a laptop into a TV. Move around with the arrow keys or the mouse, pick a channel, and
watch. Parents stay in charge with a PIN, a child profile and a daily play-time limit.

## Download

Get it from the [latest release](https://github.com/NaomiKwamboka0/family-tv/releases/latest). One file, no zip to unpack:

| Your computer | Download |
|---|---|
| **Windows 10 or 11** | [FamilyTV-Setup-1.0.0.exe](https://github.com/NaomiKwamboka0/family-tv/releases/latest/download/FamilyTV-Setup-1.0.0.exe) |
| **Mac with Apple chip** (M1 or newer), macOS 15 or newer | [FamilyTV-1.0.0-macos.dmg](https://github.com/NaomiKwamboka0/family-tv/releases/latest/download/FamilyTV-1.0.0-macos.dmg) |

**Install:** on Windows run the Setup file. On a Mac open the `.dmg` and drag Family TV to Applications.

**The first start asks once** because Family TV isn't signed by a company yet:
- **Windows:** "Windows protected your PC" → *More info* → *Run anyway*.
- **Mac:** "cannot check it for malicious software" → *Done*, then *System Settings → Privacy & Security → Open Anyway*.

Every release lists a checksum (`SHA256SUMS.txt`) if you want to check the file is untouched.

## What's inside

Five tabs along the top:

| Tab | What it does |
|---|---|
| **For you** | Jump back to your last channel, the channel row, Sports and games. |
| **Live TV** | A guide of Kenyan channels (Citizen, NTV, KTN, K24, KBC, TV47, Inooro and more) and world news (Al Jazeera, DW, France 24, Sky News), played from each station's own YouTube live stream. |
| **Sports** | Official sports channels that are live now or coming up, plus *Live football now*: live matches and watch-alongs found with YouTube's live search, filtered of junk. |
| **Streaming** | YouTube, Netflix and Google. |
| **Apps** | Three games (Family Racer, Penalty Cup, Block Garden) and the family settings. |

- **3D look and mouse:** cards are glossy 3D slabs that turn toward your mouse. The mouse works everywhere: hover to move, click to choose, right-click to go back, scroll to move through rows (or change volume while watching).
- **1080p by default**, at 60 frames a second where a channel offers it.
- **Parent controls:** a PIN-locked settings screen, a child profile with a daily play-time limit, and per-game blocking. YouTube, Netflix, Google and the unvetted football search are for the parent profile only.
- **English and Kiswahili.**

### Keys

| Key | At home | While watching |
|---|---|---|
| Arrows | move | Up/Down: channel. Left/Right: volume |
| Enter | choose | open the channel list |
| Backspace / Esc | back to the tabs | close the list, then go home |
| F or F11 | full screen | full screen |
| Q | quit | quit |

### Good to know

- **Live channels need internet.** A channel takes about 10–15 seconds to start. Some stations stream only at certain times, and Family TV then says the channel isn't live right now. The games work offline.
- **Data use** (Parent settings → *Live TV picture*): 1080p about 2.6–3 GB an hour, 720p about 1.4 GB, 480p about 0.6 GB, 360p about 0.5 GB. Nothing streams while you're on the home screen.
- **YouTube, Netflix and Google** show the real websites. On a Mac they open inside the Family TV window under a *← Family TV* bar. On Windows they open in their own full-screen window for now. Netflix may refuse to play inside a Mac web view because of its copy protection; if so, tell us.
- **Change the channels:** edit `channels.txt` (one channel per line: `number|name|https://www.youtube.com/channel/<ID>/live|#colour`). Use the channel-ID address, not the `@name` one: look-alike names belong to other channels. `tools/verify-channels.sh` checks addresses.
- **Where your settings live:** `%APPDATA%\FamilyTV\FamilyTV` on Windows, `~/Library/Application Support/FamilyTV/FamilyTV` on a Mac.

## Your data, and who owns what

- **Privacy:** Family TV has no accounts, no analytics and no server of its own. Everything it remembers stays on your computer, and nothing is sent to the author. See [PRIVACY.md](PRIVACY.md).
- **Ownership:** Copyright © 2026 Naomi Kwamboka Onyancha. Free software under the [GPL-3.0](LICENSE). See [OWNERSHIP.md](OWNERSHIP.md) for what you may do with it, and for the trademark notice: Family TV isn't affiliated with Google, YouTube, Netflix or any TV station or league.

## Screenshots

[More screens](docs/screens/README.md) · [What's tested and what isn't](docs/STATUS.md)

![For you](docs/screens/01-for-you.png)

## For developers

Family TV is a small C program that draws every pixel itself (no game engine, no browser). It uses SDL2 for the window, libmpv with yt-dlp and Deno for the live streams, and the system web view for YouTube, Netflix and Google.

```sh
brew install sdl2 mpv pkgconf       # Mac: mpv brings libmpv, yt-dlp and deno
make            # 700+ unit checks (with sanitizers), scripted walks, headless window check
make run        # Family TV in a window
make screens    # redraw docs/screens/*.png
make promo      # the 30-second advert (needs ffmpeg)
make bench      # milliseconds per frame per screen
```

**Build the installers** (on a Mac):

```sh
brew install mingw-w64 sevenzip makensis dylibbundler ffmpeg
make win-deps mac-deps   # pinned downloads, SHA-256 checked
make release             # out/release: Setup.exe, .dmg and SHA256SUMS.txt
```

Pushing a tag such as `v1.0.0` (matching `VERSION` in the Makefile) makes GitHub build everything, install and start each installer on a clean Windows and Mac machine, and publish the release with the video ([release.yml](.github/workflows/release.yml)).

Native Windows build ([MSYS2](https://www.msys2.org), UCRT64 shell): `pacman -S make python mingw-w64-ucrt-x86_64-{gcc,pkgconf,SDL2,mpv}`, then `make test window`.

Handy switches: `--player stub` (a test card instead of streams), `--stats` (frame cost), `--verbose`, `--data DIR`, `--fullscreen`, and `--keys OK,W60,...` (scripted keys; add `--player mpv` and `SDL_VIDEODRIVER=dummy` to try real streams headlessly).

| Folder | What |
|---|---|
| `ui/` | software canvas, fonts, 3D artwork, the home state machine, live TV, views |
| `media/` | player interface, libmpv player, stub player, error classifier |
| `services/` | settings and PIN, play time, channels, sports search, translations, session |
| `games/` | Family Racer, Penalty Cup, Block Garden, checksummed saves |
| `sim/` | the program: main loop, window, web views, screenshot, bench and advert tools |
| `platform/` | randomness, clocks and files on macOS and Windows |
| `packaging/` | installers, icons, notices and release notes |
| `tools/` | font and icon bakers, channel checker, dependency fetcher |

### Contributing

Pull requests are welcome. You keep the copyright in what you write, and it's released under the same GPL-3.0 licence as the rest ([OWNERSHIP.md](OWNERSHIP.md)). Run `make` before you send one.

Found a bug, or something that doesn't play? [Open an issue](https://github.com/NaomiKwamboka0/family-tv/issues) and say what you pressed. The log is `familytv.log` in the data folder.

The earlier version for the family's LG TV is at git tag `lg-tv-final`.
