BUILD_ID := $(shell git rev-parse --short HEAD 2>/dev/null || echo dev)
OUT := out

CFLAGS ?= -O2 -g
WARN := -std=c99 -Wall -Wextra -pedantic
INC := -Igames -Iui -Iplatform -Iservices -Isim -Ithird_party/monocypher

CORE_SRC := games/common.c games/save.c games/racer.c games/penalty.c games/garden.c \
            ui/shell.c services/i18n.c services/pin.c services/playtime.c services/settings.c \
            services/session.c third_party/monocypher/monocypher.c
CORE_HDR := $(wildcard games/*.h ui/*.h platform/*.h services/*.h sim/*.h third_party/monocypher/*.h)
TEST_SRC := tests/unit/test_core.c tests/unit/test_services.c
SIM_SRC := $(CORE_SRC) platform/sim.c sim/render_text.c sim/main.c

.PHONY: all host-probe target-probe sim run-sim smoke test c-test py-test validate install-check clean

all: host-probe test smoke validate

host-probe:
	mkdir -p $(OUT)/host
	$(CC) -O2 -Wall -Wextra -DBUILD_ID='"$(BUILD_ID)"' -o $(OUT)/host/probe probe/probe.c
	$(OUT)/host/probe

# Cross-build is blocked until gate I03 records a verified ABI/toolchain
# (evidence/hardware.json install.abi). Never guess -mfloat-abi or -mfpu.
target-probe:
	@if [ -z "$(CROSS_CC)" ]; then \
	  echo "BLOCKED: CROSS_CC not set; target ABI not yet verified on the TV (gate I03)"; exit 1; fi
	mkdir -p $(OUT)/target
	$(CROSS_CC) -O2 -Wall -DBUILD_ID='"$(BUILD_ID)"' -DBUILD_TARGET='"lg32lb580v"' -o $(OUT)/target/probe probe/probe.c
	file $(OUT)/target/probe
	shasum -a 256 $(OUT)/target/probe

sim: $(OUT)/host/family-tv-sim

$(OUT)/host/family-tv-sim: $(SIM_SRC) $(CORE_HDR)
	mkdir -p $(OUT)/host
	$(CC) $(CFLAGS) $(WARN) $(INC) -o $@ $(SIM_SRC)

run-sim: sim
	$(OUT)/host/family-tv-sim --demo-received

# Scripted walks with throwaway data folders (never the interactive out/simdata).
# 1: Football -> SportyTV fallback, then Games -> Racer start/pause/exit.
# 2: More -> Parent settings -> create PIN 0000 twice -> switch language to Kiswahili.
smoke: sim
	rm -rf $(OUT)/smoke1 $(OUT)/smoke2
	$(OUT)/host/family-tv-sim --demo-received --data $(OUT)/smoke1 \
	  --keys R,OK,D,D,OK,BACK,R,OK,OK,OK,W60,BACK,BACK > $(OUT)/smoke1.txt
	grep -q "channel list" $(OUT)/smoke1.txt
	grep -q "Family Racer" $(OUT)/smoke1.txt
	$(OUT)/host/family-tv-sim --data $(OUT)/smoke2 \
	  --keys D,OK,D,D,OK,OK,OK,OK,OK,OK,OK,OK,OK,OK > $(OUT)/smoke2.txt
	grep -q "Choose a new 4-digit parent PIN" $(OUT)/smoke2.txt
	grep -q "Parent PIN saved" $(OUT)/smoke2.txt
	grep -q "Lugha: Kiswahili" $(OUT)/smoke2.txt
	! grep -q "0000" $(OUT)/smoke2.txt
	@echo "smoke: OK"

$(OUT)/host/test-core: $(CORE_SRC) platform/sim.c $(TEST_SRC) tests/unit/check.h $(CORE_HDR)
	mkdir -p $(OUT)/host
	$(CC) $(CFLAGS) $(WARN) $(INC) -Itests/unit -fsanitize=address,undefined -o $@ $(CORE_SRC) platform/sim.c $(TEST_SRC)

c-test: $(OUT)/host/test-core
	$(OUT)/host/test-core

py-test:
	python3 -m unittest discover -s tests/unit -v

test: c-test py-test

validate:
	python3 tools/validate_manifest.py evidence/hardware.json

install-check:
	python3 tools/validate_manifest.py --install-ready evidence/hardware.json

clean:
	rm -rf $(OUT)
