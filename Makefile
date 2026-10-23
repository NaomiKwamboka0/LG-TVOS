BUILD_ID := $(shell git rev-parse --short HEAD 2>/dev/null || echo dev)
OUT := out

CFLAGS ?= -O2 -g
WARN := -std=c99 -Wall -Wextra -pedantic
INC := -Igames -Iui -Iplatform -Iservices -Isim

CORE_SRC := games/common.c games/save.c games/racer.c games/penalty.c games/garden.c \
            ui/shell.c services/session.c
CORE_HDR := $(wildcard games/*.h ui/*.h platform/*.h services/*.h sim/*.h)
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

# Scripted walk: Football -> SportyTV fallback, then Games -> Racer start/pause/exit.
smoke: sim
	$(OUT)/host/family-tv-sim --demo-received --keys R,OK,D,D,OK,BACK,R,OK,OK,OK,W60,BACK,BACK > $(OUT)/smoke.txt
	grep -q "channel list" $(OUT)/smoke.txt
	grep -q "Family Racer" $(OUT)/smoke.txt
	@echo "smoke: OK"

$(OUT)/host/test-core: $(CORE_SRC) platform/sim.c tests/unit/test_core.c $(CORE_HDR)
	mkdir -p $(OUT)/host
	$(CC) $(CFLAGS) $(WARN) $(INC) -fsanitize=address,undefined -o $@ $(CORE_SRC) platform/sim.c tests/unit/test_core.c

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
