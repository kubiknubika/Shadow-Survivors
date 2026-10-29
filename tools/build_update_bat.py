#!/usr/bin/env python3
"""Собирает update.bat из читаемого исходника tools/update_bat_src.txt.

Зачем сборщик, а не один файл.

cmd.exe читает .bat в кодовой странице консоли, а не в UTF-8. Файл, сохранённый
редактором в UTF-8, показывает русский текст крякозябрами, а часть строк вообще
выполняет как команды. Поэтому в репозитории лежат два файла:

  * tools/update_bat_src.txt - обычный UTF-8, его правят руками, diff читается;
  * update.bat                - cp866 с CRLF, его запускает игрок.

Сборщик проверяет то, что руками не проверишь:

  * каждая буква влезает в cp866 (иначе Word/Notion вставят «ёлочки» или тире
    и файл молча сломается);
  * кириллица стоит только в echo, rem и в маячке set "PROBE=...";
    в if/for/goto/call её быть не должно - именно такие строки cmd разбирает
    как команды и падает;
  * нет кириллицы внутри многострочного блока if ( ... );
  * каждая goto находит свою метку;
  * SELF_VER совпадает с GAME_VERSION из src/version.h.

    python3 tools/build_update_bat.py           # перезаписать update.bat
    python3 tools/build_update_bat.py --check   # код 1, если файл устарел
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "tools", "update_bat_src.txt")
OUT = os.path.join(ROOT, "update.bat")
VERSION_H = os.path.join(ROOT, "src", "version.h")

ALLOWED_WITH_CYRILLIC = ("echo", "rem", "set")


def load_source():
    with open(SRC, encoding="utf-8") as fh:
        text = fh.read()
    # Исходник может быть сохранён и с CRLF, и с LF: на выходе всё равно CRLF.
    return text.replace("\r\n", "\n").replace("\r", "\n")


def check_charset(lines):
    problems = []
    for number, line in enumerate(lines, 1):
        for char in line:
            if char in "\n":
                continue
            try:
                char.encode("cp866")
            except UnicodeEncodeError:
                problems.append(
                    f"строка {number}: символ {char!r} не существует в cp866 "
                    f"(U+{ord(char):04X}) - замени его на - или \"")
    return problems


def check_cyrillic(lines):
    problems = []
    depth = 0
    for number, line in enumerate(lines, 1):
        stripped = line.strip()
        lowered = stripped.lower()
        # Строка может начинаться с перенаправления: >> "%LOG%" echo текст.
        rest = re.sub(r'^(?:>>?|2>>?)\s*(?:"[^"]*"|\S+)\s*', "", lowered, count=1)
        command = rest.split(None, 1)[0].rstrip("(") if rest else ""
        has_cyrillic = any("а" <= ch.lower() <= "я" or ch.lower() == "ё" for ch in line)
        if has_cyrillic:
            if command not in ALLOWED_WITH_CYRILLIC:
                problems.append(
                    f"строка {number}: кириллица в команде {command!r} - "
                    f"cmd разберёт такую строку как команду: {stripped[:60]}")
            elif depth > 0:
                problems.append(
                    f"строка {number}: кириллица внутри блока if ( ... ) - "
                    f"такой блок разваливается: {stripped[:60]}")
            elif command == "set" and not re.match(r'set\s+"?PROBE=', stripped):
                problems.append(
                    f"строка {number}: кириллица в set допустима только для "
                    f"маячка PROBE: {stripped[:60]}")
        if command in ("echo", "rem"):
            continue
        depth += stripped.count("(") - stripped.count(")")
        if depth < 0:
            depth = 0
    return problems


def check_labels(lines):
    labels = set()
    gotos = set()
    for line in lines:
        stripped = line.strip()
        if re.match(r"^:[\w-]+$", stripped):
            labels.add(stripped[1:])
        for target in re.findall(r"\bgoto\s+:?([\w-]+)", stripped, re.I):
            if target.lower() != "eof":
                gotos.add(target)
    missing = sorted(gotos - labels)
    return [f"goto на несуществующую метку: {name}" for name in missing]


def check_self_version(lines):
    version = None
    with open(VERSION_H, encoding="utf-8") as fh:
        found = re.search(r'#define GAME_VERSION\s+"([\d.]+)"', fh.read())
        if found:
            version = found.group(1)
    marker = None
    for line in lines:
        if line.startswith("set SELF_VER="):
            marker = line.split("=", 1)[1].strip()
    if version and marker and version != marker:
        return [f"SELF_VER={marker}, а игра собирается как {version}"]
    return []


def source_lines():
    lines = load_source().split("\n")
    if lines and lines[-1] == "":
        lines = lines[:-1]
    return lines


def collect_problems():
    """Все нарушения в исходнике апдейтера - для сборщика и для check_version."""
    lines = source_lines()
    return (check_charset(lines) + check_cyrillic(lines)
            + check_labels(lines) + check_self_version(lines))


def build_bytes():
    """Готовые байты update.bat или None, если исходник не проходит проверки."""
    if collect_problems():
        return None
    return ("\r\n".join(source_lines()) + "\r\n").encode("cp866")


def main(argv):
    check_only = "--check" in argv
    lines = source_lines()
    problems = collect_problems()
    if problems:
        print("update.bat не собран:")
        for problem in problems:
            print("  " + problem)
        return 1

    payload = ("\r\n".join(lines) + "\r\n").encode("cp866")
    current = None
    if os.path.exists(OUT):
        with open(OUT, "rb") as fh:
            current = fh.read()
    rows = payload.count(b"\r\n")
    if current == payload:
        print(f"update.bat в порядке: {len(payload)} байт, {rows} строк, cp866")
        return 0
    if check_only:
        print("update.bat не совпадает с tools/update_bat_src.txt: "
              "запусти python3 tools/build_update_bat.py")
        return 1
    with open(OUT, "wb") as fh:
        fh.write(payload)
    print(f"update.bat собран: {len(payload)} байт, {rows} строк, cp866, CRLF")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
