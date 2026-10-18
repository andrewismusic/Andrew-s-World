#!/usr/bin/env python3
"""Compares two renders bar by bar: loudness (RMS dB) and brightness (spectral centroid).
Usage: tools/parity/compare.py prototype.wav native.wav --bpm 90 [--beats 4]   (needs numpy)"""
import argparse
import wave

import numpy as np


def load(path):
    with wave.open(path) as w:
        n, ch, sr = w.getnframes(), w.getnchannels(), w.getframerate()
        x = np.frombuffer(w.readframes(n), dtype=np.int16).astype(np.float64) / 32768
    return x.reshape(-1, ch).mean(axis=1), sr


def centroid(x, sr):
    spec = np.abs(np.fft.rfft(x * np.hanning(len(x))))
    f = np.fft.rfftfreq(len(x), 1 / sr)
    return float((spec * f).sum() / max(spec.sum(), 1e-12))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('a')
    ap.add_argument('b')
    ap.add_argument('--bpm', type=float, required=True)
    ap.add_argument('--beats', type=float, default=4)
    args = ap.parse_args()
    a, sr = load(args.a)
    b, _ = load(args.b)
    n = min(len(a), len(b))
    bar = int(round(60 / args.bpm * args.beats * sr))
    print(f"{'bar':>4} {'rms A':>7} {'rms B':>7} {'diff':>6}   {'centroid A':>10} {'centroid B':>10}")
    for i in range(n // bar):
        sa, sb = a[i * bar:(i + 1) * bar], b[i * bar:(i + 1) * bar]
        ra = 20 * np.log10(np.sqrt((sa ** 2).mean()) + 1e-12)
        rb = 20 * np.log10(np.sqrt((sb ** 2).mean()) + 1e-12)
        print(f"{i:>4} {ra:7.1f} {rb:7.1f} {rb - ra:+6.1f}   {centroid(sa, sr):10.0f} {centroid(sb, sr):10.0f}")


if __name__ == '__main__':
    main()
