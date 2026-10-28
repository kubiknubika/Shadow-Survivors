# Промпты для генерации музыки ShadowSurvivors (AIVA / Audio generation)

Два слоя. **Сначала** — общие промпты на каждую сцену: их хватает, чтобы игра
зазвучала. **Ниже** — промпты под каждое дело кампании, под каждого стража и под
эндлес: там история задаёт тембр, но темп и пульс всегда боевые, чтобы трек
подходил бою, а не превращался в заставку.

## Для AIVA → Create → Audio generation
1. Вставьте промпт в поле описания
2. Отключите "Lyrics" (нужен чистый инструментал)
3. 1 track
4. Нажмите "Create your song"

---

### Главное меню
```
Dark atmospheric ambient, low drones, subtle synth pads, mysterious and foreboding, slow tempo, minor key, haunting melody, cinematic, game menu background music, no vocals, instrumental, 60-80 BPM
```

### Бой / геймплей
```
Fast-paced dark electronic action, driving percussion, heavy bass, aggressive synth leads, intense and urgent, minor key, 128-140 BPM, combat music, survival roguelike, no vocals, instrumental
```

### Босс-файт
```
Epic orchestral dark hybrid, booming taiko drums, deep brass, choir whispers, threatening and majestic, slow building intensity, minor key, 70-90 BPM, boss battle music, cinematic, no vocals
```

### Победа над боссом
```
Triumphant orchestral, bright strings, rising melody, hopeful and victorious, major key, 90-110 BPM, victory fanfare, cinematic, no vocals, instrumental
```

### Game Over (переделано — не грустный, а с надеждой на реванш)
```
Determined orchestral, rising strings, steady percussion, hopeful and resilient, minor key building to major, 80-100 BPM, game over screen music, cinematic, no vocals, instrumental, ready to fight again
```

### Апгрейд / получение улучшения (переделано — без запрещённых слов)
```
Bright magical chime, ascending melody, sparkling and rewarding, major key, fast and positive, UI sound effect, level up sound, instrumental, no vocals, satisfying and clean
```

### Кузница / Магазин (Soul Forge)
```
Medieval fantasy tavern ambience, lute and harp, gentle percussion, cozy and inviting, minor key, 80-90 BPM, shop music, instrumental, no vocals
```

---

## Как называть файлы

См. файл `AUDIO_NAMING.md`.

---

## По делам: какой файл играет какое дело

Кампания выбирает трек **по номеру дела**: дело 01 играет первый файл плейлиста
сцены `gameplay`, дело 02 — второй, и так далее. Файлы сортируются по алфавиту,
поэтому имена надо давать с ведущим нулём.

| Что делать | Имя файла | Какое дело играет |
|-----------|-----------|-------------------|
| Все 28 треков | `ss_gameplay_case01.mp3` … `ss_gameplay_case28.mp3` | `case01` → дело 01, `case28` → дело 28 |
| Меньше 28 треков | те же имена с любым шагом | дела идут по кругу: трек `k` играет дело `k`, `k+n`, `k+2n`… |
| Один трек на всё | `ss_gameplay_run.mp3` | все дела и эндлес |

Эндлес берёт треки сцены `gameplay` в случайном порядке и переключается на
следующий, когда предыдущий доиграл, — поэтому эндлес-треки делаются из того же
пула (в конце файла — таблица «эндлес-набор»).

Стражи звучат отдельно: сцена `boss`, файл `ss_boss_guardianNN.mp3`; выбор идёт
по типу стража (0…5), `br_NN` — их усиленные «возвращения».

---

## Общая подложка: один мир, один бой

Каждый промпт ниже начинается с настроения дела, но **все** промпты заканчиваются
одним и тем же хвостом — он и делает треки совместимыми между собой и с боем.
Дописывать его к каждому промпту:

> **BATTLE BEDROCK (дописывать всегда):** steady percussive pulse on every beat,
> 4/4, clear low end, sidechained kick, no long intro under 8 seconds, no vocals,
> no spoken word, no sudden silence, energy stays between 70 and 100 % for the whole
> track, seamless loop point at the end, loopable, mastered loud but not clipped,
> -14 LUFS, 48 kHz stereo, length 1:45–2:30.

Технические правила поверх этого:

