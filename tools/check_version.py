#!/usr/bin/env python3
"""Проверка того, что номер версии нигде не разъехался.

Одна и та же цифра живёт в четырёх местах, и именно из-за рассинхрона сборка
показывала 2.9.78, пока игра уходила вперёд:

  * src/version.h      — источник правды (по нему собирается exe)
  * CHANGELOG.md       — заголовок и первая секция
  * src/game.h          — последняя запись в versions[] (её видно в хронике)
  * src/game.h          — последняя запись в versions[] (её видно в хронике)

Размеры versions[]/notes[] сверяет static_assert в src/game.h, а не этот скрипт.

    python3 tools/check_version.py            # отчёт
    python3 tools/check_version.py --strict   # код 1, если что-то не сходится
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VERSION_H = os.path.join(ROOT, "src", "version.h")
GAME_H = os.path.join(ROOT, "src", "game.h")
CHANGELOG = os.path.join(ROOT, "CHANGELOG.md")

SEMVER = r"\d+\.\d+\.\d+"


def read(path):
    with open(path, encoding="utf-8") as fh:
        return fh.read()


def updater_problems():
    """Проверки update.bat через его сборщик: кодировка, метки, номер."""
    here = os.path.dirname(os.path.abspath(__file__))
    if here not in sys.path:
        sys.path.insert(0, here)
    try:
        import build_update_bat
    except ImportError:
        return []
    problems = [f"update.bat: {p}" for p in build_update_bat.collect_problems()]
    expected = build_update_bat.build_bytes()
    current = None
    if os.path.exists(build_update_bat.OUT):
        with open(build_update_bat.OUT, "rb") as fh:
            current = fh.read()
    if expected is not None and current != expected:
        problems.append("update.bat: файл не совпадает с tools/update_bat_src.txt "
                        "(python3 tools/build_update_bat.py)")
    return problems


def main(argv):
    strict = "--strict" in argv
    problems = []

    version_h = read(VERSION_H)
    version = re.search(r'#define GAME_VERSION\s+"(' + SEMVER + r')"', version_h)
    number = re.search(r"#define GAME_VERSION_NUM\s+(\d+)", version_h)
    if not version or not number:
        print("не нашёл GAME_VERSION/GAME_VERSION_NUM в src/version.h")
        return 1
    version = version.group(1)
    number = int(number.group(1))

    major, minor, patch = (int(part) for part in version.split("."))
    expected = major * 1000 + minor * 100 + patch
    if number != expected:
        problems.append(f"GAME_VERSION_NUM={number}, а из {version} следует {expected}")

    game_h = read(GAME_H)

    # Размеры массивов versions[]/notes[] и PATCH_NOTES_COUNT сверяет компилятор
    # (static_assert в src/game.h). Здесь важно другое: последняя запись хроники,
    # которую видит игрок, должна называть текущую версию.
    versions_block = re.search(r"static const char\* const versions\[\]\s*=\s*\{(.*?)\n\s*\};",
                               game_h, re.S)
    if not versions_block:
        problems.append("не нашёл массив versions[] в src/game.h")
        last_version = None
    else:
        entries = re.findall('"(' + SEMVER + '(?:–' + SEMVER + ')?)[^"]*"', versions_block.group(1))
        last_version = entries[-1] if entries else None
        if not last_version:
            problems.append("в versions[] нет ни одной записи")
        elif last_version.split("–")[-1] != version:
            problems.append(f"последняя запись хроники {last_version}, а версия {version}")

    changelog = read(CHANGELOG)
    head = re.search(r"^# .*?(" + SEMVER + r")\+?\s*$", changelog, re.M)
    if not head:
        problems.append("в CHANGELOG.md нет заголовка с версией")
    elif head.group(1) != version:
        problems.append(f"заголовок CHANGELOG.md про {head.group(1)}, а версия {version}")
    section = re.search(r"^## (" + SEMVER + r")", changelog, re.M)
    if section and section.group(1) != version:
        problems.append(f"первая секция CHANGELOG.md про {section.group(1)}, а версия {version}")

    # Апдейтер: исходник в UTF-8, сам файл в cp866. Если он разъехался или в
    # тексте появилась буква, которой в cp866 нет, игрок увидит крякозябры.
    problems += updater_problems()

    print(f"версия: {version}   код: {number}   последняя запись хроники: {last_version}   "
          f"CHANGELOG: {section.group(1) if section else '—'}")
    if problems:
        print("\nрасхождения:")
        for problem in problems:
            print(f"  ! {problem}")
        return 1 if strict else 0
    print("всё сходится")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
