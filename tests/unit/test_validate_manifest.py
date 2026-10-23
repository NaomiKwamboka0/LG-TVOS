import copy
import json
import os
import sys
import unittest

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import validate_manifest as vm  # noqa: E402

with open(os.path.join(ROOT, "evidence", "hardware.json")) as f:
    REAL = json.load(f)


class StructureTests(unittest.TestCase):
    def test_current_manifest_structure_ok(self):
        self.assertEqual(vm.check_structure(REAL), [])

    def test_wrong_model_rejected(self):
        m = copy.deepcopy(REAL)
        m["unit"]["model"]["value"] = "55LB580V-ZM"
        self.assertTrue(any("model" in e for e in vm.check_structure(m)))

    def test_placeholder_fact_rejected(self):
        m = copy.deepcopy(REAL)
        m["unit"]["firmware"]["value"] = "<verified-hash-required>"
        self.assertTrue(any("placeholder" in e for e in vm.check_structure(m)))

    def test_missing_source_rejected(self):
        m = copy.deepcopy(REAL)
        del m["documented_design"]["soc"]["source"]
        self.assertTrue(any("source" in e for e in vm.check_structure(m)))


class InstallReadyTests(unittest.TestCase):
    def test_current_manifest_is_blocked(self):
        blockers = vm.check_install_ready(REAL)
        self.assertIn("I01: unit identity discrepancy unresolved", blockers)
        self.assertTrue(any("install.recovery_route" in b for b in blockers))

    def test_placeholder_runtime_value_blocks(self):
        m = self._fully_filled()
        m["install"]["abi"] = "<verified-target-required>"
        self.assertEqual(vm.check_install_ready(m), ["install.abi: not measured on this TV"])

    def test_fully_filled_passes(self):
        self.assertEqual(vm.check_install_ready(self._fully_filled()), [])

    def _fully_filled(self):
        m = copy.deepcopy(REAL)
        m["unit"]["identity_consistent"]["value"] = True
        for section in ("runtime", "install"):
            for k in m[section]:
                m[section][k] = "synthetic-test-value"
        return m


if __name__ == "__main__":
    unittest.main()
