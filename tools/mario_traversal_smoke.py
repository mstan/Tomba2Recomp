"""Private US chest/post/ladder presentation fixtures; no RAM or progress writes.

Requires chest perch slot1, native post hang slot4, house mid-ladder slot5.
These development-only snapshots bypass travel; they do not certify acquisition.
Never run against an owner-facing process or its memory cards.
"""
import argparse
import json
import struct
from pathlib import Path

from mario_probe import Probe

PLAYER = 0x800e7e80


def run(probe, output):
    output.mkdir(parents=True, exist_ok=True)
    report = {"checks": [], "observations": [], "ram_writes": False,
              "travel_bypassed_by_private_fixtures": True}

    def check(name, condition):
        report["checks"].append({"name": name, "passed": bool(condition)})
        if not condition:
            raise AssertionError(name)

    def observe(label, screenshot=False):
        actor, state = probe.player(), probe.state()
        report["observations"].append({"label": label, "player": actor, "mario": state})
        check(label + " has live Mario", actor["mode"] == 1 and state["equipped"] == 1)
        if screenshot:
            probe.composed_screenshot(output / (label + ".png"))
        return actor, state

    def restore(slot):
        probe.savestate("load", slot)
        # Let restored RAM reach the renderer before queuing a composed shot.
        probe.wait_frames(8)

    try:
        probe.wait_game_ready(90)
        restore(1)
        chest, state = observe("chest-perch", True)
        check("native chest perch selects Smash balance", chest["action"] == 3 and
              chest["phase"] == 9 and state["clip"] == 520)
        probe.route([(35, 0)])
        held, state = observe("chest-rest")
        check("rest stays supported", held["position"] == chest["position"] and state["clip"] == 520)
        probe.route([(4, 0x4000), (12, 0)])
        jumped, state = observe("chest-jump")
        check("native spring leaves chest with jump pose", jumped["air"] == 1 and
              jumped["position"][1] < chest["position"][1] and state["clip"] == 509)

        restore(4)
        hang, state = observe("post-hang", True)
        check("native post hang selects Smash CliffWait", hang["action"] == 4 and
              hang["phase"] == 1 and state["clip"] == 573)
        probe.route([(25, 0)])
        held, state = observe("post-hang-rest")
        check("hanging remains attached", held["action"] == 4 and state["clip"] == 573)
        probe.route([(4, 0x4000), (28, 0)])
        jumped, state = observe("post-release")
        check("native release leaves hanging pose", jumped["action"] != 4 and state["clip"] != 573)

        restore(5)
        start, state = observe("ladder-step-a", True)
        check("native sustained climb selected", start["action"] == 0xa and
              start["phase"] == 12 and state["clip"] == 573)
        probe.route([(10, 0)])
        step, state = observe("ladder-step-b", True)
        check("native ladder movement continues", step["action"] == 0xa and
              step["phase"] == 12 and step["position"][1] < start["position"][1])
        # Current-build restore must also restore the animation clock.
        probe.savestate("save", 11)
        saved = probe.state()
        probe.route([(7, 0)])
        probe.savestate("load", 11)
        restored = probe.state()
        check("current-build ladder state restores", restored["clip"] == saved["clip"] and
              abs(restored["clock"] - saved["clock"]) <= 8)
        probe.route([(140, 0)])
        end, state = observe("ladder-exit", True)
        check("native climb completes on roof", end["action"] == 0 and end["air"] == 0 and
              end["position"][1] < start["position"][1] - 300 * 65536 and state["clip"] == 499)
        check("climb preserves health", struct.unpack("<h", probe.ram(PLAYER + 0x16e, 2))[0] > 0)
        report["passed"] = True
    except BaseException as error:
        report["error"] = repr(error)
        raise
    finally:
        probe.call("clear_input")
        (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=4598)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = run(Probe(args.port, 75), args.output)
    print({"passed": result["passed"], "checks": len(result["checks"])})