- **Темп** — в коридоре 96–152 BPM (указан под каждым делом). Медленнее — бой
  начинает «плыть», быстрее — трек устаёт за две минуты.
- **Структура** — четыре восьмитактовых блока: ввод (8 с), основа, слом, возврат
  в основу. Игра не знает про вашу форму, поэтому основа должна звучать и на
  первом, и на третьем повторе.
- **Никакого вокала и никаких слов** — голос в игре один (`voice-02`), любая
  вокальная партия с ним конкурирует. Хор допустим только без слов («wordless
  choir, vowels only»).
- **Мелодия не в самой верхней октаве** — там живут сигналы интерфейса и критические
  звуки боя.
- **Пики не в 0 dBFS** — трек должен оставлять место выстрелам и выкрикам.

---

## Дела 01–28

Ниже: файл, темп, настроение из истории и промпт. Промпт — на английском, потому
что генераторы музыки на английском точнее; к каждому добавлен русский смысл,
чтобы было видно, *почему* именно так.

### 01. ПЕПЕЛЬНЫЙ ПРОТОКОЛ — «УЧАСТОК У ВОРОТ»
`ss_gameplay_case01.mp3` · 108 BPM · первый бой, ещё не страшно, но уже неправильно.

> Dark fantasy survival action, 108 BPM, low male-free choir pad, dry taiko and
> felt piano over a marching pulse, sparse brass stabs, ash-grey tone, cold air
> between hits, patient and heavy, gets louder every 8 bars. No vocals, no intro
> longer than 8 seconds, loopable.

*Почему:* город только что замолчал; трек «накрывает место», как пепел, но идти
вперёд уже надо.

### 02. СВИДЕТЕЛЬ БЕЗ ИМЕНИ — «ПРОТОКОЛ ДОПРОСА»
`ss_gameplay_case02.mp3` · 112 BPM · ритм как допрос: вопрос — пауза — вопрос.

> Noir industrial battle theme, 112 BPM, ticking clock percussion, typewriter clicks
> used as hi-hats, muted cello ostinato, one repeated two-note question motif,
> cold bureaucracy meets hunting, tight and dry. No vocals, loopable.

*Почему:* дело ведёт допрос, который никуда не записывается, — ритм щёлкает, как
печатная машинка.

### 03. ТРИ КОЛОКОЛА — «РЕЗОНАТОРЫ КОПИРУЮТ ПРИКАЗ»
`ss_gameplay_case03.mp3` · 120 BPM · три удара колокола = три печати.

> Ritual battle music, 120 BPM, three distant church bells as the main hook, pounding
> tom pattern underneath, dissonant string cluster that tightens each section,
> rising urgency, bronze and iron timbres. No vocals, loopable.

*Почему:* каждое дело повторяет один и тот же приказ — трек трижды возвращается
к одному мотиву и каждый раз злее.

### 04. ВОРОТА ДОЛГА — «ПЕЧАТЬ I • ДОЛГ» · **страж МОРГАР**
`ss_gameplay_case04.mp3` · 128 BPM · баррикада изнутри; медленно, но неостановимо.

> Siege battle theme, 128 BPM, anvil hits and chain rattles, low brass in fifths,
> relentless half-time drums, gate-crashing accents, desperate defence energy.
> No vocals, loopable.

*Почему:* Моргар держит ворота, а не нападает, — поэтому темп ровный, как у
человека, который не собирается сдвинуться.

### 05. ПРОТОКОЛ ЭВАКУАЦИИ — «ВЕТКА А • СПИСКИ УШЕДШИХ»
`ss_gameplay_case05.mp3` · 116 BPM · марш, из которого вычли людей.

> Marching industrial battle theme, 116 BPM, boots-on-gravel percussion, snare rolls
> that thin out instead of building, mourning horn buried in the mix, forward motion
> with an empty centre. No vocals, loopable.

*Почему:* восемьдесят фамилий, восемьдесят первая — твоя; идёт движение, а строя нет.

### 06. ПУСТЫЕ НАРУЧНИКИ — «ВЕТКА Б • КАМЕРА ХРАНЕНИЯ»
`ss_gameplay_case06.mp3` · 124 BPM · металл по металлу, склад, из которого вынесли всех.

