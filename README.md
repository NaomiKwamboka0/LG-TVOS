# LG-TVOS — family TV system for LG 32LB580V-TA

A TV-resident custom experience for an LG 32LB580V-TA (NetCast, chassis LD46B), following
the *Installable OS development plan v2.0*. Aerial TV comes first; the computer is only a build tool.

**Nothing in this repo has run on the TV yet.** See [docs/STATUS.md](docs/STATUS.md).

```sh
make            # probe, C + Python tests, simulator smoke test, manifest check
make run-sim    # play the home screen and games in the terminal (demo favourites)
make install-check   # expected to REJECT until runtime/recovery evidence exists
make target-probe CROSS_CC=...   # blocked until the target ABI is verified (gate I03)
```

Layout follows plan p.13: `docs/ evidence/ board/ platform/ services/ ui/ games/ assets/ tests/ tools/ packaging/`.

Rules carried from the plan: no flashing scripts, no guessed partition offsets, no service-menu
changes, no install without a proven recovery route.
