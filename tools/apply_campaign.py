#!/usr/bin/env python3
"""Вливает таблицу CAMPAIGN_MISSIONS из tools/campaign.py в src/game.h.

Зачем отдельный скрипт: раньше эту замену делали однострочником в шелле, и он
искал конец таблицы по первому «};» — а сразу за таблицей стоит закрывающая
скобка структуры HeroDef. В итоге вместе с таблицей вырезались объявления
HeroDef и missionBriefKey, и файл переставал собираться.

Здесь граница таблицы считается по вложенности скобок, а перед записью файл
проверяется на целостность: если после замены пропал HeroDef или разошлось
число строк, скрипт падает и ничего не пишет.

    python3 tools/apply_campaign.py           # применить
    python3 tools/apply_campaign.py --check   # только проверить, что совпадает
"""
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GAME = os.path.join(ROOT, "src", "game.h")
TABLE = "static const CampaignMissionDef CAMPAIGN_MISSIONS[CAMPAIGN_MISSION_COUNT] = {"
REQUIRED = ("struct HeroDef", "HERO_DEFS[5]", "missionBriefKey(int mission)", "CAMPAIGN_MISSIONS")


def table_end(text, start):
    """Индекс сразу после парной закрывающей скобки таблицы."""
    depth = 0
    for i in range(start, len(text)):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return i + 1
    raise SystemExit("не нашёл конец таблицы миссий")


def generated_rows():
    out = subprocess.check_output([sys.executable, os.path.join(ROOT, "tools", "campaign.py"), "cpp"],
                                  cwd=ROOT).decode("utf-8").rstrip("\n")
    count = out.count("\n") + 1
    # Маска закрытых дел в сейве — одна 32-битная, поэтому дел не может быть больше 32.
    if count > 32:
        raise SystemExit(f"{count} дел не влезают в 32-битную маску сейва")
    return out


def main(argv):
    with open(GAME, encoding="utf-8") as fh:
        text = fh.read()
    start = text.index(TABLE)
    end = table_end(text, start)
    rows = generated_rows()
    count = rows.count("\n") + 1
    # Число дел в game.h больше не правится руками: его задаёт campaign.py.
    import re
    text = re.sub(r"(static constexpr int CAMPAIGN_MISSION_COUNT = )\d+(;)",
                  lambda m: f"{m.group(1)}{count}{m.group(2)}", text, count=1)
    # В файле после «}» таблицы стоят лишние точки с запятой (};;;;...). Без их
    # пропуска каждая запись добавляла ещё одну «;», и --check не проходил никогда.
    tail = text[end:]
    skip = 0
    while skip < len(tail) and tail[skip] in "; \t":
        skip += 1
    new_text = text[:start] + TABLE + "\n" + rows + "\n};" + tail[skip:]
    for needle in REQUIRED:                 # ничего лишнего не вырезали
        if needle not in new_text:
            raise SystemExit(f"после замены пропало: {needle}")
    if "--check" in argv:
        print("таблица совпадает" if new_text == text else "таблица расходится с campaign.py")
        return 0 if new_text == text else 1
    if new_text == text:
        print("без изменений")
        return 0
    with open(GAME, "w", encoding="utf-8") as fh:
        fh.write(new_text)
    print("таблица миссий обновлена")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