> Metallic percussive battle theme, 124 BPM, handcuff chains and locker doors as
> instruments, syncopated bass, cold reverb tails, claustrophobic storage room
> atmosphere with a hard driving beat. No vocals, loopable.

*Почему:* ветка Б — другая комната той же ночи: тот же пульс, но тембр железа.

### 07. КРИПТА ГОЛОСОВ — «КОЛБЫ С ЧУЖИМИ СЛОВАМИ»
`ss_gameplay_case07.mp3` · 110 BPM · голоса в стекле, но слов нет.

> Dark ambient battle hybrid, 110 BPM, glass harmonics and breathy air pads,
> wordless vowel choir far in the background, low pulse like a slow heartbeat
> through glass, creeping dread that still moves forward. No words, no vocals in
> front, loopable.

*Почему:* в колбах последние слова; трек дышит, но ничего не произносит.

### 08. СЕСТРА, КОТОРАЯ НЕ СЛЫШИТ — «ДОПРОС КОПИИ»
`ss_gameplay_case08.mp3` · 118 BPM · один и тот же ответ восемь раз.

> Looping interrogation battle theme, 118 BPM, a single four-note phrase repeated
> with microscopic variations, mechanical pulse, bowed metal drones, obsessive and
> unblinking. No vocals, loopable.

*Почему:* восемь раз один голос — трек обязан быть зацикленным по смыслу, не
только технически.

### 09. ХОЛОДНЫЙ СЛЕД — «ВЕТКА А • ЛАБОРАТОРИЯ»
`ss_gameplay_case09.mp3` · 132 BPM · озон, стекло, хирургическая точность.

> Cold electronic battle theme, 132 BPM, glassy arpeggios, ozone-static texture,
> precise metallic drums, sterile reverb, clinical tension with a fast pulse.
> No vocals, loopable.

*Почему:* лаборатория, где Архив учился говорить чужими голосами.

### 10. ТЁПЛЫЙ СЛЕД — «ВЕТКА Б • МОРГ»
`ss_gameplay_case10.mp3` · 122 BPM · тепло в морге: тот же трек, но живой.

> Grimy analog battle theme, 122 BPM, warm tape saturation, low tom groove, buzzing
> fluorescent hum turned into a pad, uneasy flesh-and-formaldehyde mood with a solid
> beat. No vocals, loopable.

*Почему:* ветка Б — морг цел, и «тепло» здесь неправильное; тот же пульс, другая
кровь.

### 11. КОЛБА № 9 — «ПОСЛЕДНЯЯ ЗАПИСЬ СЕСТРЫ»
`ss_gameplay_case11.mp3` · 126 BPM · пустая колба с твоим голосом на бирке.

> Tense hybrid orchestral battle cue, 126 BPM, solo cello line that almost forms a
> melody, pulse beneath, reversed piano swells, identity crisis energy: calm surface,
> violent undertow. No vocals, loopable.

*Почему:* кто-то был здесь с твоим голосом — мелодия почти твоя, но не твоя.

### 12. ПЕЧАТЬ ГОЛОСА — «ПЕЧАТЬ II • ГОЛОС» · **страж МАЛАКОР**
`ss_gameplay_case12.mp3` · 134 BPM · он бережёт голос, потому что хоронить нечем.

> Tragic boss-pace battle theme, 134 BPM, wordless male choir on vowels, heavy low
> strings, war drums with long decay, grief turned into force, wide stereo. No words,
> loopable.

*Почему:* Малакор не злодей, он хранитель; трек должен быть и жалко, и страшно.

### 13. ЗАТОПЛЕННЫЙ РЕЕСТР — «КНИГИ ПОД ВОДОЙ»
`ss_gameplay_case13.mp3` · 104 BPM · медленно, потому что вода.

> Underwater battle theme, 104 BPM, muffled drums, sub-bass swells, drip percussion,
> drowned bells, heavy pressure, slow but unavoidable advance. No vocals, loopable.

*Почему:* нижний ярус реестра затоплен, ноги вяжут — но бой не ждёт.

### 14. ИМЯ НА КАМНЕ — «СЕМЕЙНЫЕ ЛЕНТЫ»
`ss_gameplay_case14.mp3` · 114 BPM · резец по камню.

> Stone-and-grief battle theme, 114 BPM, chisel-on-granite percussion, low viola
> drone, small bell motif, solemn but moving, dust in the air. No vocals, loopable.

