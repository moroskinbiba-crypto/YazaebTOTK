# TOTK Explorer v3.4.1 — multi-version Player resolver

Tesla overlay for The Legend of Zelda: Tears of the Kingdom.

Поддерживаемые точные builds:
- 1.4.0 — BID 6265F94D606242CE
- 1.4.1 — BID 965EAB9CEB8EB867
- 1.4.2 — BID 5CB42B1CF25469FB
- 1.4.3 — BID 277178B7DBA1B6D4
- Title ID: 0100F2C0115B6000

## Что исправлено

- CI использует devkitPro/devkitA64 и проверяет наличие `switch_rules`, C++-компилятора и `elf2nro` до сборки.
- libtesla берётся из зафиксированного коммита Status-Monitor-Deux, в котором есть совместимые с текущим проектом имена кнопок и сигнатура `handleInput`.
- `dmnt:cht` берётся из зафиксированного коммита Shiny-Stash-Live-Map вместе с `libdmntcht.a` и `dmntcht.h`.
- `source/ui.cpp` явно подключает `<cmath>` для `sqrt/lround`.
- Для точных builds 1.4.0-1.4.3 добавлен resolver актёра `Player`: профиль выбирается автоматически по BID. Для каждого build используется свой `sceneModule`, а цепочка resident actor и поля `ActorName`/`ActorPosition` общая.
- Перед использованием exact resolver проверяются Title ID и короткий 16-символьный BID через `main_nso_build_id`; неподдерживаемый build не получает чужие offsets.
- Артефакт CI принудительно включает скрытый каталог `.overlays`.
- CI проверяет последние 4 байта `.ovl` на сигнатуру `ULTR`, необходимую для распознавания Ultrahand.
- CI проверяет формат и слой каждой строки `points.csv`.

## Текущее состояние

Основной путь координат — resolver актёра `Player` для поддерживаемых builds 1.4.0-1.4.3: он автоматически выбирает профиль по BID, находит resident actor с именем `Player` и читает позицию из `ActorPosition`. На каждом обновлении читается сохранённый actor address; периодически resolver повторно валидирует actor и автоматически восстанавливается после смены процесса игры.

Эвристический сканер сохранён как fallback для неподдерживаемых builds и на случай отказа exact resolver. Он ищет тройки `float` в памяти процесса и использует движение игрока/изменение высоты для отбора кандидата.

В `data/points.csv` сейчас находится 152 святилища. Формат: `Type,Name,X,Y,Z,Layer`. Источник с колонками `X,Y,Height` нормализован в игровой порядок `X,Y,Z`, где игровая `Z` — высота. Сырой actor position из памяти TOTK имеет порядок `X,Height,Z` и перед выводом переставляется в `X,Y,Z`.

Список Nearby использует консервативную фильтрацию: в Depths остаются Depths-точки, а вне Depths доступны и Surface, и Sky. Это избегает ложного определения Sky по одной только высоте, поскольку некоторые Surface-точки находятся высоко. Diagnostics показывает адрес Player actor и количество отброшенных строк CSV.

Diagnostics показывает фактические Version/BID, определён ли build как поддерживаемый и какой источник координат активен.

Диагностика дополнительно записывается в:
`sd:/switch/totk_explorer/log.txt`.

После повторного аудита дополнительно защищено переключение между процессами: старые actor/heap-профили сбрасываются при изменении PID или базовых адресов.

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
