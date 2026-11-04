# ProtectCMDS

Расширение SourceMod для защиты сервера CS:S от эксплоитов: фильтрация кривых пакетов, блокировка опасных команд, фикс голосового чата, хеширование SteamID и фиксы крашей движка. Форк [WeSTManCoder/ProtectCMDS](https://github.com/WeSTManCoder/ProtectCMDS) со сборкой в GitHub Actions и автообновлением из релизов этого репозитория.

Только Linux.

## Какой архив скачать

В [релизах](https://github.com/fmu1337/ProtectCMDS/releases/latest) лежат готовые архивы:

| Архив | Игра | SourceMod |
|---|---|---|
| `protectcmds-css34.zip` | CS:S v34 | 1.7 – 1.12 |
| `protectcmds-css34-sm113.zip` | CS:S v34 | 1.13 и новее |
| `protectcmds-css.zip` | CS:S (Steam) | 1.7 – 1.12 |

SourceMod 1.13 не загружает сборку для 1.7–1.12 (`Extension version is too old to load`), а старые SourceMod не загружают сборку для 1.13 (`too new`).

Отдельные `.so` и `.sha256` в релизе нужны автообновлению; вручную их скачивать не нужно.

## Установка

1. Распакуйте архив в папку `cstrike` сервера (внутри архива `addons/sourcemod/...`):
   - `extensions/protectcmds.ext.so` и `extensions/protectcmds.autoload`
   - `data/protectcmds/protectcmds.ini`, `blockcmds.txt`, `steamid_hash.txt`
   - `scripting/include/protectcmds.inc` (только для разработчиков плагинов)
2. Перезапустите сервер. В консоли сервера должно быть `[ProtectCMDS] Salt is loaded!`, а `sm exts list` показывает `ProtectCMDS`.

Без `data/protectcmds/protectcmds.ini` (или с `protectcmds_enable=0`) расширение не загружается: `Failed to load settings`.

## Хеширование SteamID: прочитайте до установки

Модуль `SteamIDHashing` (включен по умолчанию) заменяет SteamID каждого игрока на хеш от SteamID и соли. Поэтому:

- **SteamID на сервере меняются.** Админы (`admins_simple.ini`, `admins.cfg`), баны, VIP и статистика, привязанные к настоящим SteamID, перестают совпадать. Новый SteamID игрока виден в консоли сервера при заходе: `[STEAM_0:1:123] changed to [STEAM_0:0:456]`.
- **Соль создается при первом запуске** в `data/protectcmds/protectcmds_steamid_salt.dat`. Чтобы у игрока был один и тот же SteamID на нескольких серверах, скопируйте этот файл на все сервера до их первого запуска. Не публикуйте соль: зная ее, можно посчитать SteamID любого игрока.
- **Если соль потерять, все SteamID изменятся.** Сделайте копию файла.
- В `steamid_hash.txt` можно вручную задать SteamID для конкретного игрока: `STEAM_0:0:432548923 = STEAM_0:1:666`.

Если хеширование не нужно, поставьте `steamidhashing_enable=0` до первого запуска.

## Обновление

Автообновление включено по умолчанию (`[Updater]` в `protectcmds.ini`). При каждом старте сервера расширение проверяет последний релиз (sha256), скачивает новую версию поверх `extensions/protectcmds.ext.so` и начинает использовать ее после следующего рестарта. Конфиги и соль не трогаются.

Обновляя вручную, заменяйте только `extensions/protectcmds.ext.so` из архива: распаковка всего архива перезапишет ваши `protectcmds.ini` и `blockcmds.txt`.

## Для разработчиков плагинов

`scripting/include/protectcmds.inc`: форварды `ProtectCMDS_OnClientPreConnect`, `ProtectCMDS_OnClientPreConnectPost`, `ProtectCMDS_OnClientCommand`, `OnClientVoice` и натив `ProtectCMDS_GetAbsoluteFrameTime`.

## Сборка

См. `.github/workflows/build.yml`: Ubuntu 20.04, `make ENGINE=css34|css`, для SourceMod 1.13 `SMLINE=sm113` с исходниками из [fmu1337/sourcemod-css34](https://github.com/fmu1337/sourcemod-css34).
