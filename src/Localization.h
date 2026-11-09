#pragma once

#include <string>
#include <functional>

// Minimal, dependency-free RU/EN string table. Plain std::wstring instead
// of Qt's translation machinery -- there's no Qt at all in this build.
class Localization
{
public:
    enum Lang { Ru, En };

    enum Key {
        AppTitle,

        // Toolbar
        AddButton,
        PauseButton,
        ResumeButton,
        DeleteButton,
        OpenFolderButton,

        // Table columns
        ColName,
        ColSize,
        ColProgress,
        ColDown,
        ColUp,
        ColRemaining,
        ColPeers,
        ColState,

        // Table cell text
        FetchingMetadata,
        StateDownloading,
        StateSeeding,
        StatePaused,
        StateErrorPrefix,

        // Status bar / shutdown. Tokens @DOWN@/@UP@/@COUNT@/@SECONDS@ are
        // substituted by the caller via std::wstring::replace -- same
        // named-token approach used for the stylesheet replace() calls in
        // the old Qt build, chosen specifically to avoid the >9-numbered-
        // placeholder ambiguity bug that bit this project once already.
        StatusLine,                  // "↓ @DOWN@   ↑ @UP@   @COUNT@ задач"
        ShuttingDownMessage,         // "...Shutting down in @SECONDS@ s..."
        ShuttingDownMessageWithHint,

        ShutdownCheckbox,

        // Empty state
        EmptyStateTitle,
        EmptyStateHint,

        // Add-torrent dialog
        AddDialogTitle,
        AddDialogMagnetLabel,
        AddDialogMagnetPlaceholder,
        AddDialogOr,
        AddDialogChooseFile,
        AddDialogSaveToLabel,
        AddDialogChangeButton,
        AddDialogHint,
        AddDialogCancel,
        AddDialogConfirm,

        // Dialogs
        OpenTorrentDialogTitle,
        TorrentFilesFilter,
        CouldntAddTorrentTitle,
        RemoveFilesQuestionTitle,
        RemoveFilesQuestion,
        YesButton,
        NoButton,
        ChooseFolderTitle,

        // Added in 1.1
        StateChecking,
        StateMetadata,
        AlreadyInListMessage,
        ShutdownBlockedByError,
        CountdownTitle,
        CancelShutdownButton,
        CancelButton,

        KeyCount
    };

    static Localization& instance();

    Lang language() const { return m_lang; }
    void setLanguage(Lang lang);

    std::wstring t(Key key) const;

    // Fired after setLanguage() actually changes the language, so the UI
    // can retranslate itself.
    std::function<void()> onLanguageChanged;

private:
    Localization();
    Lang m_lang;
};
