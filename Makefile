BUILD_ID := $(shell git rev-parse --short HEAD 2>/dev/null || echo dev)
VERSION := 1.0.0
OUT := out

CFLAGS ?= -O2 -g
WARN := -std=c99 -Wall -Wextra -pedantic
INC := -Igames -Iui -Iplatform -Iservices -Imedia -Isim -Ithird_party/monocypher -Ithird_party/stb

CORE_SRC := games/common.c games/save.c games/racer.c games/penalty.c games/garden.c \
            ui/shell.c services/i18n.c services/pin.c services/playtime.c services/settings.c \
            services/session.c services/channels.c services/prefs.c services/sports.c services/youtube.c third_party/monocypher/monocypher.c \
            ui/canvas.c ui/font_ui_data.c ui/font_scale.c ui/art.c ui/view.c ui/live.c ui/motion.c platform/desktop.c \
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
SYS_LIBS := -lbcrypt -lshell32
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

# YouTube and Netflix inside the window: WebKit on a Mac, WebView2 on Windows.
ifeq ($(shell uname -s 2>/dev/null),Darwin)
WEB_SRC := sim/web_mac.m
WEB_LIBS := -framework WebKit -framework Cocoa
else
WEB_SRC := sim/web_none.c
endif

# Live channels need libmpv (brew install mpv pkgconf). Without it the
# window still builds and says live TV is not included.
MPV_OK := $(shell pkg-config --exists mpv 2>/dev/null && echo yes)
ifeq ($(MPV_OK),yes)
MPV_CFLAGS := -DFT_HAVE_MPV -isystem $(shell pkg-config --variable=includedir mpv)
MPV_LIBS := $(shell pkg-config --libs mpv)
MPV_SRC := media/player_mpv.c
endif

.PHONY: all term run-term window run window-smoke smoke screens promo bench fonts check-fonts icon test clean \
        win-deps win win-check win-test win-zip win-setup mac-deps mac-app mac-check mac-dmg release

all: check-fonts test smoke $(WINDOW_CHECK)

term: $(OUT)/host/familytv-term

$(OUT)/host/familytv-term: $(SIM_SRC) $(CORE_HDR)
	mkdir -p $(OUT)/host
	$(CC) $(CFLAGS) $(WARN) $(INC) -o $@ $(SIM_SRC)

run-term: term
	$(OUT)/host/familytv-term

window: $(OUT)/host/familytv

$(OUT)/host/familytv: $(SIM_SRC) sim/frontend_window.c $(WEB_SRC) $(MPV_SRC) $(CORE_HDR)
	@if [ -z "$(SDL_CONFIG)" ]; then echo "SDL2 not found: brew install sdl2"; exit 1; fi
	mkdir -p $(OUT)/host
	$(CC) $(CFLAGS) $(WARN) $(INC) $(SDL_CFLAGS) $(MPV_CFLAGS) $(WEB_CFLAGS) -DFT_WINDOW -o $@ \
	  $(SIM_SRC) sim/frontend_window.c $(WEB_SRC) $(MPV_SRC) $(SDL_LIBS) $(MPV_LIBS) $(WEB_LIBS) $(SYS_LIBS)

# Family TV in a window: arrows, Enter = OK, Backspace/Esc = Back, q = quit.
run: window
	$(OUT)/host/familytv

# Headless check of the window build: scripted keys, SDL dummy video driver, last frame dumped.
# 1: Apps -> Penalty Cup. 2: watch the banner channel with the stub player.
window-smoke: window
	rm -rf $(OUT)/wsmoke $(OUT)/wsmoke*.ppm
	SDL_VIDEODRIVER=dummy $(OUT)/host/familytv --data $(OUT)/wsmoke \
	  --dump-frame $(OUT)/wsmoke.ppm --keys BACK,R,R,R,R,D,R,OK,OK,OK,W90
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
	  --keys OK,W60,U,W30,BACK,BACK,R,R,R,R,D,OK,OK,W60,BACK,BACK > $(OUT)/smoke1.txt
	grep -q "Citizen TV.*Playing" $(OUT)/smoke1.txt
	grep -q "NTV Kenya.*Playing" $(OUT)/smoke1.txt
	grep -q "Family Racer" $(OUT)/smoke1.txt
	$(OUT)/host/familytv-term --data $(OUT)/smoke2 \
	  --keys BACK,R,R,R,R,R,OK,D,OK,OK,OK,OK,OK,OK,OK,OK,OK,OK > $(OUT)/smoke2.txt
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

