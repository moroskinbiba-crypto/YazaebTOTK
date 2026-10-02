# TOTK Explorer v3.3.0 — build pipeline

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

## Что пока НЕ утверждается

Основной путь координат теперь не сканер, а resolver актёра `Player` для build `1.4.3`: он находит resident actor с именем `Player` и читает `X/Y/Z` из actor layout. Однако этот путь пока не проверен нами на физическом Switch; CI подтверждает только компиляцию.

Эвристический сканер сохранён как fallback. Он ищет тройки `float` в памяти процесса и использует движение игрока/изменение высоты для отбора кандидата.

База точек намеренно пустая, кроме комментариев в `data/points.csv`. Карта и список nearby показывают только точки, которые будут добавлены в этот CSV после отдельной проверки.

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
