# Сборка под Android — инструкция

Короткая версия: **двойной щелчок по `build_android.bat`** в корне репозитория.
Он сам найдёт Java и SDK, при необходимости скачает Gradle, соберёт APK и
положит его в `build\` рядом с версией для Windows.

Ниже — то же самое подробно, плюс ручные пути и разбор ошибок.

---

## 1. Что получится

| | |
|---|---|
| файл | `build\ShadowSurvivors_2.10.50_debug.apk` (или `..._release.apk`) |
| версия | берётся из `src/version.h` — та же, что у exe и у хроники в игре |
| пакет | `com.shadowsurvivors.game`, название на телефоне «Пепел Тьмы» |
| архитектура | `arm64-v8a` (все телефоны последних лет) |
| минимальный Android | 8.0 (API 26) — раньше нет AAudio, через который идёт звук |
| размер | около 3–5 МБ: вся игра — код и заголовочные файлы, картинок почти нет |

Игра — программа на C++, поэтому APK — это обёртка: внутри один `.so` и
манифест. Java-кода в проекте нет вообще, активность системная
(`android.app.NativeActivity`).

---

## 2. Что должно быть на компьютере

1. **JDK 17 или новее.** Android Gradle Plugin 8.x на старых JDK не работает.
   Проще всего поставить [Temurin 17](https://adoptium.net/) или ничего не
   ставить, если стоит Android Studio — у неё JDK лежит внутри
   (`C:\Program Files\Android\Android Studio\jbr`), и `build_android.bat`
   находит его сам.
2. **Android SDK** (platform-tools, platforms;android-34, build-tools).
   Ставится вместе с Android Studio или через
   [command line tools](https://developer.android.com/studio#command-tools).
3. **NDK 26.1.10909125 и CMake 3.22.1** — ими собирается `src/main_android.cpp`.
   Ставятся в Android Studio в `Settings → SDK Manager → SDK Tools`, либо
   из консоли:
   ```
   sdkmanager "ndk;26.1.10909125" "cmake;3.22.1" "platforms;android-34" "build-tools;34.0.0"
   sdkmanager --licenses
   ```
4. **Gradle 8.2+** — не обязательно: если его нет, `build_android.bat` скачает
   свою копию в `build\gradle-dist\` (только в первый раз, около 130 МБ).

Проверить, что всё видно из консоли:

```
java -version
echo %ANDROID_HOME%
sdkmanager --list_installed
```

---

## 3. Сборка батником (рекомендуется)

```
build_android.bat                 rem debug-APK
build_android.bat release         rem release-APK (отладочный ключ, ставится на телефон)
build_android.bat install         rem собрать и сразу поставить на телефон
build_android.bat clean release   rem сначала очистить, потом собрать release
```

Аргументы можно писать в любом порядке и в любом количестве.

**Что он делает по шагам:**

1. переключает кодовую страницу консоли на UTF-8, чтобы русский текст читался;
2. читает версию из `src/version.h` и считает `versionCode` (2.10.50 → 3050);
3. ищет Java: `JAVA_HOME`, потом JDK внутри Android Studio, потом `java` в PATH;
   если JDK старше 17 — предупреждает, но сборку не отменяет;
4. ищет SDK: `ANDROID_HOME`, `ANDROID_SDK_ROOT`, `%LOCALAPPDATA%\Android\Sdk`,
   и пишет `android\local.properties` (этот файл у каждого свой и в git не идёт);
5. ищет Gradle: `android\gradlew.bat` → `gradle.bat` в PATH → своя скачанная
   копия в `build\gradle-dist\`;
6. запускает `assembleDebug`/`assembleRelease` и пишет **весь** вывод Gradle в
   `build\android_build.log`;
7. копирует готовый APK в `build\ShadowSurvivors_<версия>_<тип>.apk`;
8. с аргументом `install` — ставит его через `adb install -r`;
9. в конце **всегда** пауза: окно не закрывается само, ошибку видно.

Если сборка упала, батник покажет последние 24 строки журнала и оставит их
целиком в `build\android_build.log`. Код возврата при ошибке — 1, при успехе — 0:
двойной щелчок всё равно ждёт клавишу, а из скрипта результат видно.

Проверить сам батник, проект и эту инструкцию, не собирая APK:

```
python3 tools/check_android_bat.py --strict
```

Он смотрит кодировку и переводы строк, метки и переходы, баланс скобок в
`if`-блоках, кавычки в путях, объявленность всех `!переменных!`; сверяет
`lib_name` в манифесте с именем CMake-цели, глубину `../` до `src/`, версию и
`versionCode` по умолчанию в `android/app/build.gradle` с `src/version.h`,
проверяет, что манифест и `strings.xml` — валидный XML, что все пять иконок
`mipmap-*` на месте и нужного размера, и что каждый файл, названный в
инструкции, существует.

Совсем без телефона Android-слой можно и **запустить**:

```
bash tools/check_android.sh
```

Он собирает и прогоняет два исполняющих стенда: `tools/touch_probe.cpp` (схема
касаний против настоящей игры) и `tools/android_smoke.cpp` (весь
`src/main_android.cpp` на поддельном NDK — поток игры, EGL, AAudio, ассеты,
пауза, сейв). Это не заменяет телефон, но ловит то, что не ловит компилятор.

---

## 4. Сборка в Android Studio

1. `File → Open` → каталог **`android/`** (не корень репозитория).
2. Студия предложит поставить NDK и CMake — согласиться (версии уже прописаны
   в `android/app/build.gradle`).
3. Подключить телефон по USB с включённой отладкой, нажать `Run ▶`.

Студия обновит `local.properties` сама. Версия в APK по умолчанию возьмётся из
`android/app/build.gradle` (`appVersion`), а не из `src/version.h` — если нужен
точный номер, запустите сборку батником или передайте параметр:
`./gradlew assembleDebug -PappVersion=2.10.50 -PappVersionCode=3050`.

---

## 5. Сборка руками в консоли

```bash
cd android
# один раз: путь к SDK (Studio делает это сама)
echo sdk.dir=C:/Users/you/AppData/Local/Android/Sdk > local.properties

