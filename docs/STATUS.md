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
- `tests/unit/test_core.c`: 259 host checks (ASan/UBSan). Host logic for G01-G05, F03, U01,
  U07-U09, R06. These are not TV results; the TV tests stay NOT RUN.

## Next

1. Owner: Work packet A (p.31) using the normal remote — channel list/order, Citizen/KTN/SportyTV reception, YouTube/Netflix status, stock timings.
2. Host: PIN-protected parent profile, play-time limits, Kiswahili labels, framebuffer renderer
   behind an interface (selected only after gate I05).
