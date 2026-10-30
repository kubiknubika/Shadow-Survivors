# Shadow Survivors — «Пепел Тьмы»

Выживание против орды: автоматическое оружие, прокачка во время боя, четыре
печати кампании и забеги без конца. Игра целиком на C++ в заголовочных файлах,
без движка и без сторонних библиотек: одна папка `src/`, две платформы —
Windows (`.exe`) и Android (`.apk`).

---

## Что нужно, чтобы запустить

### Windows (основная версия)

| что | зачем |
|---|---|
| Windows 10/11, 64-бит | игра — программа win32 (`main_win32.cpp`) |
| [MSYS2](https://www.msys2.org/) или [winlibs](https://winlibs.com/) с **MinGW-w64 g++ 12+** | компилятор и `windres` (иконка и версия в exe) |
| больше ничего | графика, звук и шрифты — свои, лежат в `src/` |

Установка компилятора в MSYS2 (один раз):

```
pacman -S mingw-w64-x86_64-gcc
```

и добавьте `C:\msys64\mingw64\bin` в `PATH`.

### Android

| что | зачем |
|---|---|
| JDK 17+ | Android Gradle Plugin 8.x |
| Android SDK (platform-tools, platforms;android-34, build-tools 34) | сборка APK |
| NDK 26.1.10909125 и CMake 3.22.1 | ими собирается `src/main_android.cpp` |

Подробно, с командами `sdkmanager` и разбором ошибок — [`ANDROID_BUILD.md`](ANDROID_BUILD.md).

### Linux / macOS

Собрать саму игру нельзя (win32 API), но **все стенды и проверки работают** —
именно на них проверялась эта версия. Нужен `g++` с C++17 и `python3`.

---

## Собрать и запустить

### Windows

```
build_windows.bat
```

Двойной щелчок делает всё: берёт версию из `src/version.h`, собирает иконку
через `windres`, компилирует и кладёт готовый файл в `build\`:

```
build\ShadowSurvivors.exe
```

Журнал сборки — `build\build.log`. Если что-то не так, батник покажет хвост
журнала и не закроется.

Запуск с ключами (необязательно):

```
build\ShadowSurvivors.exe --graphics 3 --threads 4
```

- `--graphics 0..4` — АВТО / ВЫСОКОЕ / СРЕДНЕЕ / БЫСТРОЕ / ЭКОНОМ (то же есть в
  настройках, клавиша `0`);
- `--threads 1..8` — сколько потоков рисуют кадр полосами (`0` — по числу ядер).

### Android

```
build_android.bat            :: debug-APK  -> build\ShadowSurvivors_<версия>_debug.apk
build_android.bat install    :: собрать и сразу поставить на телефон
```

Батник сам находит Java, SDK и Gradle. Что делать, если не собралось —
[`ANDROID_BUILD.md`](ANDROID_BUILD.md), раздел «Если не собралось».

---

## Управление

**Клавиатура и мышь (Windows):**

| | |
|---|---|
| `WASD` / стрелки | движение |
| мышь | прицел, левая кнопка — огонь/выбор |
| `Пробел` | рывок |
| `Q` | суперумение |
| `Esc` | пауза, выход из подменю |
| `F3` | техническая панель: ступень графики, цена кадра, потоки |
| `F9` (или `` ` ``) | отладка: герой, ресурсы, телепорт, спавн |

**Телефон:** стик — левая нижняя часть экрана (динамический, центр там, где
коснулись); палец справа — прицел и огонь; кнопки `СУПЕР` и `РЫВОК` у правого
большого пальца; `II` — пауза, `F3` — панель. Схема нарисована на листе
[`design/touch_controls.png`](design/touch_controls.png).

---

## Где лежат файлы игры

**Windows** — рядом с `ShadowSurvivors.exe`:

```
shadow_survivors.sav     прогресс, рекорды, открытые герои
shadow_survivors.cfg     настройки (громкость, графика, переключатели)
audio/                   своя музыка и озвучка (mp3); нет файлов — играет встроенная тема
```

**Android** — во внутреннем каталоге приложения (там же, где `audio/` и
`crash.log`; путь виден в `adb shell run-as com.shadowsurvivors.game ls files`).
Сейв переживает обновление APK, но удаляется вместе с приложением.

---

## Обновление до свежей версии

```
update.bat
```

Тянет ветку с GitHub, пересобирает и сохраняет ваши сейв, настройки и музыку.
Подробности, ручные команды и что делать при конфликте —
[`UPDATE_HOWTO.md`](UPDATE_HOWTO.md).

---

## Проверки и стенды

Всё, что можно проверить без телефона и без второго компьютера. Стенды
собираются обычным `g++` и на Windows, и на Linux.

```
g++ -O2 -std=c++17 -pthread -Isrc tools/screen_check.cpp -o build/screens
build\screens 2 1
```

| стенд | что отвечает |
|---|---|
| `tools/screen_check.cpp` | хеши десяти экранов: кадр изменился или нет (и одинаков ли он в 1, 2, 4 потока) |
| `tools/screen_shot.cpp` | один экран в BMP — когда хеша мало и надо посмотреть глазами |
| `tools/perf_bench.cpp` | цена кадра в мс; код 1, если не влезает в 16.7 мс |
| `tools/touch_probe.cpp` | 51 проверка схемы касаний против настоящей игры: стик двигает героя, пауза встаёт, тап работает как клик, три минуты забега без падений |
| `tools/android_smoke.cpp` | исполняет **весь Android-слой** на поддельном NDK: поток игры, EGL, AAudio, ассеты, пауза/возврат, сейв при выходе |
| `tools/touch_preview.cpp` | рисует схему управления на кадре боя (лист `design/touch_controls.png`) |
| `python3 tools/check_version.py --strict` | номер версии не разъехался между `version.h`, хроникой и CHANGELOG |
| `python3 tools/check_android_bat.py --strict` | батник, gradle, CMake, манифест, иконки и то, что инструкция не врёт про файлы |

Одной командой Android-часть (включая оба исполняющих стенда):

```
bash tools/check_android.sh
```

---

## Структура репозитория

```
src/            вся игра: game.h (правила), renderer.h (растеризация), audio.h (микшер),
                main_win32.cpp (Windows), main_android.cpp + touch_input.h (Android)
android/        проект Gradle: манифест, CMake, иконки, ассеты
tools/          стенды, проверки, генераторы листов
design/         отчёты и листы-картинки по каждой задаче
build_windows.bat / build_android.bat / update.bat
```

Документация по частям игры — в `design/`: [`design/android.md`](design/android.md),
[`design/mobile_phone.md`](design/mobile_phone.md),
[`design/perf_parallel.md`](design/perf_parallel.md),
[`design/perf_refactor.md`](design/perf_refactor.md),
[`design/builds_catalog.md`](design/builds_catalog.md),
[`design/campaign_map_snake.md`](design/campaign_map_snake.md),
[`design/android_check.md`](design/android_check.md).

---

## Честно о том, что проверено

- **Windows-версия** собирается `build_windows.bat`; `.exe` в репозитории не
  лежит — он собирается у вас (бинарники в git не хранятся).
- **Android-версия** собирается `build_android.bat`. APK в репозитории тоже нет:
  нужен NDK. Сам батник проверен статически, а Android-слой — исполняющим
  стендом на поддельном NDK (см. `design/android_check.md`), но **на живом
  телефоне он ещё не запускался**: первый запуск — ваш.
- Все стенды и проверки выше проходят на этой версии; что именно прогнано —
  в [`design/android_check.md`](design/android_check.md).