gradle assembleDebug            # или gradle assembleRelease
# APK: app/build/outputs/apk/debug/app-debug.apk
```

Ставить:

```bash
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

`local.properties` — файл на один компьютер, в репозиторий не попадает. Путь
внутри пишется с прямыми слэшами.

---

## 6. Что внутри APK

```
lib/arm64-v8a/libshadow_survivors.so   вся игра: ядро, рендерер, звук
assets/audio/                          ваша музыка и озвучка (необязательно)
res/mipmap-*/ic_launcher.png           иконка из src/icon.ico
AndroidManifest.xml                    NativeActivity, lib_name = shadow_survivors
```

- **Графика**: игра рисует кадр в память, APK заливает его в текстуру
  (EGL + GLES2). GPU телефона только показывает картинку, вся растеризация —
  на процессоре. Поэтому ступени графики из 2.10.48 здесь работают так же.
- **Звук**: AAudio, тот же микшер, что на Windows (`SoundEngine::mixSamples`).
- **Музыка и озвучка**: положите mp3 в `android/app/src/main/assets/audio/`
  перед сборкой (имена файлов — как в Windows-версии, список в `README.txt`
  внутри этой папки). При первом запуске игра скопирует их во внутреннюю
  память. Без файлов игра не молчит: играет встроенная тема.

---

## 7. Установка на телефон

**Через кабель:**

```
build_android.bat install
```

или вручную: `adb install -r build\ShadowSurvivors_2.10.50_debug.apk`.

**Без кабеля:** скопировать APK на телефон (проводник, облако, Telegram) и
открыть его. Android спросит разрешение «Устанавливать из неизвестных
источников» — разрешить для того приложения, из которого открываете.

