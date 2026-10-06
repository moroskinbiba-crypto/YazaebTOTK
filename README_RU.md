# TOTK Explorer — Stage 1

Минимальный Tesla/libultrahand overlay для **The Legend of Zelda: Tears of the Kingdom**.

На этом этапе намеренно **нет**:
- `dmnt:cht`;
- чтения памяти игры;
- resolver/калибровки;
- карты и CSV runtime;
- persistent HUD;
- фоновых потоков;
- собственных сервисов.

Есть только базовый Tesla lifecycle:
- заголовок `TOTK Explorer / Stage 1`;
- выход по **B**.

## Зафиксированный toolchain

- devkitPro Switch container: `devkitpro/devkita64:20260219`
- devkitA64: `r29.2`
- libnx: `4.12.0`
- libultrahand/libtesla: commit `1b7a64a4d73489c870f3fb9caa9927e9a2347478`

CI проверяет эти версии перед сборкой.

## Первый hardware test

Целевая среда из плана:
- Nintendo Switch firmware **22.5.0**
- Atmosphere **1.11.2**

Проверка Stage 1 выполняется отдельно от всех следующих функций:

1. Без запущенной игры открыть Ultrahand/Tesla.
2. Запустить `TOTK-Explorer.ovl`.
3. Убедиться, что появляется заголовок.
4. Нажать **B** и убедиться, что overlay закрывается без fatal/crash.
5. Повторить запуск/выход несколько раз.
6. Затем повторить то же самое при открытой игре.

**До подтверждения этого теста новый runtime-слой не добавляется.**

## CI

GitHub Actions собирает:
- `TOTK-Explorer.nro`
- `TOTK-Explorer.ovl`

У `.ovl` проверяется:
- первые байты == NRO;
- последние 4 байта == `ULTR`;
- бинарник не содержит старого memory/HUD runtime.

CI не доказывает поведение на реальной Switch.