*Почему:* имена режут заново поверх прежних; ритм — это удары резца.

### 15. ПИСАРЬ, КОТОРЫЙ СТЁР СЕБЯ — «ПУСТАЯ СТРОКА В КНИГЕ»
`ss_gameplay_case15.mp3` · 120 BPM · строка вымарана дважды.

> Erased-identity battle theme, 120 BPM, ink-scratch percussion, hollow mid-range,
> a motif that drops out one instrument at a time and comes back fuller, unsettling
> groove. No vocals, loopable.

*Почему:* тот, кто стирал имена, стёр и своё, — музыка «вычёркивает» саму себя.

### 16. ДЕЛО О СЕМИ СЕМЬЯХ — «ВЕТКА А • СВИДЕТЕЛИ ЖИВЫ»
`ss_gameplay_case16.mp3` · 130 BPM · семь голосов, одно показание.

> Choral battle theme, 130 BPM, seven-note vocal stack (vowels only, no words),
> driving percussion, folk fiddle line, crowd energy without a crowd melody, warm
> and determined. No words, loopable.

*Почему:* свидетели живы и говорят хором — отсюда хор, но без слов.

### 17. ДЕЛО О ПУСТЫХ СТРАНИЦАХ — «ВЕТКА Б • ВЫРВАННЫЕ ЛИСТЫ»
`ss_gameplay_case17.mp3` · 126 BPM · бумага, из которой вырезали людей.

> Paper-and-void battle theme, 126 BPM, tearing paper percussion, high glass tones,
> abrupt gaps that land on the beat, obsessive mid-tempo drive. No vocals, loopable.

*Почему:* пустые листы сложены в папку с твоим делом — трек рвётся, но держит ритм.

### 18. ПЕЧАТЬ ИМЕНИ — «ПЕЧАТЬ III • ИМЯ» · **страж КАРДИНАЛ**
`ss_gameplay_case18.mp3` · 138 BPM · забвение под видом милосердия.

> Ominous cathedral battle theme, 138 BPM, pipe-organ low cluster, choir on vowels,
> relentless double-kick pulse, sermon-like escalation, judgement in the air.
> No words, loopable.

*Почему:* Кардинал называл забвение милосердием — орган и приговор, но с боевым ходом.

### 19. ПЕПЕЛЬНЫЙ ТРОН — «КАБИНЕТ АРХИВАРИУСА» · **страж АЗАРОТ**
`ss_gameplay_case19.mp3` · 140 BPM · человек, запустивший машину, чтобы никто не исчез.

> Epic dark battle theme, 140 BPM, hybrid orchestra and industrial percussion,
> burning-ember texture, ash storm risers, tragic grandeur, momentum with weight.
> No vocals, loopable.

*Почему:* Азарот — трагедия, а не злодей; трек эпичный, но с горечью.

### 20. КОПИЯ СВИДЕТЕЛЯ — «ДВОЙНИК С МОИМ ГОЛОСОМ» · **страж ДВОЙНИК**
`ss_gameplay_case20.mp3` · 136 BPM · два тела, один голос.

> Mirror-image battle theme, 136 BPM, every element doubled with a 30 ms delay,
> glass percussion, two interlocking basslines, one slightly behind the other,
> uncanny groove. No vocals, loopable.

*Почему:* всё в треке продублировано с крошечной задержкой — как двойник.

### 21. ДОПРОС МАШИНЫ — «ПРОТОКОЛ БЕЗ СЛЕДОВАТЕЛЯ»
`ss_gameplay_case21.mp3` · 128 BPM · машина повторяет твой же рапорт.

> Machine-logic battle theme, 128 BPM, server-hum drone, data-pulse percussion,
> cold sequenced bass, echo of your own motif played backwards, relentless and
> precise. No vocals, loopable.

*Почему:* ответ — твой же рапорт; трек повторяет мотив, но задом наперёд.

### 22. ПЕЧАТЬ СВИДЕТЕЛЬСТВА — «ПЕЧАТЬ IV • СВИДЕТЕЛЬСТВО» · **страж ПРОТОКОЛИСТ**
`ss_gameplay_case22.mp3` · 142 BPM · последний засов Архива.

