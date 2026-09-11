"""Italian Tomba 2 disc recipe, guarded by the loader bytes it depends on.

Reads fresh generic extraction plus original disc files. No runtime captures.
The GAME-to-area gap has no producer ownership and is never code evidence.
"""
from pathlib import Path
import argparse
import base64
import hashlib
import json
import struct
import sys
import zlib

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--framework-root', type=Path, default=Path(__file__).resolve().parents[1] / 'psxrecomp-v4')
parser.add_argument('--disc', type=Path, required=True)
parser.add_argument('--generic-records', type=Path, required=True)
parser.add_argument('--out-dir', type=Path, required=True)
args = parser.parse_args()
FRAMEWORK = args.framework_root.resolve()
EVIDENCE = args.out_dir.resolve()
EVIDENCE.mkdir(parents=True, exist_ok=True)
sys.path.insert(0, str(FRAMEWORK / 'tools/aot_overlay_spike'))
import extract_generic as extractor
cue = args.disc.resolve()
binary, raw = extractor.parse_cue_datatrack(str(cue))
assert hashlib.file_digest(open(binary, 'rb'), 'sha256').hexdigest() == (
    'b9c8ff05f265f2ec359bc559b0731109de15c0a0d0d036b0da23ddbf98a19188')
disc = extractor.eo.DiscReader(binary, raw=raw)
files = {name.upper(): (lba, size) for name, lba, size in extractor.eo.enumerate_files(disc)}

def read(name):
    return disc.read_file_bytes(*files[name])

main = read('MAIN.EXE')[2048:]
start = read('BIN/START.BIN')
shared_base = 0x80107448
area_base = 0x8010A444

def verify_words(data, base, expected):
    for address, value in expected.items():
        assert struct.unpack_from('<I', data, address - base)[0] == value, hex(address)

verify_words(main, 0x80010000, {
    0x80045A00: 0x3C048010, 0x80045A08: 0x2463EFE8,
    0x80045A0C: 0x001010C0, 0x80045A14: 0x8C450000,
    0x80045A18: 0x8C460004, 0x80045A1C: 0x0C00789C,
    0x80045A20: 0x24847448,
    0x80045CCC: 0x3C048011, 0x80045CD8: 0x2484A444,
    0x80045CE4: 0x0C011665,
})
verify_words(start, shared_base, {
    0x8010778C: 0x3C02800C, 0x80107790: 0x2452EFE8,
    0x80107794: 0x3C028010, 0x80107798: 0x24517A90,
    0x801077A0: 0x8E250000, 0x801077AC: 0x0C02319E,
    0x801077D0: 0x0C022BA6, 0x801077D8: 0xAE420000,
    0x801077DC: 0x8E020004, 0x801077E4: 0xAE420004,
    0x801077E8: 0x26520008, 0x801077EC: 0x26310004,
    0x801077F4: 0x2A620003,
})
mapping = []
for selector, pointer in enumerate(struct.unpack_from('<3I', start, 0x648)):
    offset = pointer - shared_base
    name = start[offset:start.index(0, offset)].decode('ascii')
    expected = ['\\BIN\\START.BIN;1', '\\BIN\\DEMO.BIN;1', '\\BIN\\GAME.BIN;1'][selector]
    assert name == expected
    entry = struct.unpack_from('<I', main, 0x800A4CD4 + selector * 4 - 0x80010000)[0]
    mapping.append(dict(selector=selector, disc_file=name[1:-2].replace('\\', '/'),
                        load_addr=hex(shared_base), entry=hex(entry),
                        runtime_table=hex(0x800BEFE8 + selector * 8)))

records = json.loads(args.generic_records.read_text(encoding='utf-8-sig'))
jobs = []
area_records = {}
for record in records:
    data = base64.b64decode(record['bytes_b64'])
    load = int(record['load_addr'], 0)
    assert not record.get('executed_pcs')
    names = []
    known = []
    for name, base in [('BIN/DEMO.BIN', shared_base), ('BIN/SOP.BIN', area_base),
                       ('BIN/OPN.BIN', 0x80188000), ('BIN/CRD.BIN', 0x80188000)] + [
                       (f'BIN/A0{char}.BIN', area_base) for char in '0123456789ABCDEFGHIJKL']:
        payload = read(name)
        offset = base - load
        if offset >= 0 and data[offset:offset + len(payload)] == payload:
            names.append(Path(name).stem)
            known.append((base, base + len(payload)))
    if load < 0x800C0000 and data == main[load - 0x80010000:load - 0x80010000 + len(data)]:
        names = [f'MAIN_{load:08X}']
        known = [(load, load + len(data))]
    if record.get('producer') == 'bios_resident_manifest':
        names = ['BIOS_resident']
        known = [(load, load + len(data))]
    assert names, (hex(load), len(data))
    name = '+'.join(names)
    if len(names) == 1 and names[0].startswith('A0'):
        area_records[names[0]] = record
    jobs.append(dict(name=name, record=record, known_ranges=known, source='generic disc extraction'))

