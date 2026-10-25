"""Reload an existing private native save through Tomba 2's actual Load menu.

Start in grounded gameplay. The expected JSON is the pre-save observation from
the isolated card test, not a savestate. This never writes RAM or card files
directly; the native BIOS may write the card's system test frame (sector 63).
Screenshots of each menu must also be reviewed. A fresh-process run requires
zero savestate transactions and unequipped Mario at entry.
"""
import argparse
import hashlib
import json
from pathlib import Path
import time

from mario_probe import Probe


def run(probe, output, expected_path, cards, fresh_process=False):
    expected = json.loads(expected_path.read_text(encoding="utf-8"))
    output.mkdir(parents=True, exist_ok=True)
    report = {"checks": [], "menus": [], "fresh_process": fresh_process,
              "ram_writes": False, "savestate_loads": False}

    def check(name, condition):
        report["checks"].append({"name": name, "passed": bool(condition)})
        if not condition:
            raise AssertionError(name)

    def card_hashes():
        result = {}
        for name in ("card1.mcd", "card2.mcd"):
            data = (cards / name).read_bytes()
            check(name + " is a full raw card", len(data) == 131072 and data[:2] == b"MC")
            result[name] = hashlib.sha256(data).hexdigest()
        return result

    def menu(label, steps):
        receipt = probe.route(steps)
        probe.screenshot(output / (label + ".png"))
        report["menus"].append({"label": label, "route": steps, "receipt": receipt})

    try:
        probe.wait_game_ready()
        report["entry_player"], report["entry_mario"] = probe.player(), probe.state()
        player = report["entry_player"]
        check("starts in grounded gameplay", player["mode"] == 1 and
              player["action"] in (0, 1) and player["air"] == 0)
        report["snapshot_entry"] = probe.call("savestate_status")
        if fresh_process:
            check("fresh process has never restored a snapshot", report["snapshot_entry"]["generation"] == 0)
            check("fresh process has no Mario selection", not report["entry_mario"]["requested"]
                  and not report["entry_mario"]["equipped"])
        report["cards_before"] = card_hashes()
        for name in report["cards_before"]:
            (output / (name + ".before")).write_bytes((cards / name).read_bytes())
        card1 = (cards / "card1.mcd").read_bytes()
        check("private file 01 exists in block 1", card1[128] == 0x51 and
              card1[138:159].split(b"\0", 1)[0] == b"BASCUSP94454T2-00")

        # Start / Down selects Load data. The remaining confirmations choose
        # memory-card slot 1, native file 01 and its default Yes confirmation.
        menu("load-selected", [(4, 8), (20, 0), (4, 0x40), (10, 0)])
        menu("card-slot", [(4, 0x4000), (80, 0)])
        menu("native-files", [(4, 0x4000), (120, 0)])
        menu("load-confirm", [(4, 0x4000), (30, 0)])
        probe.tap(0x4000, 4, 120)
        deadline = time.monotonic() + 60
        while time.monotonic() < deadline:
            player = probe.player()
            if (player["mode"] == 1 and player["action"] == 0 and not player["air"]
                    and player["position"] == expected["player"]["position"]):
                break
            probe.wait_frames(60)
        else:
            raise TimeoutError("Native load did not return to the saved position")
        probe.wait_frames(30)
        probe.screenshot(output / "loaded.png")
        report["loaded_player"], report["loaded_mario"] = probe.player(), probe.state()
        check("native load restores saved location", report["loaded_player"]["position"] ==
              expected["player"]["position"])
        check("native ownership matches saved progress", probe.ram(0x800bfab4, 256).hex() ==
              expected["native_ownership"])
        check("native costume remains valid", report["loaded_player"]["costume"] ==
              expected["player"]["costume"])
        check("native load resets airborne extra jump", not report["loaded_mario"]["double_jump"])
        if fresh_process:
            check("native save does not embed private Mario state", not report["loaded_mario"]["requested"]
                  and not report["loaded_mario"]["equipped"])
        old_position = probe.player()["position"]
        probe.route([(10, 0x20), (15, 0)])
        check("loaded gameplay responds to movement", probe.player()["position"] != old_position)
        report["cards_after"] = card_hashes()
        report["card_changed_sectors"] = {}
        for name in report["cards_after"]:
            before = (output / (name + ".before")).read_bytes()
            after = (cards / name).read_bytes()
            (output / (name + ".after")).write_bytes(after)
            report["card_changed_sectors"][name] = [i for i in range(1024)
                if before[i*128:(i+1)*128] != after[i*128:(i+1)*128]]
        # The real card test writes frame 63. Check every other sector,
        # including all directory and saved-game sectors, byte for byte.
        # runtime/src/memcard.c identifies frame 63 as the write-test frame.
        check("loading preserves all native progress sectors", all(
            set(changed) <= {63} for changed in report["card_changed_sectors"].values()))
        check("unused card 2 is unchanged", not report["card_changed_sectors"]["card2.mcd"])
        report["dispatch"] = probe.call("unknown_dispatch_log", limit=8)
        check("no unresolved dispatches", report["dispatch"]["total"] == 0)
        report["snapshot_exit"] = probe.call("savestate_status")
        check("no savestate transaction was used", report["snapshot_exit"]["generation"] ==
              report["snapshot_entry"]["generation"])
        report["passed"] = True
    except Exception as error:
        report.update(passed=False, error=str(error))
        raise
    finally:
        try:
            probe.call("clear_input")
        finally:
            (output / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=4597)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--expected", type=Path, required=True)
    parser.add_argument("--cards", type=Path, required=True)
    parser.add_argument("--fresh-process", action="store_true")
    args = parser.parse_args()
    result = run(Probe(args.port), args.output, args.expected, args.cards, args.fresh_process)
    print(f"PASS: {len(result['checks'])} native reload assertions; review {args.output}")
