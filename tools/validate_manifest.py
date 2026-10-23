#!/usr/bin/env python3
"""Validate evidence/hardware.json.

Default mode checks structure only. --install-ready additionally requires every
runtime/install fact to be present and real; this is expected to FAIL today
(plan v2.0 p.18: placeholders must make the validator reject an install).
"""
import json
import re
import sys

EXPECTED_MODEL = "32LB580V-TA"
EXPECTED_PRODUCT = "32LB580V-TA.AFKWLH"
STATES = {"observed", "documented", "unverified", "runtime_verified"}
PLACEHOLDER = re.compile(r"<[^>]*>|\b(TODO|TBD|unknown|placeholder)\b", re.I)


def is_placeholder(v):
    return isinstance(v, str) and bool(PLACEHOLDER.search(v))


def check_structure(m):
    errors = []
    if m.get("schema") != 1:
        errors.append("schema must be 1")
    for section in ("unit", "documented_design", "runtime", "install"):
        if not isinstance(m.get(section), dict):
            errors.append(f"missing section: {section}")
    if errors:
        return errors
    for section in ("unit", "documented_design", "unverified_claims"):
        for key, fact in m.get(section, {}).items():
            if not isinstance(fact, dict) or "value" not in fact:
                errors.append(f"{section}.{key}: must be an object with 'value'")
                continue
            if fact.get("state") not in STATES:
                errors.append(f"{section}.{key}: invalid state {fact.get('state')!r}")
            if not fact.get("source"):
                errors.append(f"{section}.{key}: missing source")
            if is_placeholder(fact["value"]):
                errors.append(f"{section}.{key}: placeholder value")
    unit = m["unit"]
    if unit.get("model", {}).get("value") != EXPECTED_MODEL:
        errors.append("unit.model does not match target")
    if unit.get("product_code", {}).get("value") != EXPECTED_PRODUCT:
        errors.append("unit.product_code does not match target")
    return errors


def check_install_ready(m):
    blockers = []
    if m["unit"].get("identity_consistent", {}).get("value") is not True:
        blockers.append("I01: unit identity discrepancy unresolved")
    for section in ("runtime", "install"):
        for key, v in m[section].items():
            if v is None or v == "" or is_placeholder(v):
                blockers.append(f"{section}.{key}: not measured on this TV")
    return blockers


def main(argv):
    args = [a for a in argv if not a.startswith("--")]
    path = args[0] if args else "evidence/hardware.json"
    try:
        with open(path) as f:
            m = json.load(f)
    except (OSError, json.JSONDecodeError) as e:
        print(f"ERROR: cannot read {path}: {e}")
        return 2
    errors = check_structure(m)
    for e in errors:
        print(f"ERROR: {e}")
    if errors:
        return 2
    print("structure: OK")
    if "--install-ready" in argv:
        blockers = check_install_ready(m)
        for b in blockers:
            print(f"BLOCKED: {b}")
        if blockers:
            print(f"install-ready: REJECTED ({len(blockers)} blockers)")
            return 1
        print("install-ready: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