**Обновление:** ставится поверх, сейв и настройки остаются (они лежат во
внутренней памяти приложения и не входят в APK).

**Удаление:** обычное, через настройки телефона. Сейв удалится вместе с ним.

---

## 8. Если не собралось

| что видно | что делать |
|---|---|
| `не нашёл Java` / `Unsupported class file major version` | нужен JDK 17+. Поставьте Temurin 17 или Android Studio (её JDK батник находит сам). |
| `не нашёл Android SDK` / `SDK location not found` | задайте `ANDROID_HOME` на папку SDK или откройте проект в Android Studio один раз — она создаст `local.properties`. |
| `Failed to install the following SDK components` / `licenses` | `sdkmanager --licenses` и принять все лицензии. |
| `NDK not configured` / `No version of NDK matched` | `sdkmanager "ndk;26.1.10909125"` (или поменять `ndkVersion` в `android/app/build.gradle` на установленную). |
| `Could not resolve com.android.tools.build:gradle` | нет доступа к `google()` / `mavenCentral()`: проверьте интернет, прокси, корпоративный файрвол. |
| `не смог скачать Gradle` | поставить Gradle 8.2+ вручную и добавить в PATH, либо собрать в Android Studio. |
| `INSTALL_FAILED_UPDATE_INCOMPATIBLE` | на телефоне другая подпись: `adb uninstall com.shadowsurvivors.game` и поставить заново. |
| `INSTALL_FAILED_NO_MATCHING_ABIS` | телефон 32-битный: добавьте `armeabi-v7a` в `abiFilters` в `android/app/build.gradle`. |
| `INSTALL_FAILED_INSUFFICIENT_STORAGE` | освободить место (нужно ~30 МБ). |
| APK ставится, но чёрный экран или сразу закрывается | `adb logcat -s ShadowSurvivors` и `crash.log` во внутреннем каталоге: `adb shell run-as com.shadowsurvivors.game cat files/crash.log`. |

Журнал сборки — всегда `build\android_build.log`. Он не перезаписывается
частично: пишется заново на каждый запуск, поэтому после неудачи смотрите его
целиком, а не только хвост.

---

## 9. Первый запуск на телефоне

1. Открыть игру: сразу меню, тап = клик.
2. `НАСТРОЙКИ → Графика`: `АВТО` (или `БЫСТРОЕ`, если хочется максимум fps).
3. Начать забег и проверить управление: стик слева двигает, палец справа
   целится, `СУПЕР` и `РЫВОК` у правого большого пальца, `II` — пауза,
   `F3` — техническая панель.
4. Свернуть и вернуться: забег продолжается, сейв на месте.
5. Если fps низкий — `F3` покажет цену кадра. 16.7 мс — это 60 fps,
   33 мс — 30. Дальше — ступень графики.

Схема управления нарисована на листе `design/touch_controls.png`, разница
ступеней графики — на `design/mobile_levels.png`.

---

## 10. Чего здесь нет

- **Готового APK в репозитории**: он собирается на вашей машине. Сборка требует
  NDK, а бинарники в git не хранятся (как и `.exe`).
- **`gradlew`/`gradle-wrapper.jar`**: обёртка — это бинарник, поэтому батник
  берёт Gradle из PATH или скачивает свою копию. Если хотите классический
  `./gradlew`, сгенерируйте обёртку один раз: `gradle wrapper --gradle-version 8.2`
  в каталоге `android/`.
- **Подписи для Google Play**: release подписан отладочным ключом, чтобы APK
  сразу ставился на свой телефон. Для магазина нужен свой keystore —
  `signingConfigs` в `android/app/build.gradle`.
- **armeabi-v7a**: в проекте только `arm64-v8a`. Добавляется одной строкой.

Устройство Android-слоя, жизненный цикл, проверки без телефона — в
`design/android.md`. Что делать с графикой и как мерить телефон — в
`design/mobile_phone.md`.
