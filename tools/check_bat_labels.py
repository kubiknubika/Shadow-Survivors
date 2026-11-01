#!/usr/bin/env python3
"""Сверяет goto/call с метками в .bat-файлах.

Зачем: в cmd переход на несуществующую метку — не ошибка, а тихий переход к
следующей строке. Скрипт, у которого после `goto` уехала строка, продолжает
работать и делает не то, что написано; глазами это не поймать.

    python3 tools/check_bat_labels.py update.bat build_windows.bat
"""
import re
import sys

REF_GOTO = re.compile(r'(?<![-\w])goto\s+([^\s&|>]+)', re.IGNORECASE)
REF_CALL = re.compile(r'(?<![-\w])call\s+:([^\s&|>"]+)', re.IGNORECASE)
LABEL = re.compile(r'^\s*:([^\s:]+)')
SPECIAL = {'eof'}


def labels(text):
    out = set()
    for line in text.splitlines():
        m = LABEL.match(line)
        if m:
            out.add(m.group(1).lower())
    return out


def refs(text):
    """(имя, номер строки) для каждого goto/call :метки."""
    found = []
    for n, line in enumerate(text.splitlines(), 1):
        stripped = line.strip()
        if stripped.lower().startswith('rem '):
            continue
        for m in REF_GOTO.finditer(line):
            found.append((m.group(1), n, 'goto'))
        for m in REF_CALL.finditer(line):
            found.append((m.group(1), n, 'call'))
    return found


def check(path):
    try:
        text = open(path, encoding='utf-8', errors='replace').read()
    except OSError as exc:
        print(f'{path}: не открылся: {exc}')
        return 1
    have = labels(text)
    bad = 0
    for name, line, kind in refs(text):
        if name.startswith(':'):
            name = name[1:]
        key = name.lower()
        if key in SPECIAL or key in have:
            continue
        print(f'{path}:{line}: {kind} {name} — такой метки в файле нет')
        bad += 1
    if bad == 0:
        print(f'{path}: метки в порядке ({len(have)} шт.)')
    return bad


def main(argv):
    paths = argv[1:] or ['update.bat', 'build_windows.bat']
    total = 0
    for path in paths:
        total += check(path)
    if total:
        print(f'ПРОВАЛОВ: {total}')
        return 1
    print('ОК: все переходы ведут на существующие метки')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
