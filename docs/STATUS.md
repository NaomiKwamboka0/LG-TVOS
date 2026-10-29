# Status

Family TV moved from the family's LG TV (now broken) to a laptop app on 2026-10-28. This page says
what has been checked, where, and what has not.

## Checked on the development Mac (Apple Silicon, macOS 15)

- `make`: 601 unit checks under AddressSanitizer and UndefinedBehaviorSanitizer, the font check,
  two scripted terminal walks (watch a channel and zap, play a game, create a PIN, switch language)
  and two headless window walks.
- Live channels, real streams (2026-10-28, about 23:00 EAT, yt-dlp 2026.10.26, libmpv 0.41):
  - Citizen TV plays at 480p with hardware decoding (VideoToolbox), about 12 s from OK to picture;
    zapping to NTV works.
  - Drawing a frame costs about 2.3 ms for the picture and 0.6 ms for the overlays (`--stats`).
  - Error cards appear for each failure seen in practice: a channel that is not live (KBC at that
    hour), a wrong address, yt-dlp missing, and no internet (simulated with a dead proxy).
- Channel line-up checked with `tools/verify-channels.sh`: all ten addresses are the stations' own
  channel IDs. Live at the time: Citizen, NTV, KTN News, K24, TV47, Inooro. KBC, Kameme, Ramogi and
  Switch were not live then and stream on a schedule.
- Settings written before live TV (schema 1) load with PIN and child profile intact (unit test).

## The Mac app (checked 2026-10-30 on the development Mac)

- `make mac-dmg` makes `FamilyTV-1.0.0-macos.dmg` (about 106 MB): 52 libraries copied out of
  Homebrew into the app, signed ad hoc, nothing built for newer than macOS 15.
- Started from inside the mounted .dmg; `DYLD_PRINT_LIBRARIES` shows nothing loaded from Homebrew,
  SDL 3 included (sdl2-compat opens it at run time).
- Citizen TV played live through the app's own libmpv, yt-dlp and deno with Homebrew off `PATH`.
- Not yet tried on a Mac that never had Homebrew, nor through Gatekeeper's first-start prompt after
  a real download. The release workflow's clean-machine test covers the first.

## Sports tab, YouTube, mouse and 3D look (checked 2026-10-30 on the development Mac)

- Unit tests cover the Sports tab rows, the live football search filter (with real titles seen
  that night: NFL, US school games, "Video Game Simulation" streams, talk shows), the YouTube rows,
  search keyboard and typing, mouse hit-testing and focus, and the 3D card turned at the screen edge.
- Real searches: YouTube rows filled (Football, Kenya news) and a Kenya news video played inside
  Family TV through yt-dlp and libmpv. A yt-dlp search takes 20-40 s here, so the rows show
  "Loading..." first.
- 3D cards cost about 10 ms a frame at 720p on this Mac (8 ms before); a Retina screen draws four
  times the pixels.
- Not checked yet: Netflix's app window on Windows (Edge), the mouse on a Windows laptop.

## Release workflow (not run yet)

`.github/workflows/release.yml` builds the Windows installer, zip, Mac .dmg and advert on a macOS 15
runner, then installs the Setup.exe silently on a clean Windows machine, starts it with scripted
keys (SDL dummy video driver), uninstalls it, and starts the Mac app from the .dmg on a clean Mac.
The first run will be the first time Family TV runs on Windows at all.

## Built but not yet run on Windows

`make win-zip` cross-compiles a Windows 10/11 program and the import table shows only Windows' own
DLLs plus SDL2 and libmpv. Nothing has run on Windows yet: Wine is no longer available on this Mac
(Homebrew disabled it on 2026-09-01). These Windows-only fixes are covered by unit tests that pass
on the Mac but still need a run on Windows (`out/win/test-core.exe`, or `make test` in MSYS2):

- saves use binary mode and replace files atomically (`MoveFileEx`); before, every save would have
  read back as damaged and the third save would have failed
- the PIN salt comes from `BCryptGenRandom` (before: no randomness, so no PIN could be created)
- glow maths in 64-bit (Windows `long` is 32-bit: glows would have drawn garbage)

On the laptop, check:

- [ ] the zip unzips to `Documents\FamilyTV`; `FamilyTV.exe` starts after SmartScreen's
      "More info" > "Run anyway"
- [ ] home screen glows look like `docs/screens/01-for-you.png`
- [ ] F11 full screen; the window fits the screen
- [ ] Citizen TV plays within about 15 s; the banner, channel list (Enter) and volume work; no
      black console window flashes up while a channel starts
- [ ] Parent settings: create a PIN, switch to the child profile, change three settings, restart:
      everything is kept
- [ ] a game plays; play time counts down for the child
- [ ] with Wi-Fi off, a channel shows "No internet connection"
- [ ] Task Manager: Family TV's CPU use while watching at 480p and 720p (`--stats` shows the cost
      per frame; send both numbers if it stutters)

If anything fails, send `%APPDATA%\FamilyTV\FamilyTV\familytv.log`.

## Known limits

- YouTube changes break yt-dlp from time to time; then every channel says it could not play.
  Updating yt-dlp (`yt-dlp.exe -U` in the Family TV folder) fixes it.
- YouTube may ask for a "not a bot" check on shared mobile connections. Family TV says so; waiting
  or another network helps. Signing in is not supported.
- "Live football now" only finds what YouTube's live search shows: often watch-alongs and live
  scores rather than the match pictures, which belong to the broadcasters. Unofficial re-streams are
  cut by YouTube, sometimes mid-match.
- YouTube videos play without a progress bar or seeking yet; Up/Down go to the next video in the row.
- Channels take 10-15 s to start: yt-dlp needs about 4 s and opening the separate sound and picture
  streams the rest. Looking up neighbouring channels ahead of time could shorten zapping; not done.
- The Windows zip is about 110 MB and the installer about 90 MB (libmpv, deno and yt-dlp).
  `FamilyTV.exe` and the installer are not code-signed (SmartScreen asks once).
- The Kiswahili text is a first draft for the family to correct.
- libmpv is GPL, so Family TV is shared under GPL-3.0 with its source on GitHub; the installers carry
  the licence notices (`LICENSES/NOTICE.txt`).
- The Mac app needs Apple silicon and macOS 15 or newer (the Homebrew libraries inside it); it is
  signed ad hoc, not notarised, so the first start goes through Privacy & Security > Open Anyway.
