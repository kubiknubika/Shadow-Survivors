#!/usr/bin/env python3
"""Проверка озвучки сообщений.

Читает реестр src/messages.h, смотрит, что лежит в audio/voice/, и печатает
таблицу: какие фразы записаны, каких файлов не хватает, где текст реестра
разошёлся с тем, что реально записано (см. VOICE_LINES.md).

    python3 tools/check_voice.py            # отчёт
    python3 tools/check_voice.py --strict   # код возврата 1, если чего-то не хватает
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REGISTRY = os.path.join(ROOT, "src", "messages.h")
VOICE_DIR = os.path.join(ROOT, "audio", "voice")
CAMPAIGN_MISSIONS = 28  # столько же, сколько дел в src/game.h

ROW = re.compile(r'\{(GMSG_[A-Z0-9_]+),\s*"([a-z0-9_]+)",\s*(true|false),\s*"([^"]*)",\s*"([^"]*)"\}')


def registry_rows():
    with open(REGISTRY, encoding="utf-8") as fh:
        text = fh.read()
    return ROW.findall(text)


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
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