> Climactic battle theme, 142 BPM, huge stone percussion, ink-black brass, scribbling
> texture as a rhythmic layer, cathedral reverb, everything tightening toward the
> breaker. No vocals, loopable.

*Почему:* последняя печать; скрип пера работает как ритм-гитара.

### 23. СОХРАНИТЬ КОПИЮ — «ВЫБОР • ПАМЯТЬ ВМЕСТО ЧЕЛОВЕКА»
`ss_gameplay_case23.mp3` · 118 BPM · выбор сохранить — значит взять на себя.

> Ambiguous heroic battle theme, 118 BPM, warm strings and cold machine ticks in one
> bed, slow rising brass, doubt under a strong pulse, determined but not triumphant.
> No vocals, loopable.

*Почему:* бой один, а мотив двойной: и защита, и сделка.

### 24. СЖЕЧЬ КОПИЮ — «ВЫБОР • ПРАВО НА УТРАТУ»
`ss_gameplay_case24.mp3` · 118 BPM · тот же бой, но с огнём в конце.

> Firelit battle theme, 118 BPM, crackling flame percussion, low strings with
> distortion, a line that resolves downward instead of up, grief under pressure,
> same tempo family as its twin choice. No vocals, loopable.

*Почему:* зеркало дела 23 — тот же темп, но мелодия уходит вниз, а не вверх.

### 25. РЕЗЕРВНАЯ КОПИЯ ПРИКАЗА — «АРХИВ ЗАЩИЩАЕТ СЕБЯ»
`ss_gameplay_case25.mp3` · 146 BPM · машинная копия приказа запускает цикл.

> Relentless machine-battle theme, 146 BPM, duplicated-order motif in a hard loop,
> mechanical drums, alarm-like fifths, no human warmth, pure momentum.
> No vocals, loopable.

*Почему:* это не человек, а команда в цикле — отсюда жёсткая петля без развития.

### 26. ТРИ ЭХО-ПРИКАЗА — «СТРАЖИ СЕРДЦА»
`ss_gameplay_case26.mp3` · 144 BPM · три стража, которых никто не отменил.

> Triple-layer battle theme, 144 BPM, three interlocking rhythmic voices that swap
> dominance every 8 bars, hybrid orchestra, escalating pressure, feels like fighting
> three things at once. No vocals, loopable.

*Почему:* бой с тремя эхо — трек держит три слоя и не даёт одному победить.

### 27. ПОСЛЕДНЯЯ НОЧЬ — «КОЛЬЦО РАЗМЫКАЕТСЯ» · **страж ПОСЛЕДНИЙ ПРИКАЗ**
`ss_gameplay_case27.mp3` · 148 BPM · контур разорван, ночь ещё держится.

> Final-stand battle theme, 148 BPM, everything at once: war drums, full low brass,
> wordless choir, ash storm texture, a melody that keeps climbing and never resolves
> until the very last bar, desperate endurance. No words, loopable.

*Почему:* бой не за победу, а за удержание; мелодия обязана не разрешаться.

### 28. ПРАВО ПОМНИТЬ — «ДЕЛО ЗАКРЫТО»
`ss_gameplay_case28.mp3` · 112 BPM · последний бой после последнего боя.

> Bittersweet battle theme, 112 BPM, solo piano over a soft march pulse, muted
> strings, a melody that finally lands on the tonic, air after a long night, still
> moving. No vocals, loopable.

*Почему:* дело закрыто, а не выиграно — единственный трек, который разрешается.

---

## Стражи — отдельная сцена `boss`

`ss_boss_guardian00.mp3` … `ss_boss_guardian05.mp3`, плюс `ss_boss_br_NN.mp3` для
усиленных «возвращений». Все — 140–150 BPM, длиннее обычных (2:00–2:40), с
повтором основы, потому что бой со стражем идёт до трёх минут.

