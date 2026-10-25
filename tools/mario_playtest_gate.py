"""Prepare, qualify, and launch Mario playtests through an artifact/performance gate.

Use a private runtime directory and private memory cards. `launch` never starts
a game unless every audited shard and the exact executable/configuration have
passed `qualify`. This is development tooling, not redistributable packaging.
"""
import argparse
import base64
import hashlib
import json
import os
from pathlib import Path
import socket
import subprocess
import sys

from mario_probe import Probe
from mario_perf import compare, measure_route

ROOT = Path(__file__).resolve().parents[1]
FRAMEWORK = ROOT / "psxrecomp-v4"


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def framework_tools():
    sys.path.insert(0, str(FRAMEWORK / "tools"))
    import compile_overlays
    return compile_overlays


def validate_aot(directory, expected_tag):
    receipt = json.loads((directory / "AOT_CACHE_AUDIT.json").read_text())
    require(receipt["cache_tag"] == expected_tag, "Native shard audit is for another configuration")
    require(receipt.get("all_pairs_valid") is True and
            receipt.get("all_guards_match_known_input_bytes") is True,
            "Native shards have no successful original-input audit")
    require(receipt.get("recipe_count", 0) > 0 and receipt.get("required_images") and
            len(receipt.get("image_coverage", [])) == receipt["recipe_count"],
            "Native audit does not cover every verified recipe")
    pairs = receipt.get("pairs", [])
    require(pairs and len(pairs) == receipt.get("published_pairs"), "Incomplete audited shard inventory")
    arch = framework_tools().cache_arch_abi()
    cache = directory / "cache" / receipt["game_id"] / "gcc" / arch / expected_tag
    require(cache.is_dir(), "The matching native shard directory is missing")
    for pair in pairs:
        name = pair["dll"]
        require(Path(name).name == name and "\\" not in name, "Invalid shard path in receipt")
        files = [(name, "dll_sha256"), (Path(name).with_suffix(".ranges").name, "manifest_sha256")]
        if "resident_sha256" in pair:
            files.append((Path(name).with_suffix(".resident").name, "resident_sha256"))
        for name, key in files:
            path = cache / name
            require(path.is_file() and digest(path) == pair[key], "Missing or changed audited shard: " + name)
    return receipt


def identity(args):
    compiler = framework_tools()
    tag = compiler.cache_tag(str(FRAMEWORK / "runtime/include"), str(args.recompiler), str(args.config), 2)
    receipt = validate_aot(args.exe.parent, tag)
    cache = args.exe.parent / "cache" / receipt["game_id"] / "gcc" / compiler.cache_arch_abi() / tag
    artifacts = sorted((str(p.relative_to(cache)), digest(p)) for p in cache.rglob("*")
                       if p.is_file() and p.suffix in (".dll", ".so", ".dylib", ".ranges", ".resident"))
    return {"exe_sha256": digest(args.exe), "config_sha256": digest(args.config),
            "audit_sha256": digest(args.exe.parent / "AOT_CACHE_AUDIT.json"), "cache_tag": tag,
            "gate_sha256": digest(Path(__file__)),
            "measurement_tool_sha256": digest(ROOT / "tools/mario_perf.py"),
            "cache_artifacts_sha256": hashlib.sha256(json.dumps(artifacts).encode()).hexdigest()}


def audit_runtime_additions(args):
    """Prove page/orphan shards against original recipe bytes, not RAM captures.

    The framework's offline audit requires matching image bases. Runtime shards
    can start at a page within that image, so prove their bounded function CRCs
    separately while retaining the complete offline image audit.
    """
    compiler = framework_tools()
    current = identity(args)
    receipt = validate_aot(args.exe.parent, current["cache_tag"])
    inventory_bytes = args.inventory.read_bytes()
    require(hashlib.sha256(inventory_bytes).hexdigest() == receipt["recipe_input_sha256"],
            "Original-input inventory differs from the offline native audit")
    recipes = []
    for job in json.loads(inventory_bytes)["jobs"]:
        record = json.loads(Path(job["input"]).read_text(encoding="utf-8-sig"))[0]
        data = base64.b64decode(record["bytes_b64"], validate=True)
        require(not record.get("executed_pcs") and len(data) == record["size"] and
                hashlib.sha256(data).hexdigest() == job["sha256"],
                "Native audit needs unchanged original-disc recipes")
        bounds = [(lo & 0x1fffffff, hi & 0x1fffffff) for lo, hi in job["known_ranges"]]
        recipes.append((job["name"], int(record["load_addr"], 0), data, bounds))
    cache = (args.exe.parent / "cache" / receipt["game_id"] / "gcc" /
             compiler.cache_arch_abi() / current["cache_tag"])
    offline = {pair["dll"] for pair in receipt["pairs"]}
    abi = compiler.overlay_abi_tag(str(FRAMEWORK / "runtime/include"), 2)
    additions = []
    for dll in sorted(cache.glob("*" + compiler.overlay_ext())):
        if dll.name in offline:
            continue
        ids = compiler.load_shard_func_ids(str(dll), abi)
        require(ids, "Invalid runtime native shard: " + dll.name)
        matched = next((name for name, load, data, bounds in recipes
                        if all(any(lo <= (start & 0x1fffffff) and
                                   (start & 0x1fffffff) + length <= hi for lo, hi in bounds)
                               for _, _, ranges in ids for start, length in ranges)
                        and len(compiler.current_variant_func_ids(ids, data, load, len(data))) == len(ids)), None)
        require(matched, "Runtime shard has no original-input guard proof: " + dll.name)
        additions.append({"dll": dll.name, "dll_sha256": digest(dll),
                          "manifest_sha256": digest(dll.with_suffix(".ranges")),
                          "matched_recipe": matched, "functions": len(ids)})
    return {"inventory_sha256": hashlib.sha256(inventory_bytes).hexdigest(),
            "all_additional_guards_match_original_bytes": True, "pairs": additions}


