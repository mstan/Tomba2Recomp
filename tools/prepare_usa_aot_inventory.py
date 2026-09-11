"""Verify US generic AOT records against original disc file extents."""
import argparse
import base64
import hashlib
import json
from pathlib import Path
import struct
import sys
import zlib

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--framework-root', type=Path,
                    default=Path(__file__).resolve().parents[1] / 'psxrecomp-v4')
parser.add_argument('--disc', type=Path, required=True)
parser.add_argument('--generic-records', type=Path, required=True)
parser.add_argument('--out-dir', type=Path, required=True)
args = parser.parse_args()
sys.path.insert(0, str(args.framework_root / 'tools/aot_overlay_spike'))
import extract_generic as extractor
binary, raw = extractor.parse_cue_datatrack(str(args.disc))
disc = extractor.eo.DiscReader(binary, raw=raw)
files = {name.upper(): (lba, size) for name, lba, size in extractor.eo.enumerate_files(disc)}
assert 'SCUS_944.54' in files
main = disc.read_file_bytes(*files['MAIN.EXE'])
assert main[:8] == b'PS-X EXE'
main_base = struct.unpack_from('<I', main, 0x18)[0]
main_body = main[2048:]
raw_files = {name: disc.read_file_bytes(*where) for name, where in files.items()
             if name.startswith('BIN/') and name.endswith('.BIN')}
records = json.loads(args.generic_records.read_text(encoding='utf-8-sig'))
output = args.out_dir.resolve()
inputs = output / 'runtime-inputs'
inputs.mkdir(parents=True, exist_ok=True)
jobs = []
areas = set()
for index, record in enumerate(records):
    assert not record.get('executed_pcs')
    data = base64.b64decode(record['bytes_b64'], validate=True)
    load = int(record['load_addr'], 0)
    known, sources = [], []
    offset = load - main_base
    if offset >= 0 and data == main_body[offset:offset + len(data)]:
        known.append((load, load + len(data)))
        sources.append('MAIN.EXE')
    for name, payload in raw_files.items():
        offset = data.find(payload) if payload else -1
        if offset >= 0:
            assert data.find(payload, offset + 1) < 0, f'Ambiguous file extent: {name}'
            base = load + offset
            known.append((base, base + len(payload)))
            sources.append(name)
            if name in {f'BIN/A0{char}.BIN' for char in '0123456789ABCDEFGHIJKL'}:
                assert base == 0x80108F9C, (name, hex(base))
                areas.add(name)
    if record.get('producer') == 'bios_resident_manifest':
        # This separate producer is generated from the BIOS profile/manifest,
        # not inferred from a gameplay capture. Retain its declared ownership.
        known = [(int(r['start'], 0), int(r['end'], 0))
                 for r in record.get('producer_ranges', [])]
        sources = ['BIOS resident manifest']
    assert known, f'Record {index} has no verified source extent'
    record['guard_bytes'] = 0
    path = inputs / f'{index:03d}.json'
    path.write_text(json.dumps([record], indent=2), encoding='utf-8')
    jobs.append(dict(name=f"{index:03d}-" + '+'.join(sources), sources=sources,
                     known_ranges=known, input=str(path), load_addr=hex(load),
                     size=len(data), sha256=hashlib.sha256(data).hexdigest(),
                     pair_stem=f'{load & 0x1fffffff:08X}_{zlib.crc32(data):08X}'))
assert len(areas) == 22, sorted(areas)
inventory = dict(game_id='SCUS-94454', areas=sorted(areas), jobs=jobs,
                 original_disc_sha256=hashlib.file_digest(open(binary, 'rb'), 'sha256').hexdigest())
(output / 'runtime-input-inventory.json').write_text(json.dumps(inventory, indent=2), encoding='utf-8')
print(json.dumps(dict(recipes=len(jobs), areas=len(areas), output=str(output))))
