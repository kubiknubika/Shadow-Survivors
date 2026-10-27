# Именование аудио-файлов для ShadowSurvivors

Все треки в формате **MP3**. Класть в папку `audio/` рядом с `.exe`.

## Схема именования

`ss_<сцена>_<название>.mp3`

## Поддерживаемые сцены

| Сцена | Пример файла | Для чего |
|-------|-------------|----------|
| `menu` | `ss_menu_main.mp3` | Главное меню |
| `gameplay` | `ss_gameplay_run.mp3` | Основной бой / забег |
| `boss` | `ss_boss_theme.mp3` | Битва с боссом |
| `victory` | `ss_victory_boss.mp3` | Победа над боссом |
| `gameover` | `ss_gameover_death.mp3` | Экран смерти |
| `forge` | `ss_forge_shop.mp3` | Кузница душ (магазин улучшений) |
| `upgrade` | `ss_upgrade_pick.mp3` | Звук получения улучшения (UI) |
| `levelup` | `ss_levelup_ding.mp3` | Звук повышения уровня |

## Пример структуры

```
ShadowSurvivors_2.9.70.exe
audio/
  ss_menu_main.mp3
  ss_gameplay_run.mp3
  ss_boss_theme.mp3
  ss_victory_boss.mp3
  ss_gameover_death.mp3
  ss_forge_shop.mp3
  ss_upgrade_pick.mp3
  ss_levelup_ding.mp3
```

Если нужно несколько вариантов одной сцены:
```
ss_menu_main_a.mp3
ss_menu_main_b.mp3
ss_menu_main_c.mp3
```
Игра загрузит все подходящие файлы и будет выбирать случайный.

Если файл не найден — игра не вылетает, просто не играет музыку.
