"""Real hit/stun and capture checks against the private US Mario runtime.

Requires the quiet Fisherman's-house Mario fixture (slot 8 by default), with
Blackjack equipped. Builds a nearby fixture by walking; no actor position,
health, capture or success flags are written. Uses two disposable state slots.
The post pig is a special native actor: this does not certify every target's throwing,
damage amounts, weapon-specific targets or progression.
"""
import argparse
import json
import struct
from pathlib import Path

from mario_probe import Probe


PLAYER = 0x800e7e80
PIG = 0x800fd430
PIG_HANDLER = 0x80145230


def actor(data):
    return {"flags": data[:16].hex(), "mode": data[4], "action": data[5],
            "phase": data[6], "link": struct.unpack_from("<I", data, 0x10)[0],
            "handler": struct.unpack_from("<I", data, 0x1c)[0],
            "position": list(struct.unpack_from("<3i", data, 0x2c))}


def run(probe, output, mario_slot, near_slot, capture_slot, weapon=0):
    if len({mario_slot, near_slot, capture_slot}) != 3:
        raise ValueError("Input and disposable fixture slots must differ")
    output.mkdir(parents=True, exist_ok=True)
    report = {"checks": [], "cases": [], "weapon": weapon,
              "acquisition_bypassed": weapon != 0, "unverified": [
        "damage amounts and weapon-specific target requirements",
        "hold/aim/throw for other target classes", "acquisition and progression gates"]}
    snapshots = []

    def check(name, condition):
        report["checks"].append({"name": name, "passed": bool(condition)})
        if not condition:
            raise AssertionError(name)

    def counters():
        return {c["name"]: c["count"] for c in probe.counters()["counters"]}

    def trace(label, steps):
        start = probe.call("history")["newest"] + 1
        before = counters()
        receipt = probe.route(steps)
        end = probe.call("history")["newest"]
        rows = []
        # These are recorded frame observations, not a series of live reads.
        for frame in range(start, end + 1, 2):
            def read(address):
                return bytes.fromhex(probe.call("read_frame_ram", frame=frame,
                                               addr=hex(address), len=128)["hex"])
            rows.append({"frame": frame, "player": actor(read(PLAYER)),
                         "pig": actor(read(PIG))})
        after = counters()
        result = {"label": label, "receipt": receipt, "frames": rows,
                  "counter_delta": {k: v - before.get(k, 0) for k, v in after.items()
                                    if v != before.get(k, 0)}}
        report["cases"].append(result)
        probe.screenshot(output / (label + ".png"))
        return result

    def restore(slot):
        probe.savestate("load", slot)
        probe.wait_frames(8)
        check("fixture has Mario equipped", probe.state()["equipped"] == 1)
        pig = actor(probe.ram(PIG, 128))
        check("verified post-pig identity and initial state", pig["handler"] == PIG_HANDLER
              and pig["mode"] == 1 and pig["action"] == 0)

    def stunned(case):
        return any(row["pig"]["flags"][:2] in ("03", "07") and
                   row["pig"]["mode"] == 2 for row in case["frames"])

    try:
        probe.wait_game_ready()
        snapshots = probe.call("get_snapshots")["snapshots"]
        for slot, address in enumerate((PLAYER, PIG)):
            probe.call("set_snapshot", slot=slot, addr=hex(address))
        restore(mario_slot)
        check("fixture starts outside Fisherman's house", all(abs(a-b) < 64*65536
              for a, b in zip(probe.player()["position"], (248643584, -67633152, 260046848))))
        check("fixture uses the native Blackjack", probe.ram(PLAYER+0x6c, 1) == b"\0")
        ownership = probe.ram(0x800bfab4, 256)
        if weapon:
            # Only acquisition/order are bypassed, in private RAM. The menu
            # performs the actual equip and native weapon-family selection.
            check("weapon grant starts with two equipment entries", probe.state()["menu_primary"] == 2)
            probe.write(0x800bfab4+weapon, b"\1")
            probe.tap(0x1000, release=24)
            probe.write(0x800bfbb4+weapon, b"\2")
            menu = probe.state()["menu"]
            check("granted weapon adds one equipment entry", probe.state()["menu_primary"] == 3)
            probe.tap(0x4000)
            probe.tap(0x40)
            selection = probe.ram(menu, 48)
            check("granted weapon selected before confirming", selection[0x1f] == 1
                  and selection[0x1d] == 0)
            probe.tap(0x4000)
            probe.tap(0x1000, release=100)
            check("native equip selects the granted weapon", probe.ram(PLAYER+0x6c, 1) == bytes([weapon])
                  and probe.state()["equipped"] == 1)
        probe.route([(100, 0x20), (12, 0), (78, 0x20), (30, 0)])
        check("walk reaches the post approach", 5750 < probe.player()["position"][0]/65536 < 5900)
        probe.savestate("save", near_slot)

        restore(near_slot)
        approach = [(12, 0x20), (4, 0x4020), (8, 0x20)]
        control = trace("no-attack", approach + [(4, 0x20), (45, 0)])
        check("same approach without attack does not stun pig", not stunned(control)
              and all(row["pig"]["mode"] == 1 for row in control["frames"]))

        restore(near_slot)
        hit = trace("punch-hit", approach + [(4, 0x2020), (45, 0)])
        check("punch produces a real native pig stun", stunned(hit))
        check("hit has one fist spawn and balanced retirement",
              hit["counter_delta"].get("tomba2.mario.punch_spawn") == 1 and
              hit["counter_delta"].get("tomba2.mario.punch_retired") == 1)

        restore(near_slot if weapon else mario_slot)
        if weapon:
            # Walk away from the post without changing the equipped family.
            probe.route([(100, 0x80), (30, 0)])
        miss = trace("out-of-range", [(4, 0x2000), (70, 0)])
        check("out-of-range punch does not stun pig", not stunned(miss)
              and all(row["pig"]["mode"] == 1 for row in miss["frames"]))
        check("out-of-range fist retires", miss["counter_delta"].get("tomba2.mario.punch_spawn") == 1
              and miss["counter_delta"].get("tomba2.mario.punch_retired") == 1)

        # The pig alternates its spear attack. Search a bounded set of real
        # input timings rather than granting invulnerability or altering it.
        report["capture_attempts"] = []
        capture = None
        for delay in range(0, 97, 8):
            restore(near_slot)
            if delay:
                probe.wait_frames(delay)
            steps = [(5, 0x4000), (11, 0), (12, 0x20),
                     (4, 0x4020), (32, 0x20), (8, 0)]
            attempt = trace(f"capture-delay-{delay}", steps)
            settled = probe.player()
            report["capture_attempts"].append({"delay": delay, "player": settled})
            if settled["action"] == 3 and settled["phase"] == 2:
                capture = attempt
                break
        check("bounded native input route reaches capture", capture is not None)
        check("real jump enters native capture and links the pig", any(
            row["player"]["action"] == 3 and row["player"]["link"] == PIG
            and row["pig"]["mode"] == 2 and row["pig"]["action"] == 1
            for row in capture["frames"]))
        check("native capture settles while Mario remains equipped",
              probe.player()["action"] == 3 and probe.player()["phase"] == 2
              and probe.state()["equipped"] == 1)
        probe.savestate("save", capture_slot)
        probe.savestate("load", capture_slot)
        probe.wait_frames(8)
        restored = actor(probe.ram(PLAYER, 128))
        check("current-build load restores the real capture link", restored["action"] == 3
              and restored["link"] == PIG and probe.state()["equipped"] == 1)
        probe.screenshot(output / "capture-restored.png")
        held = trace("capture-hold", [(36, 0)])
        check("native capture holds without input", all(
            row["player"]["action"] == 3 and row["player"]["phase"] == 2
            and row["player"]["link"] == PIG and row["pig"]["mode"] == 2
            and row["pig"]["action"] == 1 for row in held["frames"]))
        check("overhead model rendering actually ran", held["counter_delta"].get(
            "tomba2.mario.overhead_draw", 0) > 0)
        check("presentation retains native captured actor position", all(
            row["player"]["position"][0] == row["pig"]["position"][0] and
            row["player"]["position"][2] == row["pig"]["position"][2] and
            row["pig"]["position"][1] - row["player"]["position"][1] == 100*65536
            for row in held["frames"]))
        trace("capture-aim-left", [(12, 0x80), (8, 0)])
        check("native aim turns left while holding", probe.player()["heading"] == 2048
              and probe.player()["action"] == 3 and probe.player()["phase"] == 2)
        trace("capture-aim-right", [(12, 0x20), (8, 0)])
        check("native aim turns right while holding", probe.player()["heading"] == 0
              and probe.player()["action"] == 3 and probe.player()["phase"] == 2)
        thrown = trace("capture-throw", [(4, 0x4000), (160, 0)])
        check("Cross executes native throw and release", any(
            row["player"]["action"] == 3 and row["player"]["phase"] == 6
            for row in thrown["frames"]) and any(row["pig"]["mode"] == 0
                                                  for row in thrown["frames"]))
        check("native throw clears captured target kind", probe.ram(PLAYER+0x78, 1) == b"\0"
              and probe.state()["equipped"] == 1)
        expected = bytearray(ownership)
        if weapon:
            expected[weapon] = 1
        check("native ownership bytes preserved apart from declared grant",
              probe.ram(0x800bfab4, 256) == expected)
        report["passed"] = True
    except Exception as error:
        report["passed"] = False
        report["error"] = str(error)
        raise
    finally:
        try:
            probe.call("clear_input")
            for snapshot in snapshots:
                probe.call("set_snapshot", slot=snapshot["slot"],
                           addr=snapshot["addr"] if snapshot["active"] else "0x0")
            probe.savestate("load", mario_slot)
            if "ownership" in locals():
                report["ownership_restored"] = probe.ram(0x800bfab4, 256) == ownership
        finally:
            (output / "report.json").write_text(json.dumps(report, indent=2)+"\n", encoding="utf-8")
    return report


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", type=int, default=4597)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--mario-slot", type=int, default=8)
    parser.add_argument("--near-slot", type=int, default=10)
    parser.add_argument("--capture-slot", type=int, default=11)
    parser.add_argument("--weapon", type=int, choices=range(9), default=0)
    args = parser.parse_args()
    result = run(Probe(args.port), args.output, args.mario_slot, args.near_slot, args.capture_slot, args.weapon)
    if not result.get("ownership_restored"):
        raise SystemExit("Native ownership was not restored")
    print(f"PASS: {len(result['checks'])} interaction assertions; inspect captures in {args.output}")
