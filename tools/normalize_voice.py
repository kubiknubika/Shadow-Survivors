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

Режим --join собирает длинную фразу из коротких кусков:

    python3 tools/normalize_voice.py --join out.wav part1.wav part2.wav part3.wav

Зачем он нужен. Синтез угадывает ударение по контексту, и на длинной фразе
ошибается на словах-омографах («разомкнут» — это и «разОмкнут», и «разомкнУт»).
Короткий кусок оставляет модели один смысл, поэтому длинные описания дел
читаются по одной мысли за раз, а паузы ставит уже сборка.
"""
import glob
import os
import sys
import wave

try:                      # numpy даёт фильтр перед понижением частоты
    import numpy as np
except ImportError:       # без него остаётся линейная интерполяция (хуже звучит)
    np = None

RATE = 16000
CUTOFF = 7200             # ниже половины новой частоты: выше 8 кГц всё равно не влезет
FADE_IN = 0.015
FADE_OUT = 0.060
PRE_ROLL = 0.080
TAIL = 0.100
JOIN_GAP = 0.260          # пауза между кусками при --join: вдох, а не склейка
RMS_TARGET = 0.115
RMS_TARGET_LOUD = 0.140   # --loud: короткие боевые выкрики, которые иначе слышны тише
RMS_TARGET_DENSE = 0.135  # --dense: реплики с высоким пик-фактором
PEAK_LIMIT = 0.89
# Пик-фактор (пик/RMS) — вот что слышится как тишина при равном RMS. У старых
# записей он 4,7–5,2, у свежих коротких фраз доходил до 7,4: формально громкость
# «в коридоре», а на слух тише соседей. Поэтому --dense приводит не только RMS,
# но и пик-фактор: цель 0,160 по RMS при потолке 0,70 даёт crest ≈ 4,7.
RMS_TARGET_DENSE = 0.160
PEAK_LIMIT_DENSE = 0.70


def read(path):
    """Читает WAV и приводит к 16 кГц моно 16 бит.

    Понижение частоты делается с фильтром: сначала срезается всё выше 7,2 кГц,
    потом сигнал пересчитывается на новую сетку. Без фильтра верхние гармоники
    складываются в слышимый «звон» — именно он читается как лишние гласные
    посреди слова, хотя диктор их не произносил.
    """
    with wave.open(path) as w:
        frames, rate, width, channels = w.getnframes(), w.getframerate(), w.getsampwidth(), w.getnchannels()
        data = w.readframes(frames)
    if np is None:
        import audioop
        if width != 2:
            data = audioop.lin2lin(data, width, 2)
        if channels == 2:
            data = audioop.tomono(data, 2, 0.5, 0.5)
        if rate != RATE:
            data = audioop.ratecv(data, 2, 1, rate, RATE, None)[0]
        return data

    raw = np.frombuffer(data, dtype=np.int16).astype(np.float32) / 32768.0
    if channels == 2:
        raw = raw.reshape(-1, 2).mean(axis=1)
    if rate != RATE:
        spectrum = np.fft.rfft(raw)
        freqs = np.fft.rfftfreq(len(raw), 1.0 / rate)
        # плавный спад от 0,8*CUTOFF до CUTOFF, дальше ноль — без «звонкого» среза
        taper = np.clip((CUTOFF - freqs) / (CUTOFF * 0.2), 0.0, 1.0)
        spectrum *= taper
        raw = np.fft.irfft(spectrum, n=len(raw))
        duration = len(raw) / rate
        grid = np.linspace(0.0, duration, int(round(duration * RATE)), endpoint=False)
        raw = np.interp(grid, np.arange(len(raw)) / rate, raw)
    pcm = np.clip(raw * 32767.0, -32768, 32767).astype(np.int16)
    return pcm.tobytes()


def trim(data):
    """Обрезает тишину по краям, оставляя 80 мс перед первой согласной."""
    if np is not None:
        samples = np.frombuffer(data, dtype=np.int16).astype(np.float32)
        peak = float(np.max(np.abs(samples))) or 1.0
        threshold = max(60.0, peak * 0.004)
        loud = np.nonzero(np.abs(samples) > threshold)[0]
        if len(loud) == 0:
            return data
        first, last = int(loud[0]), int(loud[-1])
    else:
        import audioop
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
    tail = min(len(data) // 2, last + int(RATE * TAIL))
    return data[head * 2:tail * 2]


def ramp(data, seconds, rising):
    """Настоящее нарастание на краях — не обнуление, иначе слышен щелчок."""
    if np is not None:
        samples = np.frombuffer(data, dtype=np.int16).astype(np.float32)
        count = min(len(samples), int(RATE * seconds))
        if count <= 0:
            return data
        gain = np.linspace(0.0, 1.0 if rising else 0.0, count, endpoint=True)
        if not rising:
            gain = gain[::-1]
        if rising:
            samples[:count] *= gain
        else:
            samples[-count:] *= gain
        return np.clip(samples, -32768, 32767).astype(np.int16).tobytes()
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


def normalize(data, target=RMS_TARGET, peak_limit=PEAK_LIMIT):
    """Выравнивает громкость по RMS, а не по пику: иначе фразы звучат по-разному."""
    if np is not None:
        samples = np.frombuffer(data, dtype=np.int16).astype(np.float32) / 32768.0
        peak = float(np.max(np.abs(samples))) or 1.0
        rms = float(np.sqrt(np.mean(samples ** 2))) or 1.0
        gain = target / rms
        if peak * gain <= peak_limit:
            samples = samples * gain
        else:
            # Короткая фраза с резким пиком не давала добрать громкость: прежний
            # min() просто оставлял клип тише остальных (замер: 2723 при 3767).
            # Мягкое колено возвращает уровень, не срезая пик в ноль.
            x = samples * gain
            knee = 0.55 * peak_limit
            ax = np.abs(x)
            soft = np.where(ax <= knee, ax, knee + (peak_limit - knee) * np.tanh((ax - knee) / (peak_limit - knee)))
            samples = np.sign(x) * soft
        data = np.clip(samples * 32767.0, -32768, 32767).astype(np.int16).tobytes()
    else:
        import audioop
        peak = audioop.max(data, 2) or 1
        rms = audioop.rms(data, 2) or 1
        gain = min(target / (rms / 32768.0), (peak_limit * 32768.0) / peak)
        data = audioop.mul(data, 2, gain)
    data = ramp(data, FADE_IN, True)
    data = ramp(data, FADE_OUT, False)
    return data


def join(out_path, parts, target=RMS_TARGET):
    """Склеивает куски настоящей тишиной — по вдоху между мыслями."""
    gap = b"\x00\x00" * int(RATE * JOIN_GAP)
    chunks = []
    for part in parts:
        chunks.append(normalize(trim(read(part)), target))
    data = gap.join(chunks)
    with wave.open(out_path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(data)
    seconds = len(data) / 2 / RATE
    print(f"{os.path.basename(out_path):32s} {seconds:5.2f} с  из {len(parts)} кусков")


def process(path, target=RMS_TARGET, dense=False):
    if dense:
        data = normalize(trim(read(path)), RMS_TARGET_DENSE, PEAK_LIMIT_DENSE)
    else:
        data = normalize(trim(read(path)), target)
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(data)
    seconds = len(data) / 2 / RATE
    if np is not None:
        samples = np.frombuffer(data, dtype=np.int16).astype(np.float32)
        rms, peak = int(np.sqrt(np.mean(samples ** 2))), int(np.max(np.abs(samples)))
    else:
        import audioop
        rms, peak = audioop.rms(data, 2), audioop.max(data, 2)
    print(f"{os.path.basename(path):32s} {seconds:5.2f} с  rms={rms:5d}  peak={peak:5d}")


def main():
    loud = "--loud" in sys.argv
    dense = "--dense" in sys.argv
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    if "--join" in sys.argv:
        if len(args) < 3:
            print("--join ждёт: out.wav part1.wav part2.wav [...]")
            return 1
        missing = [a for a in args[1:] if not os.path.exists(a)]
        if missing:
            print("нет файлов: " + ", ".join(missing))
            return 1
        join(args[0], args[1:], RMS_TARGET_LOUD if loud else RMS_TARGET)
        return 0
    paths = []
    for a in args:
        paths.extend(sorted(glob.glob(a)))
    if not paths:
        print(__doc__)
        return 1
    for p in paths:
        if dense:
            process(p, RMS_TARGET_DENSE, dense=True)
        else:
            process(p, RMS_TARGET_LOUD if loud else RMS_TARGET)
    return 0


if __name__ == "__main__":
    sys.exit(main())