new_shared = {}
for item in (mapping[0], mapping[2]):
    payload = read(item['disc_file'])
    entry = int(item['entry'], 0)
    assert shared_base <= entry < shared_base + len(payload)
    seeds = set(extractor.prologues(payload, shared_base))
    seeds |= extractor.frameless_leaf_entries(payload, shared_base)
    seeds |= extractor.supplemental_callable_seeds(payload, shared_base)
    seeds.add(entry)
    page, data = extractor.page_aligned_region(shared_base, payload)
    record = extractor.rec(page, data, sorted(seeds), dispatch_extra=[entry],
        producer_ranges=[(shared_base, shared_base + len(payload))],
        static_discovery=set(extractor.direct_jal_roots(payload, shared_base)) | {entry})
    name = Path(item['disc_file']).stem
    new_shared[name] = record
    jobs.append(dict(name=name, record=record,
                     known_ranges=[(shared_base, shared_base + len(payload))],
                     source='original-disc startup filename table and resident loader'))

game_record = new_shared['GAME']
game_bytes = read('BIN/GAME.BIN')
game_end = shared_base + len(game_bytes)
assert game_end == 0x8010A2D0 and area_base - game_end == 372
for name, area in sorted(area_records.items()) + [('SOP', next(
        job['record'] for job in jobs if job['name'] == 'SOP'))]:
    area_bytes = read(f'BIN/{name}.BIN')
    page = shared_base & ~0xFFF
    data = bytearray(area_base + len(area_bytes) - page)
    data[shared_base - page:game_end - page] = game_bytes
    data[area_base - page:] = area_bytes
    known = [(shared_base, game_end), (area_base, area_base + len(area_bytes))]
    seeds = sorted({int(pc, 0) for r in (game_record, area) for pc in r['function_entry_pcs']})
    dispatch = sorted({int(pc, 0) for r in (game_record, area) for pc in r['dispatch_entry_pcs']})
    record = extractor.rec(page, bytes(data), seeds, dispatch_extra=dispatch,
                           producer_ranges=known)
    # Filling the serialization gap does not claim its runtime value. Both
    # discovery and the post-build guard audit exclude these unknown bytes.
    record['producer_name'] = f'Italian GAME + {name}; unowned gap 0x{game_end:08X}..0x{area_base:08X}'
    jobs.append(dict(name=f'GAME+{name}', record=record, known_ranges=known,
                     unknown_ranges=[(game_end, area_base)],
                     source='loader-positioned original disc files with disjoint producer bounds'))

priority = {'BIOS_resident': 0, 'MAIN_80010000': 1, 'MAIN_80038000': 2,
            'START': 3, 'GAME': 4, 'DEMO': 5, 'OPN': 6, 'CRD': 7, 'SOP': 8}
jobs.sort(key=lambda job: (priority.get(job['name'], 10), job['name']))
inputs = EVIDENCE / 'runtime-inputs'
inputs.mkdir(exist_ok=True)
inventory = []
for job in jobs:
    record = job.pop('record')
    # Exact disc extents never append RAM-capture delay-slot guard words.
    # Without this declaration, sizes congruent to 4 modulo 4096 trigger
    # the legacy capture heuristic and incorrectly shorten a producer.
    record['guard_bytes'] = 0
    path = inputs / (job['name'] + '.json')
    path.write_text(json.dumps([record], indent=2))
    data = base64.b64decode(record['bytes_b64'])
    load = int(record['load_addr'], 0)
    job.update(input=str(path), load_addr=hex(load), size=len(data),
               sha256=hashlib.sha256(data).hexdigest(),
               pair_stem=f'{load & 0x1FFFFFFF:08X}_{zlib.crc32(data):08X}')
    inventory.append(job)
assert len(inventory) == 77 and len(area_records) == 22
output = dict(game_id='SCES-02686', disc=str(cue), shared_mapping=mapping,
              shared_area_gap=dict(start=hex(game_end), end=hex(area_base), bytes=372,
                                   treatment='unowned; all function guards must exclude it'),
              job_count=len(inventory), jobs=inventory)
(EVIDENCE / 'runtime-input-inventory.json').write_text(json.dumps(output, indent=2))
print(json.dumps({key: value for key, value in output.items() if key != 'jobs'}, indent=2))
