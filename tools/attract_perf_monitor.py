#!/usr/bin/env python3
"""Sample guest/video/audio health through Tomba 2's unattended attract loop.

The debug rings are always-on; this observer only takes periodic snapshots and
optional screenshots. Output is JSONL so slow intervals can be correlated with
the exact captured frame without keeping a TCP connection open.
"""
import argparse
import json
import os
import socket
import time


def call(port, cmd, **fields):
    request = {"id": 1, "cmd": cmd, **fields}
    with socket.create_connection(("127.0.0.1", port), timeout=10.0) as sock:
        sock.sendall((json.dumps(request) + "\n").encode())
        data = b""
        while not data.endswith(b"\n"):
            chunk = sock.recv(1 << 16)
            if not chunk:
                break
            data += chunk
    reply = json.loads(data.decode().strip())
    if not reply.get("ok"):
        raise RuntimeError(f"{cmd}: {reply}")
    return reply


def runtime_path(path):
    """Return a path the native Windows runtime can pass to fopen().

    The MinGW/MSYS Python selected by this workspace reports absolute paths as
    /f/Projects/..., which is meaningful inside MSYS but not to a native Win32
    process. Convert that drive-prefix form without requiring a cygpath child.
    """
    path = os.path.abspath(path)
    if len(path) >= 3 and path[0] == "/" and path[1].isalpha() and path[2] == "/":
        path = f"{path[1].upper()}:{path[2:]}"
    return path.replace("\\", "/")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", type=int, default=4515)
    ap.add_argument("--duration", type=float, default=600.0)
    ap.add_argument("--interval", type=float, default=5.0)
    ap.add_argument("--screenshot-every", type=float, default=20.0)
    ap.add_argument("--screenshot-delay", type=float, default=0.0)
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    os.makedirs(args.out, exist_ok=True)
    jsonl_path = os.path.join(args.out, "samples.jsonl")
    started = time.monotonic()
    next_sample = started
    next_shot = started + args.screenshot_delay
    previous = None
    previous_pc = {}

    with open(jsonl_path, "w", encoding="utf-8") as stream:
        while True:
            now = time.monotonic()
            elapsed = now - started
            if elapsed > args.duration:
                break
            if now < next_sample:
                time.sleep(min(next_sample - now, 0.1))
                continue

            audio = call(args.port, "audio_stats")
            gpu = call(args.port, "gpu_state")
            interp = call(args.port, "gl_interp")
            dirty = call(args.port, "dirty_ram_stats")
            loader = call(args.port, "overlay_loader_status")
            compiler = call(args.port, "autocompile_status")
            stamp = time.monotonic()
            row = {
                "elapsed": round(stamp - started, 3),
                "frame": gpu["ws"]["cur_frame"],
                "game_mode": gpu["ws"]["game_mode"],
                "native_43": gpu["ws"]["present_native_43"],
                "display_y": gpu["display_y"],
                "gp0_draw": gpu["gp0_draw"],
                "spu_frames": audio["taps"][0]["frames"],
                "host_frames": audio["taps"][2]["frames"],
                "pump_underruns": audio["underruns"],
                "out_underruns": audio["out"]["underruns"],
                "overflow_drops": audio["out"]["overflow_drops"],
                "fill_ms": audio["out"]["fill_ms"],
                "correction": audio["out"]["correction"],
                "interp_swaps": interp["swaps"],
                "interp_history": interp["history"],
                "dirty_blocks": dirty["blocks_run"],
                "dirty_insns": dirty["insns_run"],
                "native_handoffs": dirty["native_handoffs"],
                "overlay_registered": loader["registered"],
                "overlay_loads": loader["loads"],
                "overlay_last_msg": loader["last_msg"],
                "overlay_native": loader["dispatch_native"],
                "overlay_interp": loader["dispatch_interp_fallback"],
                "overlay_stale": loader["stale_blocked"],
                "range_links": loader["range_links"],
                "range_index_overflow": loader["range_index_overflow"],
                "lazy_manifests": loader["lazy_manifests"],
                "lazy_manifest_overflow": loader["lazy_manifest_overflow"],
                "image_warm_loaded": loader.get("image_warm_loaded", 0),
                "image_warm_pending": loader.get("image_warm_pending", 0),
                "compiler_state": compiler["compile"]["state"],
                "compiler_runs": compiler["compile"]["runs"],
            }
            current_pc = {
                entry["pc"]: entry["hits"] for entry in dirty.get("per_pc", [])
            }
            pc_delta = [
                (hits - previous_pc.get(pc, 0), pc)
                for pc, hits in current_pc.items()
                if hits > previous_pc.get(pc, 0)
            ]
            pc_delta.sort(reverse=True)
            row["dirty_pc_delta_top"] = [
                {"pc": pc, "hits": hits} for hits, pc in pc_delta[:8]
            ]
            if previous:
                dt = stamp - previous["stamp"]
                row.update({
                    "dt": round(dt, 4),
                    "guest_hz": round((row["frame"] - previous["frame"]) / dt, 3),
                    "spu_hz": round((row["spu_frames"] - previous["spu_frames"]) / dt, 1),
                    "present_hz": round((row["interp_swaps"] - previous["interp_swaps"]) / dt, 2),
                    "underrun_delta": row["out_underruns"] - previous["out_underruns"],
                    "overflow_delta": row["overflow_drops"] - previous["overflow_drops"],
                    "draws_per_s": round((row["gp0_draw"] - previous["gp0_draw"]) / dt, 1),
                    "dirty_insns_delta": row["dirty_insns"] - previous["dirty_insns"],
                    "native_handoffs_delta": row["native_handoffs"] - previous["native_handoffs"],
                    "overlay_native_delta": row["overlay_native"] - previous["overlay_native"],
                    "overlay_interp_delta": row["overlay_interp"] - previous["overlay_interp"],
                    "overlay_stale_delta": row["overlay_stale"] - previous["overlay_stale"],
                })

            if args.screenshot_every > 0 and stamp >= next_shot:
                shot = f"frame_{row['frame']:08d}_t{int(row['elapsed']):04d}.png"
                path = runtime_path(os.path.join(args.out, shot))
                try:
                    call(args.port, "screenshot", path=path)
                    row["screenshot"] = shot
                except Exception as exc:
                    # Boot/display-disabled intervals are expected; telemetry
                    # must continue until the attract sequence becomes visible.
                    row["screenshot_error"] = str(exc)
                next_shot = stamp + args.screenshot_every

            stream.write(json.dumps(row, sort_keys=True) + "\n")
            stream.flush()
            print(json.dumps(row, sort_keys=True), flush=True)
            previous = {**row, "stamp": stamp}
            previous_pc = current_pc
            next_sample = stamp + args.interval


if __name__ == "__main__":
    main()
