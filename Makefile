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
SIM_SRC := $(CORE_SRC) sim/render_text.c sim/frontend_text.c sim/main.c
# Sanitizers for the unit tests where the toolchain has them (not mingw).
SAN ?= -fsanitize=address,undefined
SYS_LIBS :=

ifeq ($(OS),Windows_NT)
# Native MSYS2 UCRT64 build. The terminal frontend needs termios, so Windows gets the window only.
SAN :=
SYS_LIBS := -lbcrypt
endif

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

.PHONY: all term run-term window run window-smoke smoke screens bench fonts check-fonts test clean

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
	  $(SIM_SRC) sim/frontend_window.c $(MPV_SRC) $(SDL_LIBS) $(MPV_LIBS)

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

clean:
	rm -rf $(OUT)
