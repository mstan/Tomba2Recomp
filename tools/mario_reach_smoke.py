"""Compare real punch reach using identical private post-pig input routes.

Requires slots 8 (quiet Mario) and 10 (post approach). Does not write RAM,
actor positions, damage, capture links or success flags.
"""
import argparse
import json
from pathlib import Path

from mario_probe import Probe
from mario_interaction_smoke import PLAYER, PIG_HANDLER, actor, find_post_pig


def run(probe, output, baseline=None):
    output.mkdir(parents=True, exist_ok=True)
    report = {"cases": [], "passed": False, "ram_writes": False}
    snapshots = probe.call("get_snapshots")["snapshots"]
    try:
        probe.savestate("load", 8)
        probe.wait_frames(8)
        pig = find_post_pig(probe)
        report["pig_address"] = hex(pig)
        probe.call("set_snapshot", slot=0, addr=hex(PLAYER))
        probe.call("set_snapshot", slot=1, addr=hex(pig))
        for travel in (0, 2, 4, 6, 8, 10, 12, 14):
            probe.savestate("load", 10)
            probe.wait_frames(8)
            target = actor(probe.ram(pig, 128))
            assert target["handler"] == PIG_HANDLER and target["mode"] == 1
            before = {c["name"]: c["count"] for c in probe.counters()["counters"]}
            start = probe.call("history")["newest"] + 1
            route = []
            if travel:
                route.append((travel, 0x20))
            route += [(4, 0x4020), (4, 0x2020), (65, 0)]
            receipt = probe.route(route)
            end = probe.call("history")["newest"]
            frames = []
            for frame in range(start, end + 1, 2):
                data = bytes.fromhex(probe.call("read_frame_ram", frame=frame,
                                               addr=hex(pig), len=128)["hex"])
                frames.append({"frame": frame, **actor(data)})
            after = {c["name"]: c["count"] for c in probe.counters()["counters"]}
            case = {"travel": travel, "route": receipt, "frames": frames,
                    "stunned": any(f["flags"][:2] in ("03", "07") and f["mode"] == 2 for f in frames),
                    "spawns": after.get("tomba2.mario.punch_spawn", 0) - before.get("tomba2.mario.punch_spawn", 0),
                    "retired": after.get("tomba2.mario.punch_retired", 0) - before.get("tomba2.mario.punch_retired", 0)}
            report["cases"].append(case)
            assert case["spawns"] == case["retired"] == 1, case
        probe.savestate("load", 8)
        before = actor(probe.ram(pig, 128))
        probe.route([(4, 0x2000), (70, 0)])
        after = actor(probe.ram(pig, 128))
        report["distant_miss"] = before["mode"] == after["mode"] == 1 and after["action"] == 0
        assert report["distant_miss"], "A distant punch reached the post pig"
        if baseline:
            original = {c["travel"]: c for c in json.loads(baseline.read_text())["cases"]}
            report["newly_reachable_inputs"] = [c["travel"] for c in report["cases"]
                                               if c["stunned"] and not original[c["travel"]]["stunned"]]
            assert report["newly_reachable_inputs"], "No real reach improvement over the same baseline inputs"
        report["passed"] = True
    except BaseException as error:
        report["error"] = repr(error)
        raise
    finally:
        probe.call("clear_input")
        for snapshot in snapshots:
            probe.call("set_snapshot", slot=snapshot["slot"],
                       addr=snapshot["addr"] if snapshot["active"] else "0x0")
        probe.savestate("load", 8)
        (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=4598)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--baseline", type=Path)
    args = parser.parse_args()
    result = run(Probe(args.port), args.output, args.baseline)
    print({"passed": result["passed"], "hits": [c["travel"] for c in result["cases"] if c["stunned"]],
           "newly_reachable_inputs": result.get("newly_reachable_inputs"), "distant_miss": result["distant_miss"]})