# The 30-second advert, drawn by the app's own views (live TV uses the stub
# test card): out/promo/familytv-promo.mp4 and a poster in docs/promo.
PROMO := $(OUT)/promo
promo: $(OUT)/host/familytv-promo
	mkdir -p $(PROMO) docs/promo
	$(OUT)/host/familytv-promo | ffmpeg -loglevel error -y -f rawvideo -pix_fmt rgb24 -s 1920x1080 -r 30 -i - \
	  -c:v libx264 -preset slow -crf 20 -pix_fmt yuv420p -movflags +faststart $(PROMO)/familytv-promo.mp4
	$(OUT)/host/familytv-promo --frame 210 $(PROMO)/poster.ppm
	python3 -I tools/ppm2png.py docs/promo $(PROMO)/poster.ppm > /dev/null
	@ls -l $(PROMO)/familytv-promo.mp4 docs/promo/poster.png

$(OUT)/host/familytv-promo: $(CORE_SRC) sim/promo.c $(CORE_HDR)
	mkdir -p $(OUT)/host
	$(CC) $(CFLAGS) $(WARN) $(INC) -o $@ $(CORE_SRC) sim/promo.c -lm

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

# App icons for Windows and macOS, drawn with our own art (checked in, like the fonts).
$(OUT)/host/iconbake: tools/iconbake.c ui/canvas.c ui/font_ui_data.c ui/font_scale.c ui/art.c $(CORE_HDR)
	mkdir -p $(OUT)/host
	$(CC) -O2 $(WARN) $(INC) -o $@ tools/iconbake.c ui/canvas.c ui/font_ui_data.c ui/font_scale.c ui/art.c

icon: $(OUT)/host/iconbake
	$(OUT)/host/iconbake | python3 -I tools/mkico.py packaging/windows/familytv.ico
	rm -rf $(OUT)/familytv.iconset
	$(OUT)/host/iconbake 16 32 64 128 256 512 1024 | python3 -I tools/mkicns.py $(OUT)/familytv.iconset
	iconutil -c icns -o packaging/macos/familytv.icns $(OUT)/familytv.iconset

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
WIN_SRC := $(CORE_SRC) sim/render_text.c sim/main.c sim/frontend_window.c sim/web_none.c media/player_mpv.c
WIN_FLAGS := -O2 $(WARN) $(INC) -isystem $(WIN_SDL)/include/SDL2 -isystem $(WIN_MPV)/include \
             -DFT_WINDOW -DFT_HAVE_MPV
# Only Windows' own DLLs, plus the two we ship: no hidden gcc/pthread DLLs.
WIN_DLLS_OK := ^(SDL2|libmpv-2|KERNEL32|USER32|ADVAPI32|bcrypt|SHELL32|api-ms-win-crt-[a-z0-9-]+)\.dll$$

win-deps:
	tools/fetch-deps.sh tools/win-deps.lock

$(WIN_OUT)/familytv-res.o: packaging/windows/familytv.rc packaging/windows/familytv.manifest \
                           packaging/windows/familytv.ico
	mkdir -p $(WIN_OUT)
	$(WIN_RC) -I packaging/windows -O coff -o $@ $<

$(WIN_DIR)/FamilyTV.exe: $(WIN_SRC) $(CORE_HDR) $(WIN_OUT)/familytv-res.o
	@test -f deps/sdl2/.ok -a -f deps/mpv/.ok || { echo "Windows libraries missing: make win-deps"; exit 1; }
	mkdir -p $(WIN_DIR)
	$(WIN_CC) $(WIN_FLAGS) -mwindows -static-libgcc -o $@ $(WIN_SRC) $(WIN_OUT)/familytv-res.o \
	  -L$(WIN_SDL)/lib -L$(WIN_MPV) -lSDL2 -lmpv -lbcrypt -lshell32

