# LiteTorrent — памятка для Claude (читать первой)

Лёгкий Windows x64 торрент-клиент: нативный Win32 C++17, libtorrent 2.0.11 и OpenSSL статически, один exe (~9,5 МБ). Владелец не разработчик: результат всегда = запушенный репозиторий + готовая сборка. Общение по-русски.

## Устройство
- `src/main.cpp` — вход, одиночный экземпляр (мьютекс `Local\LiteTorrent.SingleInstance`, окно `LiteTorrentMainWindow`, передача через WM_COPYDATA).
- `src/MainWindow.*` — главное окно, кнопки, меню «Ещё», отложенные действия (WM_APP+1).
- `src/SessionManager.*` — обёртка libtorrent: список, resume-данные (`%APPDATA%\LiteTorrent\resume`), выбор файлов, лимиты скорости (КиБ×1024).
- `src/AddTorrentDialog.*`, `FilesDialog.*` — диалоги добавления и выбора файлов.
- `src/Associations.*` — привязка .torrent/magnet (HKCU, необязательная).
- `src/ShutdownManager.*` — выключение ПК с обратным отсчётом.
- `src/AppSettings.*` — настройки в реестре `HKCU\Software\LiteTorrent`.
- `src/Localization.*` — RU/EN строки; новая строка = ключ + оба языка.
- `resources/app.rc` — версия exe. `ci/` + `.github/workflows/build.yml` — сборка и публикация релиза.

## Правила
- Новый пользовательский текст: оба языка. Не использовать экзотические глифы (в Wine показываются квадратом).
- Выпуск версии: поднять версию в `resources/app.rc` и user agent в SessionManager, добавить раздел в `CHANGELOG.md`, `release-notes/<тег>.md` (RU+EN), обновить README.md и README.ru.md.
- Сайт GitHub: тег и релиз создаёт владелец на сайте, CI сам прикрепляет zip, SHA-256 и ссылку на лог. Из сессии теги и релизы (403) и `gh` (токен недействителен) недоступны.
- Git: автор `Claude <noreply@anthropic.com>`, никогда не почта владельца. Принудительный push не делать без прямого разрешения. Коммит ed0ed7b содержит почту владельца — решение за ним.
- В конце коммита: `Co-Authored-By: Claude Sonnet 5.5 <noreply@anthropic.com>` и `Claude-Session:` из текущей сессии.

## Сборка и проверка
- Локально: `SYSROOT=<mingw64> ./build.sh` (Debian mingw-w64 GCC posix). Sysroot с проверкой SHA-256: `ci/fetch-sysroot.sh <dir>`; упаковка: `ci/package.sh`.
- Проверка под Wine: Xvfb :98, `WINEPREFIX=/tmp/wineprefix`, `/usr/lib/wine/wine64`, без оконного менеджера (окно в X=4,Y=30, абсолютные координаты xdotool, Alt+F4 закрывает). Снимок: `import -window root -crop 972x586+4+30 +repage`. Для magnet в Wine нужен HKLM-шим.
- Автотестов нет; владелец проверяет каждую версию вручную в Windows.

## Скриншоты
`docs/screenshot-*.png`; делаются под Wine на свежей сборке, README это оговаривает.
