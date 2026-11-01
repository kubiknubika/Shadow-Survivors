#!/usr/bin/env python3
"""Проверка Android-сборки без Windows, JDK и NDK.

Собирать APK в этой среде нечем, поэтому проверяется всё, что видно из файлов,
и ровно то, что чаще всего ломается тихо:

  * build_android.bat — кодировка, CRLF, метки и переходы, баланс скобок,
    кавычки в путях, объявленность переменных с задержанным раскрытием;
  * android/ — что проект вообще ссылается на существующие файлы, что
    applicationId/lib_name/ndkVersion/minSdk сходятся между gradle, CMake и
    манифестом, что каталог src/ находится из CMake на ту глубину, на которую
    рассчитывает сборка;
  * ANDROID_BUILD.md и design/android.md — что инструкция не врёт: каждый
    файл и каталог, на который она ссылается, существует, а номера версий и
    SDK в тексте совпадают с проектом;
  * src/version.h против android/app/build.gradle — что APK по умолчанию
    собирается с той же версией, что exe и хроника.

    python3 tools/check_android_bat.py           # отчёт
    python3 tools/check_android_bat.py --strict  # код 1 при расхождениях
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BAT = os.path.join(ROOT, "build_android.bat")
GRADLE_APP = os.path.join(ROOT, "android", "app", "build.gradle")
GRADLE_ROOT = os.path.join(ROOT, "android", "build.gradle")
CMAKE = os.path.join(ROOT, "android", "app", "src", "main", "cpp", "CMakeLists.txt")
MANIFEST = os.path.join(ROOT, "android", "app", "src", "main", "AndroidManifest.xml")
VERSION_H = os.path.join(ROOT, "src", "version.h")
DOCS = [os.path.join(ROOT, "ANDROID_BUILD.md"), os.path.join(ROOT, "design", "android.md"),
        os.path.join(ROOT, "README.md")]
RES = os.path.join(ROOT, "android", "app", "src", "main", "res")
STRINGS = os.path.join(RES, "values", "strings.xml")
DENSITIES = {"mipmap-mdpi": 48, "mipmap-hdpi": 72, "mipmap-xhdpi": 96,
             "mipmap-xxhdpi": 144, "mipmap-xxxhdpi": 192}


def read(path):
    with open(path, encoding="utf-8") as fh:
        return fh.read()


# ── build_android.bat ───────────────────────────────────────────────────────

# Команды, которые в bat должны раскрываться задержанно (!x!), потому что
# стоят внутри блока. Список нужен только для сообщения, поэтому краткий.
BAT_REQUIRED_SNIPPETS = [
    ("chcp 65001", "переключение кодовой страницы: файл сохранён в UTF-8"),
    ("setlocal enabledelayedexpansion", "без него !переменные! внутри блоков не работают"),
    ("src\\version.h", "версия должна браться из того же файла, что у exe"),
    ("-PappVersion=", "версия APK иначе разойдётся с игрой"),
    ("-PappVersionCode=", "versionCode должен расти вместе с версией"),
    ("local.properties", "Gradle не найдёт SDK без этого файла"),
    ("assembleDebug", "сборка по умолчанию"),
    ("assembleRelease", "release-сборка"),
    ("android_build.log", "журнал, без него разбирать нечего"),
    ("pause", "окно не должно закрываться само"),
    ("install -r", "установка на телефон через adb"),
]


def strip_quotes_and_rem(line):
    """Строка без rem и без содержимого кавычек — для подсчёта скобок."""
    s = line.strip()
    if s.lower().startswith("rem ") or s.lower() == "rem" or s.startswith("::"):
        return ""
    out = []
    in_quote = False
    for ch in s:
        if ch == '"':
            in_quote = not in_quote
            continue
        if not in_quote:
            out.append(ch)
    return "".join(out)


def check_bat():
    problems = []
    if not os.path.exists(BAT):
        return ["build_android.bat отсутствует"]
    raw = open(BAT, "rb").read()

    if raw[:3] == b"\xef\xbb\xbf":
        problems.append("build_android.bat: UTF-8 BOM в начале — cmd покажет мусор")
    try:
        text = raw.decode("utf-8")
    except UnicodeDecodeError as exc:
        return problems + ["build_android.bat: не UTF-8 (%s)" % exc]

    crlf = raw.count(b"\r\n")
    lf = raw.count(b"\n")
    if crlf != lf:
        problems.append("build_android.bat: %d строк, из них CRLF %d — смешанные "
                        "переводы строк, cmd падает на блоках" % (lf, crlf))

    lines = text.replace("\r\n", "\n").split("\n")

    # метки и переходы
    labels = set()
    gotos = []
    calls = []
    for line in lines:
        stripped = line.strip()
        m = re.match(r"^:([\w-]+)\s*$", stripped)
        if m:
            labels.add(m.group(1).lower())
        for target in re.findall(r"\bgoto\s+:?([\w-]+)", stripped, re.I):
            gotos.append(target)
        for target in re.findall(r"\bcall\s+:?([\w-]+)", stripped, re.I):
            calls.append(target)
    for target in gotos + calls:
        if target.lower() not in labels and target.lower() != "eof":
            problems.append("build_android.bat: переход на несуществующую метку :%s" % target)

    # баланс скобок вне кавычек и rem
    depth = 0
    for number, line in enumerate(lines, 1):
        bare = strip_quotes_and_rem(line)
        opened = bare.count("(") - bare.count("^(")
        closed = bare.count(")") - bare.count("^)")
        depth += opened - closed
        if depth < 0:
            problems.append("build_android.bat:%d: лишняя закрывающая скобка" % number)
            depth = 0
    if depth != 0:
        problems.append("build_android.bat: скобки не сбалансированы (остаток %+d)" % depth)

    # set "X=..." должен иметь закрывающую кавычку
    for number, line in enumerate(lines, 1):
        if re.match(r'^\s*set\s+"[^"]+=', line) and line.count('"') % 2 != 0:
            problems.append("build_android.bat:%d: незакрытая кавычка в set" % number)

    # задержанное раскрытие: !x! должен где-то присваиваться
    assigned = set(m.lower() for m in re.findall(
        r'set\s+(?:/a\s+)?"?([A-Za-z_][\w]*)=', text))
    used_delayed = set(m.lower() for m in re.findall(r"!([A-Za-z_][\w]*)!", text))
    for name in sorted(used_delayed - assigned):
        # %~dp0, loop-переменные и аргументы сюда не попадают
        problems.append("build_android.bat: !%s! используется, но нигде не задан" % name)

    for snippet, why in BAT_REQUIRED_SNIPPETS:
        if snippet not in text:
            problems.append("build_android.bat: нет «%s» — %s" % (snippet, why))

    # путь к java.exe может содержать пробелы: команда не должна стоять
    # внутри for /f в кавычках
    if re.search(r"for\s+/f[^\n]*in\s*\(\s*'\"", text):
        problems.append("build_android.bat: команда в кавычках внутри for /f — "
                        "путь с пробелами развалится")

    return problems


# ── android/ ────────────────────────────────────────────────────────────────

def check_project():
    problems = []
    for path in (GRADLE_APP, GRADLE_ROOT, CMAKE, MANIFEST):
        if not os.path.exists(path):
            problems.append("нет файла %s" % os.path.relpath(path, ROOT))
    if problems:
        return problems

    gradle = read(GRADLE_APP)
    cmake = read(CMAKE)
    manifest = read(MANIFEST)

    # lib_name в манифесте = имя CMake-цели (иначе система не найдёт .so)
    lib = re.search(r'android:name="android\.app\.lib_name"\s+android:value="([\w]+)"', manifest)
    target = re.search(r"add_library\(\s*([\w]+)", cmake)
    if not lib or not target:
        problems.append("не нашёл lib_name в манифесте или add_library в CMakeLists")
    elif lib.group(1) != target.group(1):
        problems.append("манифест ждёт lib%s.so, а CMake собирает %s"
                        % (lib.group(1), target.group(1)))

    # CMake должен находить src/ на ту глубину, на которую рассчитывает
    depth = re.search(r"CMAKE_CURRENT_SOURCE_DIR\}/(\.\./)+src", cmake)
    if depth:
        ups = depth.group(0).count("../")
        cmake_dir = os.path.dirname(CMAKE)
        resolved = os.path.normpath(os.path.join(cmake_dir, *([".."] * ups), "src"))
        if not os.path.isdir(resolved) or os.path.normpath(resolved) != os.path.join(ROOT, "src"):
            problems.append("CMakeLists поднимается на %d уровня и попадает в %s, а не в src/"
                            % (ups, resolved))
    else:
        problems.append("CMakeLists: не вижу ссылки на ../../src")

    # main_android.cpp должен быть тем самым исходником
    if "main_android.cpp" not in cmake:
        problems.append("CMakeLists не собирает src/main_android.cpp")

    # applicationId
    app_id = re.search(r"applicationId\s+'([\w.]+)'", gradle)
    if not app_id:
        problems.append("build.gradle: нет applicationId")
    else:
        # его упоминает инструкция (adb uninstall / run-as)
        doc = read(DOCS[0]) if os.path.exists(DOCS[0]) else ""
        if app_id.group(1) not in doc:
            problems.append("ANDROID_BUILD.md не упоминает applicationId %s" % app_id.group(1))

    # ndkVersion должен быть тем же, что назван в инструкции
    ndk = re.search(r"ndkVersion\s+'([\d.]+)'", gradle)
    cfg = re.search(r"ndkVersion\s+[\"']([\d.]+)[\"']", read(CMAKE))  # на случай переноса
    ndk_name = (ndk or cfg)
    if ndk_name:
        doc = read(DOCS[0]) if os.path.exists(DOCS[0]) else ""
        if ndk_name.group(1) not in doc:
            problems.append("ANDROID_BUILD.md называет другую версию NDK, чем %s"
                            % ndk_name.group(1))

    # SDK-уровни в инструкции
    doc = read(DOCS[0]) if os.path.exists(DOCS[0]) else ""
    for key, word in (("minSdk", "8.0"), ("targetSdk", "34")):
        m = re.search(key + r"\s+(\d+)", gradle)
        if m and key == "targetSdk" and m.group(1) != word:
            problems.append("build.gradle: %s %s, а инструкция говорит про API %s"
                            % (key, m.group(1), word))
        if m and key == "minSdk" and m.group(1) != "26":
            problems.append("build.gradle: minSdk %s — AAudio требует 26" % m.group(1))

    # abiFilters и текст про архитектуру
    abi = re.search(r"abiFilters\s+'([\w-]+)'", gradle)
    if not abi:
        problems.append("build.gradle: не задан abiFilters")
    elif abi.group(1) not in doc:
        problems.append("ANDROID_BUILD.md не говорит, что собирается только %s"
                        % abi.group(1))

    # hasCode=false обязателен: Java-кода в проекте нет
    if 'android:hasCode="false"' not in manifest:
        problems.append("манифест: нет android:hasCode=\"false\" при NativeActivity без Java")

    # версия по умолчанию в gradle = версия игры
    version_h = read(VERSION_H)
    game_ver = re.search(r'#define GAME_VERSION\s+"([\d.]+)"', version_h).group(1)
    game_code = re.search(r"#define GAME_VERSION_NUM\s+(\d+)", version_h).group(1)
    dflt_ver = re.search(r"findProperty\('appVersion'\)\s*\?:\s*'([\d.]+)'", gradle)
    dflt_code = re.search(r"findProperty\('appVersionCode'\)\s*\?:\s*'(\d+)'", gradle)
    if dflt_ver and dflt_ver.group(1) != game_ver:
        problems.append("build.gradle: версия по умолчанию %s, а в src/version.h %s"
                        % (dflt_ver.group(1), game_ver))
    if dflt_code and dflt_code.group(1) != game_code:
        problems.append("build.gradle: versionCode по умолчанию %s, а в src/version.h %s"
                        % (dflt_code.group(1), game_code))

    return problems


# ── ресурсы: XML и иконки ───────────────────────────────────────────────────

def check_resources():
    """Манифест и строки — валидный XML, иконки — PNG нужного размера.

    Сломанный XML Gradle ловит сам, но сообщение будет про aapt2 и без
    указания на строку; PNG неверного размера система просто не покажет.
    """
    problems = []
    import xml.etree.ElementTree as ET
    for path in (MANIFEST, STRINGS):
        if not os.path.exists(path):
            problems.append("нет %s" % os.path.relpath(path, ROOT))
            continue
        try:
            ET.parse(path)
        except ET.ParseError as exc:
            problems.append("%s: не разбирается как XML (%s)"
                            % (os.path.relpath(path, ROOT), exc))

    # размер PNG читается из IHDR — без сторонних библиотек
    for folder, size in DENSITIES.items():
        icon = os.path.join(RES, folder, "ic_launcher.png")
        if not os.path.exists(icon):
            problems.append("нет иконки %s/ic_launcher.png" % folder)
            continue
        with open(icon, "rb") as fh:
            head = fh.read(26)
        if head[:8] != b"\x89PNG\r\n\x1a\n":
            problems.append("%s/ic_launcher.png: не PNG" % folder)
            continue
        width = int.from_bytes(head[16:20], "big")
        height = int.from_bytes(head[20:24], "big")
        if (width, height) != (size, size):
            problems.append("%s/ic_launcher.png: %dx%d, ожидалось %dx%d"
                            % (folder, width, height, size, size))

    # иконка в манифесте должна существовать
    manifest = read(MANIFEST) if os.path.exists(MANIFEST) else ""
    if "@mipmap/ic_launcher" in manifest and not os.path.exists(
            os.path.join(RES, "mipmap-mdpi", "ic_launcher.png")):
        problems.append("манифест ссылается на @mipmap/ic_launcher, а файла нет")

    # название приложения
    if "@string/app_name" in manifest:
        strings = read(STRINGS) if os.path.exists(STRINGS) else ""
        if "app_name" not in strings:
            problems.append("манифест ждёт @string/app_name, а в strings.xml его нет")
    return problems


# ── документация ────────────────────────────────────────────────────────────

def check_docs():
    problems = []
    for doc_path in DOCS:
        if not os.path.exists(doc_path):
            problems.append("нет файла %s" % os.path.relpath(doc_path, ROOT))
            continue
        text = read(doc_path)
        # всё, что упомянуто в обратных кавычках и похоже на путь в репозитории
        for token in re.findall(r"`([^`]+)`", text):
            token = token.strip()
            if not re.match(r"^[\w./\\-]+$", token):
                continue
            if not re.search(r"[./\\]", token):
                continue
            if token.startswith(("--", "-", "android.", "android:", "http")):
                continue
            if token.endswith((".md", ".bat", ".gradle", ".h", ".cpp", ".txt", ".png",
                               ".ico", ".java", ".xml", ".properties")) or "/" in token:
                rel = token.replace("\\", "/").strip("/")
                if rel.startswith("../"):
                    candidates = [os.path.join(os.path.dirname(doc_path), rel)]
                else:
                    # Путь может быть от корня репозитория или от каталога,
                    # о котором идёт речь в разделе (android/, android/app/src/main).
                    candidates = [os.path.join(ROOT, rel),
                                  os.path.join(os.path.dirname(doc_path), rel),
                                  os.path.join(ROOT, "android", rel),
                                  os.path.join(ROOT, "android", "app", "src", "main", rel),
                                  os.path.join(ROOT, "src", rel),
                                  os.path.join(ROOT, "tools", rel)]
                if not any(os.path.exists(os.path.normpath(c)) for c in candidates):
                    problems.append("%s ссылается на несуществующий `%s`"
                                    % (os.path.relpath(doc_path, ROOT), token))

    # инструкция обязана содержать основные разделы и честную оговорку
    doc = read(DOCS[0]) if os.path.exists(DOCS[0]) else ""
    for needle, why in (
        ("build_android.bat", "без команды инструкция бесполезна"),
        ("Если не собралось", "разбор ошибок — главное, зачем её читают"),
        ("android_build.log", "человек должен знать, где смотреть причину"),
        ("Android Studio", "второй способ сборки"),
        ("adb install", "установка на телефон"),
    ):
        if needle not in doc:
            problems.append("ANDROID_BUILD.md: нет «%s» — %s" % (needle, why))

    return problems


def main(argv):
    strict = "--strict" in argv
    problems = check_bat() + check_project() + check_resources() + check_docs()
    version = re.search(r'#define GAME_VERSION\s+"([\d.]+)"',
                        read(VERSION_H)).group(1)
    print("Android-сборка %s: батник, проект, ресурсы, инструкция" % version)
    if problems:
        print("рассхождения:")
        for problem in problems:
            print("  ! " + problem)
        return 1 if strict else 0
    print("всё сходится: батник, gradle, CMake, манифест и инструкция не расходятся")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
