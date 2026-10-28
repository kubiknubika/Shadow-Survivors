#!/usr/bin/env python3
"""Проверка озвучки сообщений.

Читает реестр src/messages.h, смотрит, что лежит в audio/voice/, и печатает
таблицу: какие фразы записаны, каких файлов не хватает, где текст реестра
разошёлся с тем, что реально записано (см. VOICE_LINES.md).

    python3 tools/check_voice.py            # отчёт
    python3 tools/check_voice.py --strict   # код возврата 1, если чего-то не хватает
    python3 tools/check_voice.py --levels   # плюс контроль громкости всех записей
    python3 tools/check_voice.py --voice    # плюс контроль тембра: один ли голос
    python3 tools/check_voice.py --stress   # плюс поиск слов с плавающим ударением
    python3 tools/check_voice.py --alias    # плюс поиск «звона» от понижения частоты

Уровни важны не меньше, чем наличие файла: «одна фраза вдвое тише другой»
слышно сразу, а глазами в реестре не видно. Поэтому --levels перебирает все
клипы и ругается на те, что выбиваются из общего коридора громкости.
"""
import audioop
import math
import os
import re
import sys
import wave

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REGISTRY = os.path.join(ROOT, "src", "messages.h")
VOICE_DIR = os.path.join(ROOT, "audio", "voice")
CAMPAIGN_MISSIONS = 28  # столько же, сколько дел в src/game.h

ROW = re.compile(r'\{(GMSG_[A-Z0-9_]+),\s*"([a-z0-9_]+)",\s*(true|false),\s*"([^"]*)",\s*"([^"]*)"\}')


def registry_rows():
    with open(REGISTRY, encoding="utf-8") as fh:
        text = fh.read()
    return ROW.findall(text)


LEVEL_RMS_BAND = (3000, 4600)   # коридор RMS, в который попадает штатная запись
# Все реплики читает один рассказчик. Его медианная высота тона 93–119 Гц; клип,
# который вылез из этого коридора, записан другим голосом — и это слышно в игре,
# даже если громкость и формат в порядке. Поэтому тембр тоже проверяется.
VOICE_F0_BAND = (82, 130)

# Слова, на которых синтез уходит в чужое ударение: «сестрЫ» читают как «сЁстры»,
# «зАдал» вместо «задАл», «пИсарей» вместо «писарЕй» и так далее. Встретилось такое
# слово в уже записанном описании — переписываем фразу, а не надеемся на диктора.
# Доля энергии в полосе 7–8 кГц. У нормальной записи её почти нет (<1%);
# если больше, значит частоту понизили без фильтра и верхи «сложились» в звон,
# который на слух читается как лишние гласные посреди слова.
ALIAS_LIMIT = 1.2


def alias_ratio(path):
    try:
        import numpy as np
    except ImportError:
        return None
    with wave.open(path) as w:
        rate, channels = w.getframerate(), w.getnchannels()
        data = w.readframes(w.getnframes())
    if channels == 2:
        data = audioop.tomono(data, 2, 0.5, 0.5)
    x = np.frombuffer(data, dtype=np.int16).astype(np.float32)
    if rate != 16000 or len(x) == 0:
        return None
    spectrum = np.abs(np.fft.rfft(x))
    freqs = np.fft.rfftfreq(len(x), 1.0 / rate)
    total = float(spectrum.sum()) or 1.0
    return float(spectrum[(freqs >= 7000) & (freqs < 8000)].sum()) / total * 100.0


STRESS_TRAPS = {
    "сестры": "сестрЫ/сЁстры — переписать: «говорила сестра» или «родной голос»",
    "задал": "зАдал/задАл — заменить на «спрашивал»",
    "начал": "нАчал/начАл — переписать без этого глагола",
    "понял": "пОнял/понЯл — заменить на «услышал», «решил»",
    "отдал": "Отдал/отдАл — переписать",
    "писарей": "пИсарей/писарЕй — заменить на «писцов»",
    "тиснение": "тИснение — заменить на «оттиск»",
    "выбиты": "вЫбиты — заменить на «резаны»",
    "интонацию": "интонацИю — заменить на «голос»",
}


