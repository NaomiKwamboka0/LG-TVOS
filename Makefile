BUILD_ID := $(shell git rev-parse --short HEAD 2>/dev/null || echo dev)
OUT := out

CFLAGS ?= -O2 -g
WARN := -std=c99 -Wall -Wextra -pedantic
INC := -Igames -Iui -Iplatform -Iservices -Imedia -Isim -Ithird_party/monocypher

CORE_SRC := games/common.c games/save.c games/racer.c games/penalty.c games/garden.c \
            ui/shell.c services/i18n.c services/pin.c services/playtime.c services/settings.c \
            services/session.c services/channels.c services/prefs.c third_party/monocypher/monocypher.c \
            ui/canvas.c ui/font_ui_data.c ui/art.c ui/view.c ui/live.c platform/desktop.c \
            media/player.c media/player_stub.c
CORE_HDR := $(wildcard games/*.h ui/*.h platform/*.h services/*.h media/*.h sim/*.h third_party/monocypher/*.h)
TEST_SRC := tests/unit/test_core.c tests/unit/test_services.c tests/unit/test_view.c tests/unit/test_channels.c tests/unit/test_player.c
TERM_SRC := sim/frontend_text.c
# Sanitizers for the unit tests where the toolchain has them (not mingw).
SAN ?= -fsanitize=address,undefined
SYS_LIBS :=

ifeq ($(OS),Windows_NT)
# Native MSYS2 UCRT64 build: make test window. The terminal frontend needs
# termios, so Windows gets the window only.
SAN :=
SYS_LIBS := -lbcrypt
TERM_SRC :=
endif
SIM_SRC := $(CORE_SRC) sim/render_text.c $(TERM_SRC) sim/main.c

# Desktop window (SDL2: brew install sdl2).
SDL_CONFIG := $(shell command -v sdl2-config 2>/dev/null)
ifneq ($(SDL_CONFIG),)
SDL_CFLAGS := $(patsubst -I%,-isystem %,$(shell sdl2-config --cflags))
SDL_LIBS := $(shell sdl2-config --libs)
WINDOW_CHECK := window-smoke
endif

# Live channels need libmpv (brew install mpv pkgconf). Without it the
# window still builds and says live TV is not included.
MPV_OK := $(shell pkg-config --exists mpv 2>/dev/null && echo yes)
ifeq ($(MPV_OK),yes)
MPV_CFLAGS := -DFT_HAVE_MPV -isystem $(shell pkg-config --variable=includedir mpv)
MPV_LIBS := $(shell pkg-config --libs mpv)
MPV_SRC := media/player_mpv.c
endif

.PHONY: all term run-term window run window-smoke smoke screens bench fonts check-fonts icon test clean \
        win-deps win win-check win-test win-zip

all: check-fonts test smoke $(WINDOW_CHECK)

term: $(OUT)/host/familytv-term

$(OUT)/host/familytv-term: $(SIM_SRC) $(CORE_HDR)
	mkdir -p $(OUT)/host
	$(CC) $(CFLAGS) $(WARN) $(INC) -o $@ $(SIM_SRC)

run-term: term
	$(OUT)/host/familytv-term

window: $(OUT)/host/familytv

$(OUT)/host/familytv: $(SIM_SRC) sim/frontend_window.c $(MPV_SRC) $(CORE_HDR)
	@if [ -z "$(SDL_CONFIG)" ]; then echo "SDL2 not found: brew install sdl2"; exit 1; fi
	mkdir -p $(OUT)/host
	$(CC) $(CFLAGS) $(WARN) $(INC) $(SDL_CFLAGS) $(MPV_CFLAGS) -DFT_WINDOW -o $@ \
	  $(SIM_SRC) sim/frontend_window.c $(MPV_SRC) $(SDL_LIBS) $(MPV_LIBS) $(SYS_LIBS)

# Family TV in a window: arrows, Enter = OK, Backspace/Esc = Back, q = quit.
run: window
	$(OUT)/host/familytv

# Headless check of the window build: scripted keys, SDL dummy video driver, last frame dumped.
# 1: Apps -> Penalty Cup. 2: watch the banner channel with the stub player.
window-smoke: window
	rm -rf $(OUT)/wsmoke $(OUT)/wsmoke*.ppm
	SDL_VIDEODRIVER=dummy $(OUT)/host/familytv --data $(OUT)/wsmoke \
	  --dump-frame $(OUT)/wsmoke.ppm --keys BACK,R,R,D,R,OK,OK,OK,W90
	SDL_VIDEODRIVER=dummy $(OUT)/host/familytv --data $(OUT)/wsmoke \
	  --dump-frame $(OUT)/wsmoke-live.ppm --keys OK,W60,U,W
	python3 -I tools/ppm2png.py $(OUT) $(OUT)/wsmoke.ppm $(OUT)/wsmoke-live.ppm > /dev/null
	@echo "window-smoke: OK ($(OUT)/wsmoke.png, $(OUT)/wsmoke-live.png)"

# Scripted walks with throwaway data folders (never the interactive out/simdata).
# 1: watch the banner channel (stub player), change channel, back home,
#    then Apps -> Games -> Family Racer start/pause/exit.
# 2: gear -> Settings -> Parent settings -> create PIN 0000 twice -> Kiswahili.
smoke: term
	rm -rf $(OUT)/smoke1 $(OUT)/smoke2
	$(OUT)/host/familytv-term --data $(OUT)/smoke1 \
	  --keys OK,W60,U,W30,BACK,BACK,R,R,D,OK,OK,W60,BACK,BACK > $(OUT)/smoke1.txt
	grep -q "Citizen TV.*Playing" $(OUT)/smoke1.txt
	grep -q "NTV Kenya.*Playing" $(OUT)/smoke1.txt
	grep -q "Family Racer" $(OUT)/smoke1.txt
	$(OUT)/host/familytv-term --data $(OUT)/smoke2 \
	  --keys BACK,R,R,R,OK,D,OK,OK,OK,OK,OK,OK,OK,OK,OK,OK > $(OUT)/smoke2.txt
	grep -q "Choose a new 4-digit parent PIN" $(OUT)/smoke2.txt
	grep -q "Parent PIN saved" $(OUT)/smoke2.txt
	grep -q "Lugha: Kiswahili" $(OUT)/smoke2.txt
	! grep -q "0000" $(OUT)/smoke2.txt
	@echo "smoke: OK"

INTER := assets/fonts/inter
FONT_SPECS := font_display=$(INTER)/InterDisplay-Bold.ttf:46 font_title=$(INTER)/InterDisplay-SemiBold.ttf:30 \
              font_label=$(INTER)/Inter-SemiBold.ttf:24 font_body=$(INTER)/Inter-Medium.ttf:20 \
              font_caption=$(INTER)/Inter-Medium.ttf:18

# Font baker (stb_truetype); the app itself never parses fonts.
$(OUT)/host/fontbake: tools/fontbake.c third_party/stb/stb_truetype.h
	mkdir -p $(OUT)/host
	$(CC) -O2 -w -Ithird_party/stb -o $@ tools/fontbake.c -lm

# Regenerate ui/font_ui_data.c from the vendored Inter fonts.
fonts: $(OUT)/host/fontbake
	$(OUT)/host/fontbake $(FONT_SPECS) > ui/font_ui_data.c

# The checked-in glyph tables must match a fresh bake (same toolchain).
check-fonts: $(OUT)/host/fontbake
	$(OUT)/host/fontbake $(FONT_SPECS) > $(OUT)/font_ui_data.c 2>/dev/null
	cmp -s $(OUT)/font_ui_data.c ui/font_ui_data.c || { echo "ui/font_ui_data.c is stale: run make fonts"; exit 1; }
	@echo "fonts: OK"

# Render every screen to docs/screens/*.png for review (live TV uses the stub test card).
screens: $(OUT)/host/familytv-shots
	rm -rf $(OUT)/screens
	$(OUT)/host/familytv-shots $(OUT)/screens > /dev/null
	python3 -I tools/ppm2png.py docs/screens $(OUT)/screens/*.ppm

# Milliseconds per frame for each screen on this computer.
bench: $(OUT)/host/familytv-bench
	$(OUT)/host/familytv-bench

$(OUT)/host/familytv-bench: $(CORE_SRC) sim/bench.c $(CORE_HDR)
	mkdir -p $(OUT)/host
	$(CC) -O2 $(WARN) $(INC) -o $@ $(CORE_SRC) sim/bench.c

$(OUT)/host/familytv-shots: $(CORE_SRC) sim/shots.c $(CORE_HDR)
	mkdir -p $(OUT)/host
	$(CC) $(CFLAGS) $(WARN) $(INC) -o $@ $(CORE_SRC) sim/shots.c

$(OUT)/host/test-core: $(CORE_SRC) $(TEST_SRC) tests/unit/check.h $(CORE_HDR)
	mkdir -p $(OUT)/host
	$(CC) $(CFLAGS) $(WARN) $(INC) -Itests/unit $(SAN) -o $@ $(CORE_SRC) $(TEST_SRC) $(SYS_LIBS)

test: $(OUT)/host/test-core
	$(OUT)/host/test-core

# App icon for Windows, drawn with our own art (checked in, like the fonts).
$(OUT)/host/iconbake: tools/iconbake.c ui/canvas.c ui/font_ui_data.c ui/art.c $(CORE_HDR)
	mkdir -p $(OUT)/host
	$(CC) -O2 $(WARN) $(INC) -o $@ tools/iconbake.c ui/canvas.c ui/font_ui_data.c ui/art.c

icon: $(OUT)/host/iconbake
	$(OUT)/host/iconbake | python3 -I tools/mkico.py packaging/windows/familytv.ico

# ---- Windows, cross-built on this Mac (brew install mingw-w64 sevenzip) ------
# make win-deps   fetch SDL2, libmpv, yt-dlp and deno (pinned, hash-checked)
# make win-zip    out/win/FamilyTV-<commit>.zip to copy to the Windows laptop
WIN_CC ?= x86_64-w64-mingw32-gcc
WIN_RC ?= x86_64-w64-mingw32-windres
WIN_OBJDUMP ?= x86_64-w64-mingw32-objdump
WIN_SDL := deps/sdl2/x/SDL2-2.32.10/x86_64-w64-mingw32
WIN_MPV := deps/mpv/x
WIN_OUT := $(OUT)/win
WIN_DIR := $(WIN_OUT)/FamilyTV
WIN_SRC := $(CORE_SRC) sim/render_text.c sim/main.c sim/frontend_window.c media/player_mpv.c
WIN_FLAGS := -O2 $(WARN) $(INC) -isystem $(WIN_SDL)/include/SDL2 -isystem $(WIN_MPV)/include \
             -DFT_WINDOW -DFT_HAVE_MPV
# Only Windows' own DLLs, plus the two we ship: no hidden gcc/pthread DLLs.
WIN_DLLS_OK := ^(SDL2|libmpv-2|KERNEL32|USER32|ADVAPI32|bcrypt|SHELL32|api-ms-win-crt-[a-z0-9-]+)\.dll$$

win-deps:
	tools/fetch-win-deps.sh

$(WIN_OUT)/familytv-res.o: packaging/windows/familytv.rc packaging/windows/familytv.manifest \
                           packaging/windows/familytv.ico
	mkdir -p $(WIN_OUT)
	$(WIN_RC) -I packaging/windows -O coff -o $@ $<

$(WIN_DIR)/FamilyTV.exe: $(WIN_SRC) $(CORE_HDR) $(WIN_OUT)/familytv-res.o
	@test -f deps/sdl2/.ok -a -f deps/mpv/.ok || { echo "Windows libraries missing: make win-deps"; exit 1; }
	mkdir -p $(WIN_DIR)
	$(WIN_CC) $(WIN_FLAGS) -mwindows -static-libgcc -o $@ $(WIN_SRC) $(WIN_OUT)/familytv-res.o \
	  -L$(WIN_SDL)/lib -L$(WIN_MPV) -lSDL2 -lmpv -lbcrypt

win: $(WIN_DIR)/FamilyTV.exe
	@test -f deps/yt-dlp/.ok -a -f deps/deno/.ok || { echo "yt-dlp or deno missing: make win-deps"; exit 1; }
	cp $(WIN_SDL)/bin/SDL2.dll $(WIN_MPV)/libmpv-2.dll deps/yt-dlp/x/yt-dlp.exe deps/deno/x/deno.exe $(WIN_DIR)/
	cp assets/channels.txt packaging/windows/README-Windows.txt $(WIN_DIR)/
	mkdir -p $(WIN_DIR)/LICENSES
	cp packaging/windows/NOTICE.txt $(WIN_DIR)/LICENSES/
	cp deps/sdl2/x/SDL2-2.32.10/LICENSE.txt $(WIN_DIR)/LICENSES/SDL2-LICENSE.txt
	cp third_party/monocypher/LICENCE.md $(WIN_DIR)/LICENSES/Monocypher-LICENCE.md
	cp assets/fonts/inter/LICENSE.txt $(WIN_DIR)/LICENSES/Inter-LICENSE.txt

win-check: win
	$(WIN_OBJDUMP) -p $(WIN_DIR)/FamilyTV.exe | awk '/DLL Name:/ {print $$3}' | sort -u | tee $(WIN_OUT)/dlls.txt
	! grep -viE '$(WIN_DLLS_OK)' $(WIN_OUT)/dlls.txt
	@echo "win-check: OK"

# The unit tests as a Windows program (no sanitizers in mingw); run it on
# Windows from the folder holding assets/ and out/, or under Wine.
win-test:
	mkdir -p $(WIN_OUT)
	$(WIN_CC) -O2 $(WARN) $(INC) -Itests/unit -o $(WIN_OUT)/test-core.exe $(CORE_SRC) $(TEST_SRC) -lbcrypt

win-zip: win-check
	rm -f $(WIN_OUT)/FamilyTV-*.zip
	cd $(WIN_OUT) && zip -qr -9 FamilyTV-$(BUILD_ID).zip FamilyTV
	@ls -l $(WIN_OUT)/FamilyTV-$(BUILD_ID).zip

clean:
	rm -rf $(OUT)
