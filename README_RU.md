# TOTK Explorer v3.0.3 — stable build layout

Целевая игра: The Legend of Zelda: Tears of the Kingdom 1.4.3
Title ID: `0100F2C0115B6000`
Build ID: `277178B7DBA1B6D4`

## Важно

Эта версия использует структуру Makefile официального Tesla Template. В корневом make файл входит в `build/` ровно один раз; внутренний make уже собирает объекты и `.ovl`.

`points.csv` не передаётся Makefile как DATA, поэтому CSV не компилируется.

## CI

GitHub Actions получает libtesla и libdmntcht, запускает `make clean && make -j2`, проверяет наличие `.ovl` и создаёт SD artifact.

## Установка

Скопировать:

`sd:/switch/.overlays/TOTK-Explorer-v3.ovl`

и:

`sd:/switch/totk_explorer/points.csv`

в корень SD-карты.

## Ограничение функциональности

Auto Discovery использует read-only memory access и эвристическую фильтрацию кандидатов X/Y/Z. Это не гарантирует правильное определение координат без проверки на реальной сборке игры. Карта использует внешний CSV, поэтому в поставленном пакете есть только тестовые точки.
