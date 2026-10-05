# TOTK Explorer v3.4.2 — multi-version Player resolver

Ultrahand/Tesla-compatible overlay for The Legend of Zelda: Tears of the Kingdom.

Поддерживаемые точные builds:
- 1.4.0 — BID 6265F94D606242CE
- 1.4.1 — BID 965EAB9CEB8EB867
- 1.4.2 — BID 5CB42B1CF25469FB
- 1.4.3 — BID 277178B7DBA1B6D4
- Title ID: 0100F2C0115B6000

## Текущий архитектурный статус

- Сборка использует libultrahand; для CI и setup pinned commit соответствует submodule текущего Tetris Overlay: `1b7a64a4d73489c870f3fb9caa9927e9a2347478`.
- `dmnt:cht` не инициализируется до появления GUI. Сервис подключается лениво при выборе Player Coordinates и освобождается в `exitServices()`.
- CI собирает обычный `TOTK-Explorer-v3.nro` и overlay `TOTK-Explorer-v3.ovl`.
- `.ovl` строится как тот же NRO плюс последние 4 байта `ULTR`.
- NACP встраивается в NRO через `NROFLAGS --nacp`. Это важно для Ultrahand: при сканировании `*.ovl` он читает NRO header, asset header и NACP, а затем получает имя/версию overlay.
- NRO остаётся доступным как отдельный диагностический вариант для запуска через Homebrew Menu.

## Координаты

Основной путь координат — exact resolver актёра `Player` для builds 1.4.0-1.4.3: профиль выбирается по BID, затем находится resident actor с именем `Player`, после чего читается его позиция.

Эвристический сканер сохранён как fallback для неподдерживаемых builds и при отказе exact resolver. Он ищет тройки `float` в памяти процесса и фильтрует их по движению и изменению высоты.

Переход между игровыми процессами обрабатывается безопасно: при смене PID/base старые actor/profile данные сбрасываются.

## Данные карты

`data/points.csv` содержит 152 святилища.

Формат:
`Type,Name,X,Y,Z,Layer`

## Диагностика

Логи:
`sdmc:/switch/totk_explorer/log.txt`

В Diagnostics показываются:
- фактический Game Version;
- BID;
- PID и heap size;
- статус `dmnt:cht`;
- источник координат;
- адрес Player actor;
- число отклонённых строк CSV.

## Сборка

Локально:

```sh
bash tools/setup_deps.sh
make clean
make -j2
```

Результат:
- `TOTK-Explorer-v3.nro`
- `TOTK-Explorer-v3.ovl`

## Установка

Из CI artifact скопировать содержимое `sd` в корень microSD:

```text
sd:/switch/.overlays/TOTK-Explorer-v3.ovl
sd:/switch/totk_explorer/TOTK-Explorer-v3.nro
sd:/switch/totk_explorer/points.csv
```

Для Ultrahand используется `TOTK-Explorer-v3.ovl`.

Для отдельной проверки через Homebrew Menu можно запускать `TOTK-Explorer-v3.nro`.

Физическая проверка на Switch всё равно обязательна: CI подтверждает сборку и структуру NRO/OVL, но не заменяет runtime-тест на консоли.
