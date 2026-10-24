"""Verify an imported voice in the real post-SPU capture (private runtime/cache).

Requires NumPy for correlation. Stimulates guest voice state directly, so this
checks audio integration, not the gameplay event that normally triggers it.
Native event selection and state transactions are checked by mario_smoke.py.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import wave
import numpy as np
from mario_probe import Probe


def cached_voice(path, voice=437):
    cache = path.read_bytes()
    if hashlib.sha256(cache).hexdigest() != "1cbe9ce74e63464da8582d14dea5f8ba94e30a8c0258f03fee80f9abb3ace588":
        raise ValueError("Expected the verified private r2 cache")
    count, = struct.unpack_from("<I", cache, 8)
    offset = 12
    for _ in range(count):
        identity, size = struct.unpack_from("<II", cache, offset)
        offset += 8
        if identity == 10000+voice:
            return np.frombuffer(cache[offset:offset+size], dtype="<i2").astype(np.int64)
        offset += size
    raise ValueError("Voice is absent from cache")


def expected_pcm(source):
    # Independent explicit arithmetic, including C++ division toward zero.
    def divide(a, b):
        return a//b if a >= 0 else -((-a)//b)
    cursor = fraction = 0
    samples = []
    while cursor < len(source):
        a, b = int(source[cursor]), int(source[min(cursor+1, len(source)-1)])
        value = a + divide((b-a)*fraction, 44100)
        fade = min(128, cursor, len(source)-1-cursor)
        samples.append(divide(value*fade*3, 128*4))
        fraction += 32000
        cursor += fraction//44100
        fraction %= 44100
    return np.asarray(samples, dtype=np.float64)


def correlate(captured, expected):
    # Eight phases avoid hiding a real voice because its first sample is not
    # aligned to our decimation. Bound work to the short private capture.
    expected = expected[::8]
    expected -= expected.mean()
    norm = np.linalg.norm(expected)
    best = (-1.0, 0)
    for phase in range(8):
        signal = captured[phase::8].astype(np.float64)
        size = len(expected)
        if len(signal) < size:
            continue
        sums = np.r_[0., np.cumsum(signal)]
        energy = np.r_[0., np.cumsum(signal*signal)]
        variance = energy[size:]-energy[:-size] - (sums[size:]-sums[:-size])**2/size
        values = np.correlate(signal, expected, "valid") / (norm*np.sqrt(np.maximum(variance, 1)))
        peak = int(np.argmax(values))
        if values[peak] > best[0]:
            best = float(values[peak]), peak*8+phase
    return {"normalized_correlation": best[0], "voice_offset_samples": best[1]}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cache", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--port", type=int, default=4597)
    args = parser.parse_args()
    expected = expected_pcm(cached_voice(args.cache))
    args.output.mkdir(parents=True, exist_ok=True)
    probe = Probe(args.port)
    probe.wait_game_ready()
    probe.savestate("load", 8)
    probe.wait_frames(8)
    address = probe.state()["address"]
    probe.write(address+36, bytes(12))
    start = probe.call("audio_stats")["taps"][0]["frames"]
    probe.write(address+36, struct.pack("<I", 437))
    probe.wait_frames(60)
    end = probe.call("audio_stats")["taps"][0]["frames"]
    capture = args.output/"post-spu-voice.wav"
    receipt = probe.call("audio_wav", tap=0, path=str(capture.resolve()), start=str(start), count=str(end-start))
    with wave.open(str(capture), "rb") as wav:
        if wav.getframerate() != 44100 or wav.getnchannels() != 2 or wav.getsampwidth() != 2:
            raise ValueError("Unexpected runtime audio format")
        samples = np.frombuffer(wav.readframes(wav.getnframes()), dtype="<i2").reshape(-1, 2)
    result = {**correlate(samples[:, 0], expected), "receipt": receipt,
              "state_after": probe.state(), "stimulus": "direct guest voice 437, no gameplay success flags"}
    result["passed"] = result["normalized_correlation"] > 0.55 and result["state_after"]["voice"] == 0
    (args.output/"report.json").write_text(json.dumps(result, indent=2)+"\n", encoding="utf-8")
    print(json.dumps(result, indent=2))
    if not result["passed"]:
        raise SystemExit(1)
