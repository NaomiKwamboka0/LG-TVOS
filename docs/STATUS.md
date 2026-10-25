# Status against plan v2.0

Source: `LG32LB580V_Standalone_OS_Architecture (1).pdf` (v2.0, 23 Oct 2026).

## Installation gates (p.41)

| Gate | Status | Notes |
|------|--------|-------|
| I01 Identity | BLOCKED | Model/firmware observed; rear-label vs info-screen serial mismatch unresolved |
| I02 Access | NOT RUN | No legitimate code-execution route established on this TV |
| I03 ABI | NOT RUN | Needs vendor ELF headers + runtime report |
| I04 Memory/storage | NOT RUN | |
| I05 Display/input | NOT RUN | |
| I06 TV/audio/power | NOT RUN | |
| I07 Recovery | NOT RUN | No recovery path verified; persistent install BLOCKED |
| I08 Temporary trial | NOT RUN | |
| I09 Persistence | NOT RUN | |
| I10 Package | NOT RUN | |
| I11 Standalone | NOT RUN | |
| I12 Release | NOT RUN | |

All 56 acceptance tests (H, U, G, N, F, S, P, K) are NOT RUN on the TV.

## Done on the computer (host only, not device evidence)

- `evidence/hardware.json`: observed / documented / unverified / unknown facts kept separate.
- `tools/validate_manifest.py`: structure check; `--install-ready` rejects nulls and placeholders.
- `board/lg32lb580v/collect-report.sh`: read-only collector from p.43 (run on the TV only after I02).
- `probe/probe.c`: harmless probe; host build only. Target build blocked until I03.
- `ui/shell.*`: family home (Live TV, Football, Games, More; YouTube/Netflix tiles hidden until
  exact-device playback is tested). Favourites become shortcuts only when reception is confirmed.
- `services/session.*` + `platform/sim.c`: platform adapter interface; direct tuning reports
  UNSUPPORTED, so Football shortcuts fall back to the TV channel list (p.19).
- `games/`: Family Racer, Penalty Cup, Block Garden (original games, remote-only, seeded and
  deterministic) and a checksummed save service with last-known-good fallback.
- `sim/`: terminal simulator. `make run-sim`. All behaviour is labelled [SIMULATOR].
- `services/pin.*`: parent PIN stored only as a salted Argon2id verifier (vendored Monocypher
  4.0.2, see `third_party/monocypher/README.md`). 3 free tries, then 30 s doubling to a 5 min
  lockout; the failure count survives restarts. Argon2 cost must be re-tuned on the TV.
- `services/playtime.*`: children's daily allowance counted on a monotonic clock; resets at
  Nairobi midnight only while the TV clock is trusted (not before 2026, not moved backwards).
  Known limit: moving the TV clock forward can trigger an early reset.
- `services/settings.*` + `services/session.*`: Parent settings behind the PIN (language, daily
  limit, +15 min, reset today, allowed games, change PIN). Child profile survives restarts.
- `services/i18n.*`: optional Kiswahili labels (draft wording for the family to correct).
- `ui/canvas.*`, `ui/view.*`, `ui/art.*`: software 2D renderer at 1280x720 (ADR-02) with alpha
  blending, anti-aliased rounded shapes, gradients, soft shadows and anti-aliased kerned text; integer
  maths only (TV float ABI unknown). Apple TV-inspired look with our own vector artwork and no
  third-party logos. Views are pure functions of state; every draw is clipped to the buffer.
  `make screens` writes `docs/screens/*.png`. The TV display backend is still unchosen (gate I05).
- `ui/font_ui_data.c`: Inter 4.1 (SIL OFL) pre-rendered by `tools/fontbake.c` (stb_truetype plus a GPOS
  kerning reader) into 74 KB of 4-bit glyphs; the TV never parses font files.
- `make bench`: milliseconds per frame on the computer (home 1.8, settings 1.3, racer 2.1, penalty 2.8,
  garden 3.7 on an Apple-silicon Mac). The TV's 1 GHz Cortex-A9 is likely 15-25x slower, so racing may
  fall short of the 30 fps target (P03) until measured and tuned on the device.
- `sim/frontend_*.c`: the simulator runs either in the terminal (`make run-sim`) or in a desktop
  window showing the real pixel views (`make run-window`, SDL2 on the computer only; SDL is never
  part of the TV build). Held OK/Back do not repeat. `make window-smoke` checks the window build
  headlessly.
- Tests: 411 host checks (ASan/UBSan) plus two scripted simulator walks. Host logic for G01-G05,
  F03, S01, U01, U07-U09, R06. These are not TV results; the TV tests stay NOT RUN.

## Next

1. Owner: Work packet A (p.31) using the normal remote — channel list/order, Citizen/KTN/SportyTV reception, YouTube/Netflix status, stock timings.
2. Family: review the Kiswahili wording in `services/i18n.c`.
3. Host: frame-time and input-latency measurement in the window build (groundwork for P01/P03);
   optional offline-first fixtures cache. TV display/input backend only after gates I02-I05.
