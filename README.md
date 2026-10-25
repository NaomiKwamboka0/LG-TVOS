# LG-TVOS — family TV system for LG 32LB580V-TA

A TV-resident custom experience for an LG 32LB580V-TA (NetCast, chassis LD46B), following
the *Installable OS development plan v2.0*. Aerial TV comes first; the computer is only a build tool.

**Nothing in this repo has run on the TV yet.** See [docs/STATUS.md](docs/STATUS.md).

```sh
make            # probe, C + Python tests, simulator smoke test, manifest check
make run-window # play the pixel version in a desktop window (needs: brew install sdl2)
make run-sim    # play the home screen and games in the terminal (demo favourites)
                # parent settings: More -> Parent settings (you choose the PIN on first use)
make screens    # draw every screen to docs/screens/*.png (see docs/screens/README.md)
make bench      # milliseconds per frame for each screen on this computer
make install-check   # expected to REJECT until runtime/recovery evidence exists
make target-probe CROSS_CC=...   # blocked until the target ABI is verified (gate I03)
```

Layout follows plan p.13: `docs/ evidence/ board/ platform/ services/ ui/ games/ assets/ tests/ tools/ packaging/`.

Rules carried from the plan: no flashing scripts, no guessed partition offsets, no service-menu
changes, no install without a proven recovery route.