| Страж | Файл | Промпт |
|-------|------|--------|
| 0 МОРГАР • ВОРОТА | `ss_boss_guardian00.mp3` | Boss siege theme, 142 BPM, anvil-and-chain percussion, low brass fifths, gate-slam hits, unyielding half-time feel, dark and massive. No vocals, loopable. |
| 1 МАЛАКОР • ГОЛОС | `ss_boss_guardian01.mp3` | Boss tragedy theme, 144 BPM, wordless male choir, solo violin, war drums with long decay, grief and force together. No words, loopable. |
| 2 КАРДИНАЛ • РЕЕСТР | `ss_boss_guardian02.mp3` | Boss judgement theme, 146 BPM, pipe organ cluster, choir on vowels, double-kick pulse, sermon escalation. No words, loopable. |
| 3 АЗАРОТ • АРХИВАРИУС | `ss_boss_guardian03.mp3` | Boss epic theme, 145 BPM, ember-storm risers, hybrid orchestra, heavy taiko, tragic grandeur. No vocals, loopable. |
| 4 ПРОТОКОЛИСТ • СТРОКИ | `ss_boss_guardian04.mp3` | Boss scribe theme, 148 BPM, stone percussion, ink-brass, rhythmic pen-scratch layer, cathedral reverb, tightening. No vocals, loopable. |
| 5 ДВОЙНИК • СЛЕПОК | `ss_boss_guardian05.mp3` | Boss mirror theme, 147 BPM, everything doubled with a 30 ms delay, glass percussion, one bassline slightly behind the other, uncanny. No vocals, loopable. |
| Возвращение | `ss_boss_br_00.mp3` … | Тот же промпт стража, но: **+6 BPM, добавлены индустриальные удары и второй слой баса, снят верхний блеск** — «тот же силуэт, но злее». |

---

## Эндлес-набор

Эндлес не привязан к делу — он тасует треки сцены `gameplay`. Если для эндлеса
хочется своего пула, кладите отдельные файлы; они попадут в тот же плейлист.

Набор из шести треков (промпт — с тем же BATTLE BEDROCK):

1. `ss_gameplay_endless_a.mp3` — 124 BPM, ash-storm drums, dark synth ostinato,
   climbing energy, endless-night motion.
2. `ss_gameplay_endless_b.mp3` — 134 BPM, industrial toms, distorted bass, alarm
   fifths, relentless chase.
3. `ss_gameplay_endless_c.mp3` — 118 BPM, ritual bells, low choir vowels, patient
   heavy groove.
4. `ss_gameplay_endless_d.mp3` — 142 BPM, machine pulse, arpeggiated cold synth,
   panic tempo, no breathing room.
5. `ss_gameplay_endless_e.mp3` — 128 BPM, tape-warm analog, funky syncopation,
   ash-noir groove with swagger.
6. `ss_gameplay_endless_f.mp3` — 136 BPM, orchestral chase, string ostinato, war
   drums, heroic but doomed.

Все шесть должны заканчиваться в той же тональности-семье (минор с малой секундой),
чтобы переход по концу трека не «спотыкался».

---

## Служебные экраны (короткие)

| Экран | Файл | Промпт |
|-------|------|--------|
| Меню | `ss_menu_main.mp3` | Dark ambient title theme, 84 BPM, ash wind, distant bell, felt piano motif, no drums until 0:40, quiet and lonely, loopable. |
| Кузница душ | `ss_forge_shop.mp3` | Forge workshop loop, 96 BPM, hammer-on-anvil rhythm, warm fire crackle, low drone, comfortable industry, loopable. |
| Победа над стражем | `ss_victory_boss.mp3` | Short victory cue, 20–35 s, brass swell resolving to a low, tired unison, no fanfare brightness, ends in air. |
| Смерть | `ss_gameover_death.mp3` | Death cue, 25–40 s, the battle pulse slowing to a stop, single string line, no drums at the end, ash falling. |

---

## Чек-лист перед тем, как класть файл в `audio/`

- [ ] Темп в коридоре, указанном для дела (96–152 BPM).
- [ ] Вокала нет; если хор — только гласные, без слов.
- [ ] Первые 8 секунд уже боевые: игра не ждёт вступления.
- [ ] Трек держит 1:45–2:30 и не «устаёт» на четвёртом повторе.
- [ ] Конец сходится с началом — петля без щелчка.
- [ ] Громкость ровная с соседним треком (проверять на слух подряд, как в игре).
- [ ] Верх не забит: там живут сигналы боя и интерфейса.
- [ ] Пики не в 0 dBFS.
- [ ] Имя файла — по схеме `ss_<сцена>_<название>.mp3`, с ведущим нулём у номеров.
- [ ] В `docs`/`AUDIO_NAMING.md` ничего править не нужно: список читается из папки.
