#!/usr/bin/env python3
"""Приводит записанную реплику к формату игры.

Правила, которые здесь зашиты (это не вкусовщина, а требования сборки):
  * 16 кГц, моно, 16 бит — VoiceBank читает только это;
  * края обрабатываются настоящими рампами (15 мс вход, 60 мс выход), а не
    обнулением: обрезанный в ноль сэмпл слышен как щелчок;
  * громкость выравнивается по RMS, а не по пику — иначе одна фраза тише другой;
  * перед согласной оставляется 80 мс, иначе первое слово проглатывается.

Запуск:
    python3 tools/normalize_voice.py audio/voice/mission_brief_11.wav [...]
    python3 tools/normalize_voice.py --all audio/voice/*.wav
"""
import audioop
import glob
import os
import sys
import wave

RATE = 16000
FADE_IN = 0.015
FADE_OUT = 0.060
PRE_ROLL = 0.080
TAIL = 0.100
RMS_TARGET = 0.115
PEAK_LIMIT = 0.89


def read(path):
    with wave.open(path) as w:
        frames, rate, width, channels = w.getnframes(), w.getframerate(), w.getsampwidth(), w.getnchannels()
        data = w.readframes(frames)
    if width != 2:
        data = audioop.lin2lin(data, width, 2)
    if channels == 2:
        data = audioop.tomono(data, 2, 0.5, 0.5)
    if rate != RATE:
        data = audioop.ratecv(data, 2, 1, rate, RATE, None)[0]
    return data


def trim(data):
    peak = audioop.max(data, 2) or 1
    threshold = max(60, int(peak * 0.004))
    total = len(data) // 2
    first, last = 0, total - 1
    for i in range(total):
        if abs(int.from_bytes(data[i * 2:i * 2 + 2], "little", signed=True)) > threshold:
            first = i
            break
    for i in range(total - 1, -1, -1):
        if abs(int.from_bytes(data[i * 2:i * 2 + 2], "little", signed=True)) > threshold:
            last = i
            break
    head = max(0, first - int(RATE * PRE_ROLL))
    tail = min(total, last + int(RATE * TAIL))
    return data[head * 2:tail * 2]


def ramp(data, seconds, rising):
    count = min(len(data) // 2, int(RATE * seconds))
    if count <= 0:
        return data
    out = bytearray(data)
    total = len(data) // 2
    for i in range(count):
        gain = (i + 1) / count if rising else (count - i) / count
        idx = i if rising else total - count + i
        value = int.from_bytes(out[idx * 2:idx * 2 + 2], "little", signed=True)
        out[idx * 2:idx * 2 + 2] = max(-32768, min(32767, int(value * gain))).to_bytes(2, "little", signed=True)
    return bytes(out)


def normalize(data):
    peak = audioop.max(data, 2) or 1
    rms = audioop.rms(data, 2) or 1
    gain = min(RMS_TARGET / (rms / 32768.0), (PEAK_LIMIT * 32768.0) / peak)
    data = audioop.mul(data, 2, gain)
    data = ramp(data, FADE_IN, True)
    data = ramp(data, FADE_OUT, False)
    return data


def process(path):
    data = normalize(trim(read(path)))
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(data)
    seconds = len(data) / 2 / RATE
    print(f"{os.path.basename(path):32s} {seconds:5.2f} с  rms={audioop.rms(data, 2):5d}  peak={audioop.max(data, 2):5d}")


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    paths = []
    for a in args:
        paths.extend(sorted(glob.glob(a)))
    if not paths:
        print(__doc__)
        return 1
    for p in paths:
        process(p)
    return 0


if __name__ == "__main__":
    sys.exit(main())
