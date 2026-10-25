"""Native suit exclusivity fixture; acquisition is bypassed in private RAM only.

Run against the isolated US debug runtime with Mario fixture slot 8. Native
inventory equip and transformation perform both swaps. Restores the original
fixture/ownership on completion; never writes a native memory-card save.
"""
import argparse
import json
from pathlib import Path
from mario_probe import Probe


def run(probe, item, output):
    output.mkdir(parents=True, exist_ok=True)
    report = {"item": item, "acquisition_bypassed": True, "checks": []}
    def check(name, passed):
        report["checks"].append({"name": name, "passed": bool(passed)})
        if not passed:
            raise AssertionError(name)
    def choose(flat):
        probe.tap(0x1000, release=24)
        state = probe.state()
        check("fixture has three native equipment entries", state["menu_primary"] == 3)
        # Fix only the granted item's sort position; the native builder has not
        # run a pickup transaction for this cheat. Existing item order stays.
        probe.write(0x800bfbb4+item, b"\2")
        probe.tap(0x4000)
        probe.tap(0x0040)
        if flat%2:
            probe.tap(0x0020)
        selected = probe.ram(state["menu"], 48)
        check("expected item selected before confirmation", selected[0x1f] == 1 and selected[0x1d] == flat%2)
        probe.screenshot(output/"selected.png")
        probe.tap(0x4000)
        probe.tap(0x1000, release=25)

    probe.wait_game_ready()
    probe.savestate("load", 8)
    probe.wait_frames(8)
    ownership = probe.ram(0x800bfab4, 256)
    try:
        check("starts as Mario", probe.state()["equipped"] == 1)
        probe.write(0x800bfab4+item, b"\1")
        choose(2)
        probe.wait_frames(220)
        report["native_suit"] = {"state": probe.state(), "player": probe.player()}
        check("native suit restores Tomba", report["native_suit"]["state"]["equipped"] == 0 and
              report["native_suit"]["state"]["requested"] == 0 and
              report["native_suit"]["player"]["costume"] == item)
        probe.screenshot(output/"native-suit.png")
        choose(3)
        probe.screenshot(output/"hat-confirmation.png")
        probe.wait_frames(220)
        report["hat"] = {"state": probe.state(), "player": probe.player()}
        check("hat restores Mario from native suit", report["hat"]["state"]["equipped"] == 1 and
              report["hat"]["state"]["requested"] == 1 and report["hat"]["player"]["costume"] == 0)
        check("hat confirmation was consumed", report["hat"]["state"]["hat_toast"] == 0)
        probe.screenshot(output/"mario-restored.png")
        report["passed"] = True
    except Exception as error:
        report["passed"] = False
        report["error"] = str(error)
        raise
    finally:
        probe.call("clear_input")
        probe.savestate("load", 8)
        probe.wait_frames(8)
        report["ownership_restored"] = probe.ram(0x800bfab4, 256) == ownership
        (output/"report.json").write_text(json.dumps(report, indent=2)+"\n", encoding="utf-8")
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--item", type=int, choices=range(14, 23), default=15)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--port", type=int, default=4597)
    args = parser.parse_args()
    report = run(Probe(args.port), args.item, args.output)
    if not report["ownership_restored"]:
        raise SystemExit("Fixture ownership was not restored")
    print(f"PASS: {len(report['checks'])} native suit/hat assertions; acquisition bypassed")