def qualification_process(port):
    require(os.name == "nt", "Process/endpoint qualification is currently implemented for Windows")
    command = ("$ids = @(Get-NetTCPConnection -LocalPort " + str(int(port)) +
               " -State Listen | Select-Object -ExpandProperty OwningProcess -Unique); "
               "if ($ids.Count -ne 1) { throw 'Expected one debug listener' }; "
               "Get-CimInstance Win32_Process -Filter ('ProcessId=' + $ids[0]) | "
               "Select-Object ExecutablePath,CommandLine,ProcessId | ConvertTo-Json -Compress")
    process = json.loads(subprocess.check_output(
        ["powershell.exe", "-NoProfile", "-Command", command], text=True))
    require("--hidden-window" in process["CommandLine"] and "--memcard-dir" in process["CommandLine"],
            "Automated qualification requires a hidden runtime with explicitly isolated cards")
    return process


def prepare(args):
    # Reuse the established disc extraction, then rebuild through the supported
    # recipe compiler and auditor. Never rename an older cache namespace.
    sys.path.insert(0, str(FRAMEWORK / "tools"))
    import aot_overlay_pipeline as pipeline
    inventory = json.loads(args.inventory.read_text())
    work = args.work / "native-build"
    work.mkdir(parents=True, exist_ok=False)
    cache = pipeline.build(inventory, args.config, args.recompiler, work,
                           args.gcc, args.workers, ROOT, flavor=2)
    receipt = pipeline.audit(args.config, args.recompiler, cache, args.inventory,
                             work / "audit.json", flavor=2)
    for key in ("profile_sha256", "original_disc_sha256", "required_images"):
        receipt[key] = inventory[key]
    pipeline.stage(cache, args.stage, receipt)


def qualify(args):
    additions = audit_runtime_additions(args)
    before = identity(args)
    process = qualification_process(args.port)
    require(digest(process["ExecutablePath"]) == before["exe_sha256"],
            "The debug endpoint belongs to a different executable")
    probe = Probe(args.port, timeout=75)
    loader = probe.call("overlay_loader_status")
    require(Path(loader["cache_dir"]).resolve() == (args.exe.parent / "cache").resolve(),
            "Qualification runtime is using another native cache")
    report = {"identity": before, "passed": False, "minimum_hz": args.minimum_hz,
              "runtime_additions_audit": additions}
    try:
        report["quiet"] = compare(probe)
        probe.savestate("load", args.moving_slot)
        probe.wait_frames(30)
        require(probe.player()["mode"] == 1 and probe.state()["equipped"] == 1,
                "Moving fixture must start as live Mario gameplay")
        native_start = probe.call("overlay_loader_status")
        dirty_start = probe.call("dirty_ram_stats")
        # Stay on the safe village path while exercising walking, both turns,
        # jumps and attacks. No success flags or actor coordinates are written.
        steps = [(55, 0x20), (55, 0x80), (4, 0x4000), (6, 0),
                 (4, 0x4000), (65, 0), (4, 0x2000), (55, 0)] * 3
        measurement = measure_route(probe, steps)
        native_end = probe.call("overlay_loader_status")
        dirty_end = probe.call("dirty_ram_stats")
        native = native_end["dispatch_native"] - native_start["dispatch_native"]
        fallback = native_end["dispatch_interp_fallback"] - native_start["dispatch_interp_fallback"]
        report["moving"] = {**measurement,
                            "native_dispatches": native, "fallback_dispatches": fallback,
                            "fallback_fraction": fallback / max(1, native + fallback),
                            "interpreted_instructions": dirty_end["insns_run"] - dirty_start["insns_run"],
                            "perf": probe.call("frame_perf")}
        report["dispatch"] = probe.call("unknown_dispatch_log", limit=8)
        report["checks"] = {
            "quiet_pacing": all(c["guest_vblanks_per_second"] >= args.minimum_hz for c in report["quiet"]["cases"]),
            "moving_pacing": report["moving"]["guest_vblanks_per_second"] >= args.minimum_hz,
            "native_execution": native > 0 and report["moving"]["fallback_fraction"] < 0.01,
            "no_dispatch_misses": report["dispatch"]["total"] == 0,
            "still_in_gameplay": probe.player()["mode"] == 1 and probe.state()["equipped"] == 1,
            "artifacts_unchanged": before == identity(args),
        }
        report["passed"] = all(report["checks"].values())
    except BaseException as error:
        report["error"] = repr(error)
        raise
    finally:
        probe.call("clear_input")
        probe.savestate("load", 8)
        args.qualification.write_text(json.dumps(report, indent=2) + "\n")
    require(report["passed"], "Playtest performance gate failed; see " + str(args.qualification))


