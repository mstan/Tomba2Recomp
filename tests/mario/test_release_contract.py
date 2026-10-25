"""Release configuration must retain the native hooks used by the mod."""
from pathlib import Path
import tomllib
import unittest

ROOT = Path(__file__).resolve().parents[2]


class ReleaseContract(unittest.TestCase):
    def test_release_uses_the_tested_native_hook_sites(self):
        development = tomllib.loads((ROOT / "game.toml").read_text(encoding="utf-8"))
        release = tomllib.loads((ROOT / "packaging/release/game.toml").read_text(encoding="utf-8"))
        for key in ("mod_function_entry_funcs", "mod_instruction_sites"):
            self.assertEqual(development["recompiler"][key], release["recompiler"][key], key)

    def test_regional_release_catalogs_exclude_loading_controls(self):
        for catalog in ("preloaded", "preloaded_ita"):
            for manifest in (ROOT / "mods" / catalog / "packages").glob("*/*/manifest.toml"):
                package = tomllib.loads(manifest.read_text(encoding="utf-8"))
                self.assertNotIn(package["id"], (
                    "psx.enhancement.cd-speed", "psx.enhancement.fast-loading"), str(manifest))


if __name__ == "__main__":
    unittest.main()
