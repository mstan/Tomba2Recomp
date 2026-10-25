"""Regression checks for refusing stale/missing playtest artifacts."""
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "tools"))
from mario_playtest_gate import validate_aot, validate_qualification, validate_production_build, framework_tools


class GateTest(unittest.TestCase):
    def test_production_refuses_diagnostics_and_stale_staging(self):
        with tempfile.TemporaryDirectory() as scratch:
            root = Path(scratch)
            build = root / "build"
            build.mkdir()
            exe = root / "Mario.exe"
            exe.write_bytes(b"production")
            (build / exe.name).write_bytes(b"production")
            settings = "CMAKE_BUILD_TYPE:STRING=Release\nPSX_DEBUG_TOOLS:BOOL=OFF\n"
            (build / "CMakeCache.txt").write_text(settings)
            (build / "build.ninja").write_text("DEFINES = -DPSX_NO_DEBUG_TOOLS=1\n")
            validate_production_build(build, exe)
            (build / "CMakeCache.txt").write_text(settings.replace("=OFF", "=ON"))
            with self.assertRaisesRegex(RuntimeError, "diagnostics OFF"):
                validate_production_build(build, exe)
            (build / "CMakeCache.txt").write_text(settings)
            (build / "build.ninja").write_text("DEFINES = -DPSX_DEBUG_TOOLS=1\n")
            with self.assertRaisesRegex(RuntimeError, "compile flags"):
                validate_production_build(build, exe)
            (build / "build.ninja").write_text("DEFINES = -DPSX_NO_DEBUG_TOOLS=1\n")
            exe.write_bytes(b"old staging")
            with self.assertRaisesRegex(RuntimeError, "differs"):
                validate_production_build(build, exe)

    def test_missing_changed_and_wrong_namespace_shards_are_refused(self):
        with tempfile.TemporaryDirectory() as scratch:
            root = Path(scratch)
            tag = "test-current-namespace"
            folder = root / "cache/SCUS-94454/gcc" / framework_tools().cache_arch_abi() / tag
            folder.mkdir(parents=True)
            (folder / "test.dll").write_bytes(b"audited compiled code")
            (folder / "test.ranges").write_bytes(b"audited original-byte guards")
            receipt = {"game_id": "SCUS-94454", "cache_tag": tag, "recipe_count": 1,
                       "required_images": ["MAIN.EXE"], "image_coverage": [{}],
                       "all_pairs_valid": True, "all_guards_match_known_input_bytes": True,
                       "published_pairs": 1, "pairs": [{"dll": "test.dll",
                           "dll_sha256": hashlib.sha256((folder / "test.dll").read_bytes()).hexdigest(),
                           "manifest_sha256": hashlib.sha256((folder / "test.ranges").read_bytes()).hexdigest()}]}
            (root / "AOT_CACHE_AUDIT.json").write_text(json.dumps(receipt))
            validate_aot(root, tag)
            with self.assertRaisesRegex(RuntimeError, "another configuration"):
                validate_aot(root, "stale-tag")
            (folder / "test.dll").write_bytes(b"replaced after audit")
            with self.assertRaisesRegex(RuntimeError, "changed audited shard"):
                validate_aot(root, tag)
            (folder / "test.dll").unlink()
            with self.assertRaisesRegex(RuntimeError, "changed audited shard"):
                validate_aot(root, tag)

    def test_new_binary_or_failed_runtime_check_cannot_reuse_qualification(self):
        identity = {"exe_sha256": "old", "cache_tag": "current"}
        report = {"identity": identity, "passed": True, "checks": {"moving_pacing": True}}
        validate_qualification(identity, report)
        with self.assertRaisesRegex(RuntimeError, "stale"):
            validate_qualification({**identity, "exe_sha256": "new"}, report)
        with self.assertRaisesRegex(RuntimeError, "No passing"):
            validate_qualification(identity, {**report, "checks": {"moving_pacing": False}})


if __name__ == "__main__":
    unittest.main()