def validate_qualification(current, report):
    require(report.get("identity") == current, "Qualification is stale for this executable/config/cache")
    require(report.get("passed") is True and report.get("checks") and all(report["checks"].values()),
            "No passing runtime performance qualification")


def launch(args):
    current = identity(args)
    validate_qualification(current, json.loads(args.qualification.read_text()))
    require(not (args.exe.parent / "settings.toml").exists(),
            "Use a private handoff directory without inherited fullscreen/audio settings")
    # Do not steal an existing owner's debug endpoint or close their process.
    with socket.socket() as check:
        require(check.connect_ex(("127.0.0.1", args.port)) != 0, "Debug port is already in use")
    env = dict(os.environ)
    for key in ("SDL_AUDIO_DRIVER", "SDL_AUDIODRIVER", "PSX_HEADLESS"):
        env.pop(key, None)
    env["PATH"] = str(Path(args.gcc).parent) + os.pathsep + env.get("PATH", "")
    command = [str(args.exe), "--no-launcher", "--game", str(args.config),
               "--bios", str(args.bios), "--debug-port", str(args.port),
               "--memcard-dir", str(args.cards)]
    process = subprocess.Popen(command, cwd=ROOT, env=env)
    probe = Probe(args.port, timeout=75)
    probe.wait_game_ready(90)
    probe.savestate("load", 8)
    print("Owner playtest ready; PID", process.pid, flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("prepare", "qualify", "launch"))
    parser.add_argument("--exe", type=Path)
    parser.add_argument("--config", type=Path, default=ROOT / ".research/game-test.toml")
    parser.add_argument("--recompiler", type=Path, default=FRAMEWORK / "recompiler/build/psxrecomp-game.exe")
    parser.add_argument("--inventory", type=Path)
    parser.add_argument("--work", type=Path)
    parser.add_argument("--stage", type=Path)
    parser.add_argument("--gcc", default="C:/msys64/mingw64/bin/gcc.exe")
    parser.add_argument("--workers", type=int, default=2)
    parser.add_argument("--qualification", type=Path)
    parser.add_argument("--minimum-hz", type=float, default=57.0)
    parser.add_argument("--moving-slot", type=int, default=10)
    parser.add_argument("--port", type=int, default=4598)
    parser.add_argument("--bios", type=Path, default=ROOT / "build-mario/bios/openbios.bin")
    parser.add_argument("--cards", type=Path)
    args = parser.parse_args()
    for key, value in vars(args).items():
        if isinstance(value, Path):
            setattr(args, key, value.resolve())
    require(sys.version_info >= (3, 11), "Use Python 3.11 or newer for the native-cache tooling")
    os.chdir(ROOT)
    require(args.workers > 0 and args.minimum_hz >= 57, "Do not weaken the playtest gate")
    if args.action == "prepare":
        require(args.inventory and args.work and args.stage, "prepare needs inventory, work, stage")
        prepare(args)
    else:
        require(args.exe and args.qualification, "qualify/launch need exe and qualification")
        if args.action == "qualify":
            require(args.inventory and args.inventory.is_file(), "qualify needs the audited original-input inventory")
            qualify(args)
        else:
            require(args.cards and args.cards.is_dir(), "launch needs private cards/fixtures")
            launch(args)


if __name__ == "__main__":
    main()
