"""Verify resident metadata against an owner-supplied SCUS-94454 raw disc.

Developer verification only (pip install unicorn). Players need no Python or
compiler: the native plugin prepares its own cache on first launch.
Executes the original MIPS decoder, never generated recompilation output.
Writes metadata only; use --check to compare with the checked-in catalogs.
"""
import argparse
import hashlib
import importlib.util
from pathlib import Path
import struct

from unicorn import Uc, UC_ARCH_MIPS, UC_MODE_MIPS32, UC_MODE_LITTLE_ENDIAN
from unicorn import mips_const as reg


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("disc", type=Path)
    parser.add_argument("--framework", type=Path, default=root / "psxrecomp-v4")
    parser.add_argument("--output", type=Path, default=root / "src/mods")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    if sha(args.disc.read_bytes()) != "8e9568388155787384c3166a5934a495fbff3fa57154ab0489b0f214fe05a8ab":
        raise ValueError("Expected original SCUS-94454 raw disc")
    spec = importlib.util.spec_from_file_location("disc_reader", args.framework / "tools/extract_overlays.py")
    iso = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(iso)
    disc = iso.DiscReader(str(args.disc), raw=True)
    files = {name: (lba, size) for name, lba, size in iso.enumerate_files(disc)}

    raw = ['// SCUS-94454 original-disc sector-padded hashes. No copyrighted asset bytes.',
           'static const CatalogEntry catalog[] = {']
    for name, (lba, size) in files.items():
        if not (name.startswith("BIN/") or name in (
                "MAIN.EXE", "CD/SWDATA.BIN", "CD/TOMBA2.DAT", "CD/TOMBA2.IDX",
                "CD/TOMBA2.IMG", "CD/TOMBA2.SND")):
            continue
        data = disc.read_file_bytes(lba, (size + 2047) & ~2047)
        assert not any(data[size:]), name
        raw.append(f'    {{"{name}", {lba}u, {size}u, "{sha(data)}"}},')
    assert len(raw) - 2 == 34
    raw.append('};')

    exe = disc.read_file_bytes(*files["MAIN.EXE"])
    assert sha(exe) == "cb580f5bd42895b1c452dffe53e1c8cb639a029e0afec194ae197fdfda9721ed"
    idx = disc.read_file_bytes(*files["CD/TOMBA2.IDX"])
    img = disc.read_file_bytes(*files["CD/TOMBA2.IMG"])
    table = struct.unpack_from('<16i', exe, 0x800 + 0x53c8)
    uc = Uc(UC_ARCH_MIPS, UC_MODE_MIPS32 | UC_MODE_LITTLE_ENDIAN)
    uc.mem_map(0, 16 * 1024 * 1024)
    uc.mem_write(0x10000, exe[2048:])
    rows = ['// Original MAIN.EXE 0x80044D8C verified in Unicorn with two fill patterns.',
            'static const TextureEntry textures[] = {']
    total = 0
    for scene in range(48):
        begin, end = struct.unpack_from('<II', idx, scene * 2048)
        if end <= begin:
            continue
        assert end <= len(img)
        count = struct.unpack_from('<I', img, begin)[0]
        assert count < 170
        pos = begin + 2048
        for j in range(count):
            rect = img[begin + 4 + j * 12:begin + 12 + j * 12]
            _, _, w, h = struct.unpack('<4h', rect)
            n = struct.unpack_from('<I', img, begin + 12 + j * 12)[0]
            start = pos
            data = img[pos:pos + n]
            pos += n
            assert pos <= end
            out = bytearray()
            i = 0
            deltas = [table[k * 2] + 2 * w * table[k * 2 + 1] for k in range(8)]
            while i < n:
                token = data[i]
                i += 1
                length, kind = token >> 3, token & 7
                if not kind:
                    if not length:
                        break
                    assert i + length <= n
                    out.extend(data[i:i + length])
                    i += length
                else:
                    for _ in range(length):
                        offset = len(out) + deltas[kind]
                        assert 0 <= offset < len(out)
                        out.append(out[offset])
            assert w * h * 2 <= len(out) < w * h * 2 + 64
            for fill in (0xA5, 0x5A):
                uc.mem_write(0x300000, rect)
                uc.mem_write(0x400000, data)
                uc.mem_write(0x7ffff0, bytes([fill]) * (len(out) + 32))
                for r, value in ((reg.UC_MIPS_REG_A0, 0x80300000),
                                 (reg.UC_MIPS_REG_A1, 0x80800000),
                                 (reg.UC_MIPS_REG_A2, 0x80400000),
                                 (reg.UC_MIPS_REG_A3, n),
                                 (reg.UC_MIPS_REG_SP, 0x801ff000),
                                 (reg.UC_MIPS_REG_RA, 0x80000000)):
                    uc.reg_write(r, value)
                uc.emu_start(0x80044d8c, 0x80000000, timeout=5000000, count=50000000)
                assert uc.reg_read(reg.UC_MIPS_REG_PC) == 0x80000000
                assert uc.reg_read(reg.UC_MIPS_REG_V0) == len(out)
                assert bytes(uc.mem_read(0x800000, len(out))) == out
                assert bytes(uc.mem_read(0x7ffff0, 16)) == bytes([fill]) * 16
                assert bytes(uc.mem_read(0x800000 + len(out), 16)) == bytes([fill]) * 16
            rows.append(f'    {{{start}u, {n}u, {w}u, {len(out)}u, "{sha(out)}"}},')
            total += len(out)
    assert len(rows) - 2 == 280
    rows.append('};')
    for name, lines in (("catalog", raw), ("textures", rows)):
        path = args.output / f"tomba2_seamless_{name}.inc"
        text = '\n'.join(lines) + '\n'
        if args.check:
            assert path.read_text() == text, f"Catalog mismatch: {path}"
        else:
            args.output.mkdir(parents=True, exist_ok=True)
            path.write_text(text, newline='\n')
    print(f"PASS: 34 source resources; 280 original-MIPS texture outputs, {total} decoded bytes")


if __name__ == "__main__":
    main()
