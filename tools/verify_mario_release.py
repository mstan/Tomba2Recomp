"""Verify the shipped Mario contract and exclude private imported material."""
from pathlib import Path
import sys
import tomllib


def verify(stage):
    stage = Path(stage)
    manifest = stage / "mods/bundled/tomba2.character.mario-hat/0.0.1/manifest.toml"
    package = tomllib.loads(manifest.read_text(encoding="utf-8"))
    assert package["version"] == "0.0.1" and package["channel"] == "experimental"
    assert not package["feature"][0]["default_enabled"]
    rom = next(r for r in package["resource"] if r["id"] == "smash-rom")
    assert rom["required"] and rom["input_only"] and rom["format"] == "n64-rom"
    assert rom["size"] == 16777216
    assert rom["sha256"] == "15592e79d3c5295cef4371d4992f0bd25bec2102fc29644c93e682f7ea99ef3d"
    for forbidden in ("psx.enhancement.cd-speed", "psx.enhancement.fast-loading"):
        assert not (stage / "mods/bundled" / forbidden).exists(), forbidden
    for path in stage.rglob("*"):
        assert path.suffix.lower() not in (".z64", ".v64", ".n64", ".wav", ".cache"), str(path)
    print("Mario release contract: EXPERIMENTAL 0.0.1, default off, verified ROM required, no imported assets")


if __name__ == "__main__":
    verify(sys.argv[1])