def clip_levels():
    """(файл, частота, rms) по всем записям в audio/voice/."""
    out = []
    for name in sorted(os.listdir(VOICE_DIR)):
        if not name.endswith(".wav"):
            continue
        path = os.path.join(VOICE_DIR, name)
        try:
            with wave.open(path) as w:
                rate = w.getframerate()
                channels = w.getnchannels()
                data = w.readframes(w.getnframes())
            if channels == 2:
                data = audioop.tomono(data, 2, 0.5, 0.5)
            out.append((name, rate, channels, audioop.rms(data, 2)))
        except Exception as exc:  # битый файл не должен валить проверку целиком
            out.append((name, 0, 0, -1))
            print(f"! {name}: не читается ({exc})")
    return out


def median_f0(path, decimate=2):
    """Медианная высота тона по озвученным кадрам, в герцах.

    Дешёвая автокорреляция на прореженном сигнале: точности хватает, чтобы
    отличить одного рассказчика от другого (95 Гц против 140–190 Гц), а
    полный разбор спектра здесь не нужен.
    """
    with wave.open(path) as w:
        rate, channels = w.getframerate(), w.getnchannels()
        data = w.readframes(w.getnframes())
    if channels == 2:
        data = audioop.tomono(data, 2, 0.5, 0.5)
    samples = [int.from_bytes(data[i * 2:i * 2 + 2], "little", signed=True) for i in range(len(data) // 2)]
    samples = [(samples[i] + samples[i + 1]) * 0.5 for i in range(0, len(samples) - 1, decimate)]
    rate /= decimate
    window, hop = int(rate * 0.04), int(rate * 0.02)
    low, high = int(rate / 350), int(rate / 70)
    found = []
    for start in range(0, max(0, len(samples) - window), hop):
        frame = samples[start:start + window]
        if math.sqrt(sum(v * v for v in frame) / window) < 1200:
            continue
        energy = sum(v * v for v in frame) or 1.0
        corr = []
        for lag in range(low, high):
            tail = sum(frame[i] * frame[i] for i in range(window - lag)) or 1.0
            value = sum(frame[i] * frame[i + lag] for i in range(window - lag)) / math.sqrt(energy * tail)
            corr.append((value, lag))
        peak = max(c[0] for c in corr)
        if peak < 0.45:
            continue
        for value, lag in corr:          # первый сильный максимум = основной тон, не субгармоника
            if value > 0.85 * peak:
                found.append(rate / lag)
                break
    found.sort()
    return found[len(found) // 2] if found else 0.0


def stress_warnings():
    """Записанные описания, в которых остались слова с плавающим ударением."""
    sys.path.insert(0, os.path.join(ROOT, "tools"))
    try:
        import campaign
    except Exception:
        return []
    out = []
    for i, mission in enumerate(campaign.MISSIONS):
        key = f"mission_brief_{i:02d}"
        if not any(os.path.exists(os.path.join(VOICE_DIR, key + ext)) for ext in (".wav", ".mp3")):
            continue
        text = mission["briefing"].lower()
        for stem, why in STRESS_TRAPS.items():
            if re.search(r"\b" + stem, text):
                out.append((key, stem, why))
    return out


def main(argv):
    rows = registry_rows()
    if not rows:
        print("не нашёл строк реестра в src/messages.h")
        return 1
    present, missing, silent = [], [], []
    for enum, key, voiced, line, _note in rows:
        path = None
        for ext in (".wav", ".mp3"):
            candidate = os.path.join(VOICE_DIR, key + ext)
            if os.path.exists(candidate):
                path = candidate
                break
        flag = voiced == "true"
        if not flag:
            silent.append((key, line))
            if line:
                print(f"! {key}: помечено как «молчит», но у фразы есть текст")
            continue
        if not line:
            print(f"! {key}: помечено как «озвучивается», но текст фразы пуст")
        (present if path else missing).append((key, line))

    print(f"озвучивается: {len(present) + len(missing)}   на месте: {len(present)}   нет файла: {len(missing)}   молчит: {len(silent)}")

    # Отдельный банк: у каждого Дела кампании своя запись описания.
    briefs = 0
    brief_missing = []
    for i in range(CAMPAIGN_MISSIONS):
        key = f"mission_brief_{i:02d}"
        if any(os.path.exists(os.path.join(VOICE_DIR, key + ext)) for ext in (".wav", ".mp3")):
            briefs += 1
        else:
            brief_missing.append(key)
    print(f"описания дел: {briefs} из {CAMPAIGN_MISSIONS}")
    if brief_missing and ("--verbose" in argv or len(brief_missing) <= 5):
        print("  без записи: " + ", ".join(brief_missing))
    levels_bad = []
    if "--levels" in argv:
        levels = clip_levels()
        low, high = LEVEL_RMS_BAND
        for name, rate, channels, rms in levels:
            if rate and (rate != 16000 or channels != 1):
                levels_bad.append((name, f"формат {rate} Гц/{channels} кан."))
            elif rms >= 0 and not (low <= rms <= high):
                levels_bad.append((name, f"громкость rms={rms} вне коридора {low}..{high}"))
        print(f"уровни: проверено {len(levels)} записей, вне коридора {len(levels_bad)}")
        for name, why in levels_bad:
            print(f"  ! {name}: {why}")
        if levels_bad:
            print("  лечится: python3 tools/normalize_voice.py audio/voice/<файл>.wav")

    voice_bad = []
    if "--voice" in argv:
        low, high = VOICE_F0_BAND
        checked = 0
        for name in sorted(os.listdir(VOICE_DIR)):
            if not name.endswith(".wav"):
                continue
            checked += 1
            hz = median_f0(os.path.join(VOICE_DIR, name))
            if hz and not (low <= hz <= high):
                voice_bad.append((name, hz))
        print(f"тембр: проверено {checked}, другим голосом {len(voice_bad)}")
        for name, hz in voice_bad:
            print(f"  ! {name}: {hz:.0f} Гц (рассказчик ~95 Гц) — перечитать этим же голосом")

    if "--stress" in argv:
        traps = stress_warnings()
        print(f"ударения: записей с рискованными словами {len(traps)}")
        for key, stem, why in traps:
            print(f"  ! {key}: «{stem}» — {why}")

    alias_bad = []
    if "--alias" in argv:
        for name in sorted(os.listdir(VOICE_DIR)):
            if not name.endswith(".wav"):
                continue
            value = alias_ratio(os.path.join(VOICE_DIR, name))
            if value is not None and value > ALIAS_LIMIT:
                alias_bad.append((name, value))
        print(f"звон: проверено клипов, вне нормы {len(alias_bad)}")
        for name, value in alias_bad:
            print(f"  ! {name}: {value:.2f}% энергии в 7–8 кГц — перечитать из исходника")
        if alias_bad:
            print("  причина: понижение частоты без фильтра (см. tools/normalize_voice.py)")

    if missing:
        print("\nне хватает файлов в audio/voice/:")
        for key, line in missing:
            print(f"  {key}.wav/mp3   — {line}")
    if present:
        print("\nзаписано:")
        for key, line in present:
            print(f"  {key}.wav/mp3   — {line}")
    if silent:
        print("\nнамеренно без голоса:")
        for key, _ in silent:
            print(f"  {key}")
    if "--strict" in argv and missing:
        return 1
    if "--levels" in argv and levels_bad:
        return 1
    if "--voice" in argv and voice_bad:
        return 1
    if "--stress" in argv and "--strict" in argv and stress_warnings():
        return 1
    if "--alias" in argv and alias_bad:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
