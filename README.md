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
| `protectcmds-css-sm113.zip` | CS:S (Steam) | 1.13 и новее |

## Установка

1. Распакуйте архив в папку `cstrike` сервера (внутри архива `addons/sourcemod/...`):
   - `extensions/protectcmds.ext.so` и `extensions/protectcmds.autoload`
   - `data/protectcmds/protectcmds.ini`, `blockcmds.txt`, `steamid_hash.txt`
   - `scripting/include/protectcmds.inc` (только для разработчиков плагинов)
2. Перезапустите сервер. `sm exts list` должен показать `ProtectCMDS`.

Без `data/protectcmds/protectcmds.ini` (или с `protectcmds_enable=0`) расширение не загружается: `Failed to load settings`.

## ProcessCmds

ProtectCMDS не работает вместе с ProcessCmds: оба перехватывают одни и те же функции движка, и сервер падает. Если в процессе сервера загружена библиотека ProcessCmds (любой файл с именем на `processcmds`, на CS:S это расширение SourceMod `processcmds.ext.*.so`), ProtectCMDS не запускается:

- ProcessCmds уже загружен — ProtectCMDS не загружается, в `sm exts list` видна ошибка `processcmds.ext.1.ep1.so is loaded: ProtectCMDS conflicts with ProcessCmds and does not start`;
- ProcessCmds загрузился после ProtectCMDS при старте сервера — ProtectCMDS снимает свои перехваты, пишет ту же ошибку в лог SourceMod и в `sm exts list` значится неработающим;
- ProcessCmds загружен вручную (`sm exts load`) на уже работающем сервере — ProtectCMDS снять перехваты уже не может и на ближайшей смене карты пишет ошибку в лог: перезапустите сервер без ProcessCmds.

Оставьте одно из двух расширений. `preload_x86.so` из комплекта ProcessCmds на запуск ProtectCMDS не влияет.

## Хеширование SteamID

Модуль `SteamIDHashing` выключен по умолчанию (`steamidhashing_enable=0`). Включенный, он заменяет SteamID каждого игрока на хеш от SteamID и соли. Прежде чем включать:

- **SteamID на сервере меняются.** Админы (`admins_simple.ini`, `admins.cfg`), баны, VIP и статистика, привязанные к настоящим SteamID, перестают совпадать. Новый SteamID игрока виден в консоли сервера при заходе: `[STEAM_0:1:123] changed to [STEAM_0:0:456]`.
- **Соль создается при первом запуске с включенным хешированием** в `data/protectcmds/protectcmds_steamid_salt.dat` (в консоли `[ProtectCMDS] Salt is loaded!`). Чтобы у игрока был один и тот же SteamID на нескольких серверах, скопируйте этот файл на все сервера до включения хеширования на них. Не публикуйте соль: зная ее, можно посчитать SteamID любого игрока.
- **Если соль потерять, все SteamID изменятся.** Сделайте копию файла.
- В `steamid_hash.txt` можно вручную задать SteamID для конкретного игрока: `STEAM_0:0:432548923 = STEAM_0:1:666`.

## Статистика пакетов

Модуль `FilterPacket` исправляет пустые UDP-пакеты (как dosprotect, поэтому ставить dosprotect не нужно) и отбрасывает split-пакеты (v34). Сколько таких пакетов пришло и от кого, показывает серверная команда:

```
sm protectcmds packets         // статус, итоги и таблица IP | empty | split | flood | first seen | last seen
sm protectcmds packets reset   // очистить
```

Если с одного IP (или со всех вместе) за `filterpacket_stats_log_interval` секунд пришло `filterpacket_stats_log_threshold` или больше таких пакетов, в лог SourceMod пишется строка с адресом и числом. Без атаки таких пакетов нет, и лог пустой. Таблица хранит 256 адресов, новые вытесняют давно не появлявшиеся.

На v34 можно ограничить connectionless-пакеты (запросы сервера, challenge, connect) с одного IP в секунду: `filterpacket_connectionless_limit` (0 — без ограничения). Лишние отбрасываются и попадают в статистику как `flood`.

## Обновление

Автообновление включено по умолчанию (`[Updater]` в `protectcmds.ini`). При каждом старте сервера расширение сверяет sha256 своего файла с последним релизом. Если вышла новая версия, она скачивается в `extensions/protectcmds.ext.so.new`, проверяется по sha256 и атомарно (`rename()`) встает на место `protectcmds.ext.so`; загруженная копия при этом не меняется, новая начинает работать после следующего рестарта. Прошлая версия остается в `protectcmds.ext.so.prev`: для отката переименуйте ее обратно и перезапустите сервер. Конфиги и соль не трогаются.

Ключ `protectcmds_updater_url` из конфигов 1.0.10 больше не используется (в лог пишется предупреждение): обновления берутся из релизов GitHub, репозиторий задает `protectcmds_updater_repository`.

Обновляя вручную, заменяйте только `extensions/protectcmds.ext.so` из архива: распаковка всего архива перезапишет ваши `protectcmds.ini` и `blockcmds.txt`.

## Для разработчиков плагинов

`scripting/include/protectcmds.inc`: форварды `ProtectCMDS_OnClientPreConnect`, `ProtectCMDS_OnClientPreConnectPost`, `ProtectCMDS_OnClientCommand`, `OnClientVoice` и натив `ProtectCMDS_GetAbsoluteFrameTime`.

## Сборка

См. `.github/workflows/build.yml`: Ubuntu 20.04, `make ENGINE=css34|css`, для SourceMod 1.13 `SMLINE=sm113` с исходниками из [fmu1337/sourcemod-css34](https://github.com/fmu1337/sourcemod-css34).
