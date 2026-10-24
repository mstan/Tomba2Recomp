"""Brief matched-scene Tomba/Mario comparison in the same private debug runtime.

This measures costume cost with the mod loaded in both cases. It does not
qualify the framework against a separate pre-change or mod-disabled binary.
"""
import argparse
import json
from pathlib import Path
import subprocess
import time
from mario_probe import Probe


def compare(probe, frames=700, reverse=False):
    if frames < 600:
        raise ValueError("Allow the 256-present performance ring to replace its prior samples")
    report = {"method": __doc__.strip(), "vblanks_per_case": frames, "cases": []}
    probe.wait_game_ready()
    cases = [("Tomba", 7, 0), ("Mario", 8, 1)]
    if reverse:
        cases.reverse()
    for label, slot, equipped in cases:
        probe.savestate("load", slot)
        probe.wait_frames(24)
        state, player = probe.state(), probe.player()
        if state["equipped"] != equipped or player["mode"] != 1 or player["air"]:
            raise RuntimeError(f"{label} fixture is not quiet grounded gameplay")
        start = time.monotonic()
        route = probe.wait_frames(frames)
        elapsed = time.monotonic() - start
        report["cases"].append({"form": label, "player": player,
                                "wall_seconds": elapsed,
                                "guest_vblanks_per_second": frames/elapsed,
                                "route": route, "perf": probe.call("frame_perf"),
                                "native": probe.call("overlay_loader_status")})
    if report["cases"][0]["player"]["position"] != report["cases"][1]["player"]["position"]:
        raise RuntimeError("Fixture positions differ; comparison is not matched")
    for key, path in (("title_head", "."), ("framework_head", "psxrecomp-v4")):
        report[key] = subprocess.check_output(["git", "-C", path, "rev-parse", "HEAD"], text=True).strip()
    report["source_state"] = "uncommitted implementation; retain build log and source diff with results"
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=4597)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--reverse", action="store_true")
    args = parser.parse_args()
    report = compare(Probe(args.port), reverse=args.reverse)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    for case in report["cases"]:
        print(case["form"], f"{case['guest_vblanks_per_second']:.2f} guest VBlanks/s", case["perf"]["all"])
