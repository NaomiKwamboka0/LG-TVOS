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
- Channels take 10-15 s to start: yt-dlp needs about 4 s and opening the separate sound and picture
  streams the rest. Looking up neighbouring channels ahead of time could shorten zapping; not done.
- The Windows zip is about 110 MB (libmpv, deno and yt-dlp). `FamilyTV.exe` is not code-signed.
- The Kiswahili text is a first draft for the family to correct.
- libmpv is GPL: the zip is for the family's own use (see `LICENSES/NOTICE.txt` in it).
