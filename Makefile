BUILD_ID := $(shell git rev-parse --short HEAD 2>/dev/null || echo dev)
OUT := out

.PHONY: all host-probe target-probe test validate install-check clean

all: host-probe test validate

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

test:
	python3 -m unittest discover -s tests/unit -v

validate:
	python3 tools/validate_manifest.py evidence/hardware.json

install-check:
	python3 tools/validate_manifest.py --install-ready evidence/hardware.json

clean:
	rm -rf $(OUT)
