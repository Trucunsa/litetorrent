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