win: $(WIN_DIR)/FamilyTV.exe
	@test -f deps/yt-dlp/.ok -a -f deps/deno/.ok || { echo "yt-dlp or deno missing: make win-deps"; exit 1; }
	cp $(WIN_SDL)/bin/SDL2.dll $(WIN_MPV)/libmpv-2.dll deps/yt-dlp/x/yt-dlp.exe deps/deno/x/deno.exe $(WIN_DIR)/
	cp assets/channels.txt packaging/windows/README-Windows.txt $(WIN_DIR)/
	mkdir -p $(WIN_DIR)/LICENSES
	cp packaging/windows/NOTICE.txt $(WIN_DIR)/LICENSES/
	cp deps/sdl2/x/SDL2-2.32.10/LICENSE.txt $(WIN_DIR)/LICENSES/SDL2-LICENSE.txt
	cp third_party/monocypher/LICENCE.md $(WIN_DIR)/LICENSES/Monocypher-LICENCE.md
	cp assets/fonts/inter/LICENSE.txt $(WIN_DIR)/LICENSES/Inter-LICENSE.txt
	mkdir -p $(WIN_DIR)/fonts
	cp $(INTER)/*.ttf $(WIN_DIR)/fonts/

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
	cd $(WIN_OUT) && zip -qr -9 FamilyTV-$(VERSION)-windows.zip FamilyTV
	@ls -l $(WIN_OUT)/FamilyTV-$(VERSION)-windows.zip

# Setup.exe with shortcuts and an uninstaller (brew install makensis; it needs a UTF-8 locale).
win-setup: win-check
	rm -f $(WIN_OUT)/FamilyTV-Setup-*.exe
	LC_ALL=en_US.UTF-8 makensis -V2 -DSRC=$(CURDIR)/$(WIN_DIR) -DOUTFILE=$(CURDIR)/$(WIN_OUT)/FamilyTV-Setup-$(VERSION).exe \
	  -DVERSION=$(VERSION) -DICON=$(CURDIR)/packaging/windows/familytv.ico packaging/windows/installer.nsi
	@ls -l $(WIN_OUT)/FamilyTV-Setup-$(VERSION).exe

# ---- macOS app for Apple silicon (brew install sdl2 mpv pkgconf dylibbundler) --
# make mac-deps   fetch yt-dlp and deno for macOS (pinned, hash-checked)
# make mac-dmg    out/mac/FamilyTV-<version>-macos.dmg to give to others
# Libraries come from Homebrew and are copied into the app, so the .dmg needs
# the macOS version those were built for (15 on current Homebrew).
MAC_OUT := $(OUT)/mac
MAC_APP := $(MAC_OUT)/Family TV.app
MAC_C := $(MAC_APP)/Contents
SDL3_LIB := $(shell pkg-config --variable=libdir sdl3 2>/dev/null)/libSDL3.0.dylib

mac-deps:
	tools/fetch-deps.sh tools/mac-deps.lock

mac-app: $(OUT)/host/familytv
	@test -f deps/mac-yt-dlp/.ok -a -f deps/mac-deno/.ok || { echo "yt-dlp or deno missing: make mac-deps"; exit 1; }
	@test "$(MPV_OK)" = yes || { echo "libmpv not found: brew install mpv pkgconf"; exit 1; }
	rm -rf "$(MAC_APP)"
	mkdir -p "$(MAC_C)/MacOS" "$(MAC_C)/Frameworks" "$(MAC_C)/Resources/fonts" "$(MAC_C)/Resources/LICENSES"
	sed 's/@VERSION@/$(VERSION)/g' packaging/macos/Info.plist > "$(MAC_C)/Info.plist"
	cp $(OUT)/host/familytv "$(MAC_C)/MacOS/FamilyTV"
	cp packaging/macos/familytv.icns assets/channels.txt packaging/macos/README-Mac.txt "$(MAC_C)/Resources/"
	cp $(INTER)/*.ttf "$(MAC_C)/Resources/fonts/"
	cp deps/mac-yt-dlp/x/yt-dlp_macos "$(MAC_C)/Resources/yt-dlp"
	cp deps/mac-deno/x/deno "$(MAC_C)/Resources/deno"
	cp LICENSE packaging/macos/NOTICE.txt "$(MAC_C)/Resources/LICENSES/"
	cp third_party/monocypher/LICENCE.md "$(MAC_C)/Resources/LICENSES/Monocypher-LICENCE.md"
	cp assets/fonts/inter/LICENSE.txt "$(MAC_C)/Resources/LICENSES/Inter-LICENSE.txt"
	cp "$$(dirname $(SDL3_LIB))/../share/licenses/SDL3/LICENSE.txt" "$(MAC_C)/Resources/LICENSES/SDL-LICENSE.txt" 2>/dev/null || \
	  cp "$$(brew --prefix sdl3)/LICENSE.txt" "$(MAC_C)/Resources/LICENSES/SDL-LICENSE.txt"
	dylibbundler -cd -of -b -x "$(MAC_C)/MacOS/FamilyTV" -d "$(MAC_C)/Frameworks" \
	  -p @executable_path/../Frameworks/ -s /opt/homebrew/lib < /dev/null > $(MAC_OUT)/dylibbundler.log
	# sdl2-compat opens SDL 3 at run time from its own folder, so dylibbundler cannot see it.
	cp $(SDL3_LIB) "$(MAC_C)/Frameworks/libSDL3.dylib"
	chmod u+w "$(MAC_C)/Frameworks/libSDL3.dylib"
	install_name_tool -id @executable_path/../Frameworks/libSDL3.dylib "$(MAC_C)/Frameworks/libSDL3.dylib" 2> /dev/null
	tools/mac-dedup-rpaths.sh "$(MAC_C)/MacOS/FamilyTV" "$(MAC_C)/Frameworks/"*.dylib
	codesign --force --sign - "$(MAC_C)/Frameworks/"*.dylib "$(MAC_C)/Resources/yt-dlp" "$(MAC_C)/MacOS/FamilyTV" 2> /dev/null
	codesign --force --sign - "$(MAC_APP)"
	@echo "mac-app: $(MAC_APP)"

# Nothing in the app may point back into Homebrew, and the signature must hold.
mac-check: mac-app
	for f in "$(MAC_C)/MacOS/FamilyTV" "$(MAC_C)/Frameworks/"*.dylib; do otool -L "$$f" | tail -n +2; done \
	  | awk '{print $$1}' | sort -u > $(MAC_OUT)/dylibs.txt
	! grep -E '^/(opt|usr/local)/' $(MAC_OUT)/dylibs.txt
	@# Info.plist promises macOS 15: nothing inside may need a newer one.
	for f in "$(MAC_C)/MacOS/FamilyTV" "$(MAC_C)/Frameworks/"*.dylib "$(MAC_C)/Resources/yt-dlp" "$(MAC_C)/Resources/deno"; do \
	  otool -l "$$f" | awk '/ minos / {split($$2, v, "."); print v[1]}'; done | sort -n | tail -1 > $(MAC_OUT)/minos.txt
	@test "$$(cat $(MAC_OUT)/minos.txt)" -le 15 || { echo "needs macOS $$(cat $(MAC_OUT)/minos.txt), Info.plist says 15.0"; exit 1; }
	codesign --verify --deep --strict "$(MAC_APP)"
	SDL_VIDEODRIVER=dummy "$(MAC_C)/MacOS/FamilyTV" --data $(MAC_OUT)/smoke \
	  --dump-frame $(MAC_OUT)/smoke.ppm --keys BACK,R,R,R,R,D,R,OK,OK,OK,W90
	@echo "mac-check: OK ($$(ls "$(MAC_C)/Frameworks" | wc -l | tr -d ' ') libraries bundled)"

mac-dmg: mac-check
	rm -rf $(MAC_OUT)/dmg $(MAC_OUT)/FamilyTV-*.dmg
	mkdir -p $(MAC_OUT)/dmg
	cp -R "$(MAC_APP)" packaging/macos/README-Mac.txt $(MAC_OUT)/dmg/
	ln -s /Applications $(MAC_OUT)/dmg/Applications
	hdiutil create -quiet -volname "Family TV" -srcfolder $(MAC_OUT)/dmg -fs HFS+ -format ULFO \
	  $(MAC_OUT)/FamilyTV-$(VERSION)-macos.dmg
	@ls -l $(MAC_OUT)/FamilyTV-$(VERSION)-macos.dmg

# Everything for a GitHub release, with checksums, in out/release: one
# installer per computer, nothing to unzip.
RELEASE := $(OUT)/release
release: win-setup mac-dmg
	rm -rf $(RELEASE)
	mkdir -p $(RELEASE)
	cp $(WIN_OUT)/FamilyTV-Setup-$(VERSION).exe $(MAC_OUT)/FamilyTV-$(VERSION)-macos.dmg $(RELEASE)/
	cd $(RELEASE) && shasum -a 256 FamilyTV-* > SHA256SUMS.txt
	@cat $(RELEASE)/SHA256SUMS.txt

clean:
	rm -rf $(OUT)
