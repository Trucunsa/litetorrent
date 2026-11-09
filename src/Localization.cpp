#include "Localization.h"
#include "AppSettings.h"

namespace {
// [key][Lang] -- kept in the same order as Localization::Key.
const wchar_t* const kStrings[Localization::KeyCount][2] = {
    /* AppTitle                    */ {L"LiteTorrent", L"LiteTorrent"},

    /* AddButton                   */ {L"Добавить", L"Add"},
    /* PauseButton                 */ {L"Пауза", L"Pause"},
    /* ResumeButton                */ {L"Возобновить", L"Resume"},
    /* DeleteButton                */ {L"Удалить", L"Delete"},
    /* OpenFolderButton            */ {L"Папка", L"Folder"},

    /* ColName                     */ {L"Имя", L"Name"},
    /* ColSize                     */ {L"Размер", L"Size"},
    /* ColProgress                 */ {L"Прогресс", L"Progress"},
    /* ColDown                     */ {L"Загрузка", L"Down"},
    /* ColUp                       */ {L"Отдача", L"Up"},
    /* ColRemaining                */ {L"Осталось", L"Remaining"},
    /* ColPeers                    */ {L"Пиры", L"Peers"},
    /* ColState                    */ {L"Статус", L"State"},

    /* FetchingMetadata            */ {L"(получение метаданных...)", L"(fetching metadata...)"},
    /* StateDownloading            */ {L"Загрузка", L"Downloading"},
    /* StateSeeding                */ {L"Раздача", L"Seeding"},
    /* StatePaused                 */ {L"Пауза", L"Paused"},
    /* StateChecking               */ {L"Проверка", L"Checking"},
    /* StateQueued                 */ {L"В очереди", L"Queued"},
    /* StateMetadata               */ {L"Получение метаданных", L"Fetching metadata"},
    /* StateErrorPrefix            */ {L"Ошибка: ", L"Error: "},

    /* StatusLine                  */ {L"↓ @DOWN@   ↑ @UP@   @COUNT@ задач",
                                         L"↓ @DOWN@   ↑ @UP@   @COUNT@ task(s)"},
    /* ShuttingDownMessage         */ {L"Все загрузки завершены. Выключение через @SECONDS@ с...",
                                         L"All downloads finished. Shutting down in @SECONDS@ s..."},
    /* ShuttingDownMessageWithHint */ {L"Все загрузки завершены. Выключение через @SECONDS@ с... "
                                         L"(добавьте торрент или снимите галочку, чтобы отменить)",
                                         L"All downloads finished. Shutting down in @SECONDS@ s... "
                                         L"(add a torrent or uncheck the box to cancel)"},

    /* ShutdownCheckbox            */ {L"Выключить ПК после завершения загрузок", L"Shut down PC when all downloads finish"},
    /* ShutdownPromptTitle         */ {L"Выключение компьютера", L"Shutting down"},
    /* ShutdownPromptText          */ {L"Все загрузки завершены.\nКомпьютер выключится через @SECONDS@ с.",
                                         L"All downloads have finished.\nThe PC will shut down in @SECONDS@ s."},
    /* ShutdownPromptCancel        */ {L"Отменить выключение", L"Cancel shutdown"},
    /* DuplicateTorrent            */ {L"Этот торрент уже есть в списке.", L"This torrent is already in the list."},

    /* EmptyStateTitle             */ {L"Пока нет задач", L"No tasks yet"},
    /* EmptyStateHint              */ {L"Перетащите .torrent-файл в это окно или добавьте по magnet-ссылке",
                                         L"Drop a .torrent file into this window, or add a magnet link"},

    /* AddDialogTitle              */ {L"Добавить торрент", L"Add torrent"},
    /* AddDialogMagnetLabel        */ {L"Magnet-ссылка", L"Magnet link"},
    /* AddDialogMagnetPlaceholder  */ {L"magnet:?xt=urn:btih:...", L"magnet:?xt=urn:btih:..."},
    /* AddDialogOr                 */ {L"или", L"or"},
    /* AddDialogChooseFile         */ {L"Выбрать .torrent файл...", L"Choose a .torrent file..."},
    /* AddDialogSaveToLabel        */ {L"Сохранить в:", L"Save to:"},
    /* AddDialogChangeButton       */ {L"Изменить...", L"Change..."},
    /* AddDialogHint                */ {L"Запомним этот путь и предложим его для следующей задачи",
                                         L"We'll remember this folder and suggest it next time"},
    /* AddDialogCancel             */ {L"Отмена", L"Cancel"},
    /* AddDialogConfirm            */ {L"Добавить", L"Add"},

    /* OpenTorrentDialogTitle      */ {L"Открыть файл торрента", L"Open torrent file"},
    /* TorrentFilesFilter          */ {L"Файлы торрентов (*.torrent)", L"Torrent files (*.torrent)"},
    /* CouldntAddTorrentTitle      */ {L"Не удалось добавить торрент", L"Couldn't add torrent"},
    /* RemoveFilesQuestionTitle    */ {L"Удаление задачи", L"Removing task"},
    /* RemoveFilesQuestion         */ {L"Удалить также файлы с диска?", L"Also delete the downloaded files from disk?"},
    /* YesButton                   */ {L"Да, с файлами", L"Yes, with files"},
    /* NoButton                    */ {L"Нет, только из списка", L"No, list only"},
    /* ChooseFolderTitle           */ {L"Папка для сохранения", L"Save-to folder"},

    /* FilesButton                 */ {L"Файлы", L"Files"},
    /* MoreButton                  */ {L"Ещё", L"More"},
    /* FilesDialogTitle            */ {L"Файлы", L"Files"},
    /* FilesColName                */ {L"Файл", L"File"},
    /* FilesSelectAll              */ {L"Все", L"All"},
    /* FilesSelectNone             */ {L"Ни одного", L"None"},
    /* FilesOk                     */ {L"Готово", L"OK"},
    /* FilesSummary                */ {L"Выбрано @COUNT@ из @ALL@, @SIZE@", L"@COUNT@ of @ALL@ selected, @SIZE@"},
    /* FilesNotReady               */ {L"Список файлов появится, когда будут получены метаданные торрента. Попробуйте через минуту.",
                                         L"The file list appears once the torrent's metadata has arrived. Try again in a minute."},
    /* MenuDownloadLimit           */ {L"Ограничить загрузку", L"Limit download"},
    /* MenuUploadLimit             */ {L"Ограничить отдачу", L"Limit upload"},
    /* MenuNoLimit                 */ {L"Без ограничения", L"No limit"},
    /* MenuOpenWith                */ {L"Открывать .torrent и magnet-ссылки в LiteTorrent", L"Open .torrent files and magnet links with LiteTorrent"},
    /* AssocTitle                  */ {L"LiteTorrent", L"LiteTorrent"},
    /* AssocQuestion               */ {L"Открывать файлы .torrent и magnet-ссылки в LiteTorrent?\n\nЭто можно изменить позже в меню «Ещё».",
                                         L"Open .torrent files and magnet links with LiteTorrent?\n\nYou can change this later in the \"More\" menu."},
    /* AssocYes                    */ {L"Да", L"Yes"},
    /* AssocNo                     */ {L"Нет", L"No"},
    /* AssocTakenText              */ {L"Сейчас .torrent или magnet-ссылки открывает другая программа. Windows не даёт сменить это без вас: "
                                         L"в окне «Приложения по умолчанию» выберите LiteTorrent для .torrent и для MAGNET.",
                                         L"Another program currently opens .torrent files or magnet links. Windows won't let an app change that by itself: "
                                         L"in \"Default apps\", choose LiteTorrent for .torrent and for MAGNET."},
    /* AssocOpenSettings           */ {L"Открыть параметры Windows", L"Open Windows Settings"},
    /* LimitTag                    */ {L"(макс. @LIMIT@)", L"(max @LIMIT@)"},
};
}

Localization::Localization()
{
    AppSettings settings;
    m_lang = (settings.language() == L"en") ? En : Ru;
}

Localization& Localization::instance()
{
    static Localization instance;
    return instance;
}

void Localization::setLanguage(Lang lang)
{
    if (lang == m_lang)
        return;
    m_lang = lang;

    AppSettings settings;
    settings.setLanguage(lang == En ? L"en" : L"ru");

    if (onLanguageChanged)
        onLanguageChanged();
}

std::wstring Localization::t(Key key) const
{
    return kStrings[key][m_lang];
}
