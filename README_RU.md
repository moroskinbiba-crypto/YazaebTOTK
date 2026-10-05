# TOTK Explorer v3.3.1 — build pipeline

Tesla overlay for The Legend of Zelda: Tears of the Kingdom.

Target build:
- Version: 1.4.3
- Title ID: 0100F2C0115B6000
- Build ID: 277178B7DBA1B6D4

## Что исправлено

- CI использует devkitPro/devkitA64 и проверяет наличие `switch_rules`, C++-компилятора и `elf2nro` до сборки.
- libtesla берётся из зафиксированного коммита Status-Monitor-Deux, в котором есть совместимые с текущим проектом имена кнопок и сигнатура `handleInput`.
- `dmnt:cht` берётся из зафиксированного коммита Shiny-Stash-Live-Map вместе с `libdmntcht.a` и `dmntcht.h`.
- `source/ui.cpp` явно подключает `<cmath>` для `sqrt/lround`.
- Для точного target build добавлен resolver актёра `Player`: он использует layout TOTK 1.4.3 из публичного профиля и читает позицию по полю `ActorPosition`.
- Перед использованием exact resolver проверяется Title ID и короткий 16-символьный BID `277178B7DBA1B6D4` через `main_nso_build_id`.
- Артефакт CI принудительно включает скрытый каталог `.overlays`.
- CI проверяет последние 4 байта `.ovl` на сигнатуру `ULTR`, необходимую для распознавания Ultrahand.
- CI проверяет формат и слой каждой строки `points.csv`.

## Текущее состояние

Основной путь координат — resolver актёра `Player` для build `1.4.3`: он находит resident actor с именем `Player` и читает `X/Y/Z` из actor layout. На каждом обновлении читается сохранённый actor address; периодически resolver повторно валидирует actor и автоматически восстанавливается после смены процесса игры.

Эвристический сканер сохранён как fallback. Он ищет тройки `float` в памяти процесса и использует движение игрока/изменение высоты для отбора кандидата.

В `data/points.csv` сейчас находится 152 святилища. Формат: `Type,Name,X,Y,Z,Layer`. Источник с колонками `X,Y,Height` нормализован в игровой порядок `X,Y,Z`, где игровая `Y` — высота.

Список Nearby автоматически фильтрует точки по текущему слою (Surface/Sky/Depths), а Diagnostics показывает адрес Player actor и количество отброшенных строк CSV.

Диагностика дополнительно записывается в:
`sd:/switch/totk_explorer/log.txt`.

Физическая проверка на Switch всё равно остаётся обязательной: CI подтверждает сборку, упаковку и формат данных, но не заменяет тестирование на консоли.

## Сборка

В GitHub Actions запускается workflow `TOTK Explorer Build`.

Локально:

```sh
bash tools/setup_deps.sh
make clean
make -j2
```

Результат: `TOTK-Explorer-v3.ovl`.

## Установка

Из артефакта CI содержимое `switch` копируется в корень microSD:

```text
sd:/switch/.overlays/TOTK-Explorer-v3.ovl
sd:/switch/totk_explorer/points.csv
```
