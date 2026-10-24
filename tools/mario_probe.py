"""Non-pausing development probe for an isolated Tomba 2 mod test runtime."""
import argparse
import json
import socket
import struct
import time


def request(port, command, **params):
    with socket.create_connection(("127.0.0.1", port), timeout=20) as connection:
        stream = connection.makefile("rwb")
        stream.write((json.dumps({"id": 1, "cmd": command, **params})+"\n").encode())
        stream.flush()
        response = json.loads(stream.readline())
        if not response.get("ok"):
            raise RuntimeError(response)
        return response


class Probe:
    """Frame-counted input on a live runtime; never pauses or focuses a window.

    Use only with a development runtime and private memory cards. State slots
    are build-specific fixtures. RAM reads are live observations, not atomic
    snapshots; use the runtime's frame history for exact frame observations.
    """

    def __init__(self, port=4597, timeout=30):
        self.port, self.timeout = port, timeout

    def call(self, command, **params):
        return request(self.port, command, **params)

    def ram(self, address, size):
        return bytes.fromhex(self.call("read_ram", addr=address, len=size)["hex"])

    def write(self, address, data):
        self.write_fields([(address, data)])

    def write_fields(self, fields):
        # The server closes each connection after one command. These are live
        # byte writes; native physics may update fields between requests.
        for address, data in fields:
            for offset, value in enumerate(data):
                self.call("write_ram", addr=address+offset, val=value)

    def counters(self):
        return self.call("mod_counters")

    def wait_game_ready(self, timeout=60):
        # A restore before the host has entered MAIN.EXE can leave boot/title
        # host context active while RAM already looks like gameplay. Wait for
        # the real entry and US MAIN function identity before loading fixtures.
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            try:
                if self.call("turbo_loads")["game_started"] and self.ram(0x800368d0, 4) == bytes.fromhex("24008390"):
                    return
            except OSError:
                pass
            time.sleep(0.05)
        raise TimeoutError("US MAIN.EXE did not become ready before fixture restore")

    def state(self):
        counters = self.counters()
        addresses = [c["count"] for c in counters["counters"]
                     if c["name"] == "tomba2.mario.state_address"]
        if len(addresses) != 1:
            raise RuntimeError("Mario plugin is not active")
        address = addresses[0]
        words = struct.unpack("<16I", self.ram(address, 64))
        names = ("magic", "requested", "equipped", "double_jump", "clip", "clock",
                 "last_action", "menu_primary", "icon_item", "voice", "voice_cursor",
                 "voice_fraction", "previous_action", "previous_air", "menu", "hat_toast")
        return {"address": address, "frame": counters["frame"], **dict(zip(names, words))}

    def player(self):
        data = self.ram(0x800e7e80, 0x180)
        return {"mode": data[4], "action": data[5], "phase": data[6],
                "position": list(struct.unpack_from("<3i", data, 0x2c)),
                "velocity_y": struct.unpack_from("<h", data, 0x4a)[0],
                "heading": struct.unpack_from("<H", data, 0x56)[0],
                "costume": data[0x6f], "ground": data[0x29], "air": data[0x145]}

    def actors(self):
        """Native active lists (not an enemy classifier); +0x24 is next."""
        ram = self.ram(0x80000000, 0x200000)
        def word(address):
            return struct.unpack_from("<I", ram, address & 0x1fffff)[0]
        result = []
        for group, head in enumerate((0x800fb168, 0x800f2624, 0x800f2738)):
            address, seen = word(head), set()
            while address:
                if address in seen or len(seen) >= 256 or not 0x80010000 <= address <= 0x801fff40:
                    raise RuntimeError(f"Invalid actor list {group}: {address:#x}")
                seen.add(address)
                offset = address & 0x1fffff
                result.append({"address": address, "group": group, "handler": word(address+0x1c),
                               "flags": ram[offset:offset+16].hex(),
                               "position": list(struct.unpack_from("<3i", ram, offset+0x2c))})
                address = word(address+0x24)
        return result

    def route(self, steps):
        """Steps are (VBlank count, pressed-button mask); zero means release."""
        self.call("clear_input")
        self.call("input_route_clear")
        for frames, pressed in steps:
            if not 0 < frames <= 36000 or not 0 <= pressed <= 0xffff:
                raise ValueError("Invalid input route step")
            self.call("input_route_append", frames=frames, buttons=0xffff ^ pressed)
        receipt = self.call("input_route_start")
        deadline = time.monotonic() + self.timeout
        try:
            while self.call("input_route_status")["active"]:
                if time.monotonic() >= deadline:
                    raise TimeoutError("Input route did not complete")
                time.sleep(0.025)
        finally:
            self.call("clear_input")
        return {**receipt, "end_frame": self.counters()["frame"]}

    def wait_frames(self, frames):
        return self.route([(frames, 0)])

    def tap(self, pressed, frames=4, release=8):
        return self.route([(frames, pressed), (release, 0)])

    def savestate(self, operation, slot):
        generation = self.call("savestate_status")["generation"]
        self.call("savestate", op=operation, slot=slot)
        deadline = time.monotonic() + self.timeout
        while True:
            status = self.call("savestate_status")
            if status["generation"] > generation and not status["pending"]:
                if not status["last_ok"] or status["last_op"] != operation or status["last_slot"] != slot:
                    raise RuntimeError(status)
                return status
            if time.monotonic() >= deadline:
                raise TimeoutError("Save-state transaction did not complete")
            time.sleep(0.025)

    def screenshot(self, path):
        from pathlib import Path
        return self.call("screenshot_hires", path=str(Path(path).resolve()))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command")
    parser.add_argument("--port", type=int, default=4597)
    parser.add_argument("--path")
    parser.add_argument("--addr", type=lambda v: int(v, 0))
    parser.add_argument("--len", type=int)
    parser.add_argument("--buttons", type=lambda v: int(v, 0))
    parser.add_argument("--slot", type=int)
    args = vars(parser.parse_args())
    command, port = args.pop("command"), args.pop("port")
    print(json.dumps(request(port, command, **{k: v for k, v in args.items() if v is not None}), indent=2))
