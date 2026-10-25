"""Live US Mario's Hat smoke checks. Run only against an isolated private-card runtime.

Requires a quiet, grounded Tomba gameplay fixture in --tomba-slot (default 7).
Writes build-specific fixtures in --mario-slot/--scratch-slot (default 8/9).
Screenshots require visual review; this does not certify enemy interaction,
progression, memory-card saves, rewind or release readiness.
"""
import argparse
import hashlib
import json
import struct
import time
from pathlib import Path

from mario_probe import Probe


def run(probe, output, tomba_slot, mario_slot, scratch_slot):
    output.mkdir(parents=True, exist_ok=True)
    report = {"checks": [], "observations": [], "unverified": [
        "enemy damage and weapon-specific target requirements", "overhead grab/aim/throw",
        "other suits", "death/continue", "native memory-card save/reload",
        "rewind audio", "matched baseline performance", "adaptive widescreen",
        "native picker/import UI", "redistributable packaging"]}

    def observe(label):
        record = {"label": label, "state": probe.state(), "player": probe.player()}
        report["observations"].append(record)
        return record

    def check(name, condition):
        report["checks"].append({"name": name, "passed": bool(condition)})
        if not condition:
            raise AssertionError(name)

    def count(name):
        return next((c["count"] for c in probe.counters()["counters"] if c["name"] == name), 0)

    def wait_grounded():
        for _ in range(20):
            player = probe.player()
            if player["mode"] == 1 and player["action"] in (0, 1) and player["air"] == 0:
                return
            probe.wait_frames(20)
        raise AssertionError("Did not return to grounded gameplay")

    def choose_hat():
        probe.tap(0x1000, release=24)
        menu = probe.state()["menu"]
        check("native inventory has opened", menu != 0 and probe.ram(menu, 1) == b"\1")
        probe.tap(0x4000)
        # This fixture owns exactly two ordinary equipment entries. Fail if
        # the menu layout differs instead of equipping an unknown native item.
        check("fixture has two native equipment entries", probe.state()["menu_primary"] == 2)
        probe.tap(0x0040)
        selection = probe.ram(menu, 48)
        check("virtual hat is selected before confirmation", selection[9] == 1 and
              selection[0x1d] == 0 and selection[0x1f] == 1)
        probe.tap(0x4000)

    snapshot = None
    try:
        probe.wait_game_ready()
        probe.savestate("load", tomba_slot)
        probe.wait_frames(24)
        start = observe("Tomba fixture")
        check("fixture starts as grounded Tomba", not start["state"]["equipped"] and
              not start["state"]["requested"] and start["player"]["air"] == 0)
        ownership = probe.ram(0x800bfab4, 256)
        report["ownership_sha256"] = hashlib.sha256(ownership).hexdigest()
        label_before = count("tomba2.mario.costume_label")
        choose_hat()
        check("hat selection requests native costume transaction", probe.state()["requested"] == 1)
        probe.screenshot(output / "inventory-equipped.png")
        probe.tap(0x1000, release=25)
        probe.screenshot(output / "equip-confirmation.png")
        check("one native hat confirmation queued", count("tomba2.mario.costume_label") == label_before + 1)
        wait_grounded()
        check("native costume transaction equips Mario", probe.state()["equipped"] == 1)
        probe.screenshot(output / "mario-gameplay.png")
        probe.savestate("save", mario_slot)
        jumps = count("tomba2.mario.double_jump")
        native_voices = count("tomba2.mario.native_voice_replaced")
        probe.tap(0x4000, release=6)
        first = observe("first jump")
        check("first jump follows native airborne state", first["player"]["air"] != 0 and
              first["state"]["double_jump"] == 0)
        check("native Tomba jump vocal is replaced through its real sound event",
              count("tomba2.mario.native_voice_replaced") > native_voices)
        probe.tap(0x4000, release=4)
        second = observe("second jump")
        check("second jump applies upward velocity once", second["state"]["double_jump"] == 1 and
              second["player"]["velocity_y"] < 0 and count("tomba2.mario.double_jump") == jumps + 1)
        probe.tap(0x4000, release=4)
        observe("third press")
        check("third airborne press adds no jump", count("tomba2.mario.double_jump") == jumps + 1)
        probe.wait_frames(100)
        wait_grounded()
        check("landing restores extra jump", probe.state()["double_jump"] == 0)
        probe.tap(0x4000, release=6)
        probe.tap(0x4000, release=4)
        check("next airborne cycle permits another extra jump", count("tomba2.mario.double_jump") == jumps + 2)
        probe.wait_frames(100)
        wait_grounded()
        # Submit the save as soon as the actual extra-jump voice starts. Its
        # 311 ms duration can end while a completed route/save receipt and
        # several live observations are travelling through the debug server.
        memory = probe.state()["address"]
        snapshot = probe.call("get_snapshots")["snapshots"][0]
        probe.call("set_snapshot", slot=0, addr=hex(memory))
        generation = probe.call("savestate_status")["generation"]
        probe.start_route([(4, 0x4000), (4, 0), (4, 0x4000), (80, 0)])
        deadline = time.monotonic() + probe.timeout
        while True:
            words = struct.unpack("<16I", probe.ram(memory, 64))
            if words[9] == 436:
                report["voice_save_request"] = {"voice": words[9], "cursor": words[10],
                                                "fraction": words[11]}
                probe.call("savestate", op="save", slot=scratch_slot)
                break
            if time.monotonic() >= deadline or not probe.call("input_route_status")["active"]:
                raise AssertionError("Native extra-jump voice did not start during the input route")
        report["voice_save_receipt"] = probe.wait_savestate("save", scratch_slot, generation)
        probe.wait_route()
        observe("mid-voice save receipt (live, may already have completed)")
        start = probe.call("history")["newest"] + 1
        probe.savestate("load", scratch_slot)
        # The completion receipt can arrive at a BB before the first restored
        # VBlank has entered the history ring. Retain a few actual frames.
        probe.wait_frames(4)
        end = probe.call("history")["newest"]
        recorded = []
        for frame in range(start, end + 1):
            raw = probe.call("read_frame_ram", frame=frame, addr=hex(memory), len=64)
            words = struct.unpack("<16I", bytes.fromhex(raw["hex"]))
            if words[9] == 436:
                recorded.append({"frame": frame, "equipped": words[2], "voice": words[9],
                                 "cursor": words[10], "fraction": words[11]})
        report["recorded_voice_restore"] = recorded
        check("mid-voice fixture contains an active imported voice", bool(recorded))
        check("current-build load restores Mario and active voice", recorded[0]["equipped"] == 1 and
              recorded[0]["cursor"] <= report["voice_save_request"]["cursor"] + 2000 and
              recorded[0]["fraction"] < 44100)
        observe("mid-voice load receipt (live, may already have completed)")
        probe.wait_frames(100)
        wait_grounded()
        check("restored voice completes", probe.state()["voice"] == 0)
        choose_hat()
        check("hat toggle requests Tomba", probe.state()["requested"] == 0)
        probe.tap(0x1000, release=25)
        probe.screenshot(output / "remove-confirmation.png")
        wait_grounded()
        check("native costume transaction restores Tomba", probe.state()["equipped"] == 0)
        probe.screenshot(output / "tomba-restored.png")
        check("native ownership bytes preserved", probe.ram(0x800bfab4, 256) == ownership)
        probe.savestate("load", mario_slot)
        probe.wait_frames(8)
        observe("final Mario fixture")
        report["passed"] = True
    except Exception as error:
        report["passed"] = False
        report["error"] = str(error)
        raise
    finally:
        try:
            probe.call("clear_input")
            if snapshot is not None:
                probe.call("set_snapshot", slot=0,
                           addr=snapshot["addr"] if snapshot["active"] else "0x0")
            report["counters"] = probe.counters()
        except Exception as error:
            report["cleanup_error"] = str(error)
        finally:
            (output / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=4597)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--tomba-slot", type=int, default=7)
    parser.add_argument("--mario-slot", type=int, default=8)
    parser.add_argument("--scratch-slot", type=int, default=9)
    args = parser.parse_args()
    result = run(Probe(args.port), args.output, args.tomba_slot, args.mario_slot, args.scratch_slot)
    print(f"PASS: {len(result['checks'])} live smoke assertions; screenshots in {args.output}")
