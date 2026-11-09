# Changelog / История изменений

## 1.2.0

**English**

- Optionally opens `.torrent` files and `magnet:` links: asked once on the first start, switchable under **More**. Per-user registration (no admin rights); if another program owns them, LiteTorrent says so and offers to open Settings → Default apps.
- **Files** button: choose which files of a torrent to download. The choice is saved.
- **More** menu with download and upload speed limits; an active limit is shown in the status bar. Limits are now counted in KiB/s (1024 bytes), matching the speeds shown.
- Fixed a possible crash: the "already in the list" message was shown while the engine's events were being processed, and the timer could re-enter that processing behind the message box.
- A torrent handed over by a second copy is added after the hand-over completes, so that copy never waits behind a message box.
- Releases are now built by GitHub Actions from the tagged commit; the release notes link the build log and list the exe's SHA-256.

**Русский**

- По желанию открывает файлы `.torrent` и ссылки `magnet:`: вопрос один раз при первом запуске, переключается в меню **Ещё**. Регистрация для текущего пользователя, без прав администратора; если их занимает другая программа, LiteTorrent сообщит об этом и предложит открыть «Параметры → Приложения по умолчанию».
- Кнопка **Файлы**: выбор, какие файлы торрента скачивать. Выбор сохраняется.
- Меню **Ещё** с ограничением скорости загрузки и отдачи; действующее ограничение видно в строке состояния. Ограничения теперь считаются в КиБ/с (1024 байта), как и показанные скорости.
- Исправлено возможное падение: сообщение «уже в списке» показывалось во время разбора событий движка, и таймер мог повторно войти в этот разбор, пока окно сообщения открыто.
- Торрент, переданный второй копией программы, добавляется после завершения передачи, поэтому та копия не ждёт за окном сообщения.
- Релизы теперь собирает GitHub Actions из помеченного коммита; в описании релиза есть ссылка на журнал сборки и SHA-256 exe.

## 1.1.0

**English**

- The task list, progress and paused state are now saved (`%APPDATA%\LiteTorrent\resume`) and restored on the next start. Progress is written once a minute and on exit, including when Windows shuts down.
- Only one copy runs: opening a `.torrent` while the app is running adds it to the existing window. Adding a torrent that is already in the list shows a notice.
- The shutdown countdown is now a window in the middle of the screen, on top of everything, with a "Cancel shutdown" button.
- The shutdown feature no longer fires when a torrent has stopped with an error, nor when the box is ticked over a list that was already complete.
- The interface follows the display scale (125–200 %) instead of staying at 100 % size.
- Pausing a torrent now sticks (the engine's queue could resume it before).
- "Folder" opens the selected torrent's own folder.
- The delete confirmation uses the app's language for its buttons.
- New states in the list: Checking, Queued, Fetching metadata.

**Русский**

- Список задач, прогресс и состояние паузы теперь сохраняются (`%APPDATA%\LiteTorrent\resume`) и восстанавливаются при следующем запуске. Прогресс записывается раз в минуту и при выходе, в том числе при выключении Windows.
- Работает только одна копия: если открыть `.torrent` при запущенной программе, он добавится в существующее окно. При добавлении торрента, который уже есть в списке, показывается сообщение.
- Отсчёт перед выключением теперь идёт в окне по центру экрана поверх всех окон, с кнопкой «Отменить выключение».
- Выключение больше не срабатывает, если какой-то торрент остановился с ошибкой, и если галочку поставили при уже готовом списке.
- Интерфейс следует масштабу экрана (125–200 %), а не остаётся в размере 100 %.
- Пауза теперь действительно держится (раньше очередь движка могла снять её сама).
- «Папка» открывает папку выбранного торрента.
- Кнопки в окне удаления — на языке программы.
- Новые статусы в списке: «Проверка», «В очереди», «Получение метаданных».

## 1.0.0

First public release. / Первый публичный выпуск.
