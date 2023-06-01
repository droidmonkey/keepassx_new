/*
 *  Copyright (C) 2024 KeePassXC Team <team@keepassxc.org>
 *  Copyright (C) 2011 Felix Geyer <debfx@fobos.de>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 or (at your option)
 *  version 3 of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef KEEPASSX_CONFIG_H
#define KEEPASSX_CONFIG_H

#include <QPointer>
#include <QVariant>
#include <QVector>

class QSettings;

class Config : public QObject
{
    Q_OBJECT

public:
    Q_DISABLE_COPY(Config)

    enum ConfigKey
    {
        SingleInstance,
        RememberLastDatabases,
        NumberOfRememberedLastDatabases,
        RememberLastKeyFiles,
        OpenPreviousDatabasesOnStartup,
        AutoSaveAfterEveryChange,
        AutoReloadOnChange,
        AutoSaveOnExit,
        AutoSaveNonDataChanges,
        BackupBeforeSave,
        BackupFilePathPattern,
        UseAtomicSaves,
        UseDirectWriteSaves,
        SearchLimitGroup,
        MinimizeOnOpenUrl,
        OpenURLOnDoubleClick,
        HideWindowOnCopy,
        MinimizeOnCopy,
        MinimizeAfterUnlock,
        DropToBackgroundOnCopy,
        UseGroupIconOnEntryCreation,
        AutoTypeEntryTitleMatch,
        AutoTypeEntryURLMatch,
        AutoTypeDelay,
        AutoTypeStartDelay,
        AutoTypeHideExpiredEntry,
        AutoTypeDialogSortColumn,
        AutoTypeDialogSortOrder,
        GlobalAutoTypeKey,
        GlobalAutoTypeModifiers,
        GlobalAutoTypeRetypeTime,
        FaviconDownloadTimeout,
        UpdateCheckMessageShown,
        DefaultDatabaseFileName,

        LastDatabases,
        LastKeyFiles,
        LastChallengeResponse,
        LastActiveDatabase,
        LastOpenedDatabases,
        LastDir,

        GUI_Language,
        GUI_HideMenubar,
        GUI_HideToolbar,
        GUI_MovableToolbar,
        GUI_HideGroupPanel,
        GUI_HidePreviewPanel,
        GUI_AlwaysOnTop,
        GUI_ToolButtonStyle,
        GUI_LaunchAtStartup,
        GUI_ShowTrayIcon,
        GUI_TrayIconAppearance,
        GUI_MinimizeToTray,
        GUI_MinimizeOnStartup,
        GUI_MinimizeOnClose,
        GUI_HideUsernames,
        GUI_HidePasswords,
        GUI_ColorPasswords,
        GUI_MonospaceNotes,
        GUI_ApplicationTheme,
        GUI_CompactMode,
        GUI_CheckForUpdates,
        GUI_CheckForUpdatesIncludeBetas,
        GUI_ShowExpiredEntriesOnDatabaseUnlock,
        GUI_ShowExpiredEntriesOnDatabaseUnlockOffsetDays,

        GUI_MainWindowGeometry,
        GUI_MainWindowState,
        GUI_ListViewState,
        GUI_SearchViewState,
        GUI_PreviewSplitterState,
        GUI_SplitterState,
        GUI_GroupSplitterState,
        GUI_AutoTypeSelectDialogSize,
        GUI_CheckForUpdatesNextCheck,

        Security_ClearClipboard,
        Security_ClearClipboardTimeout,
        Security_ClearSearch,
        Security_ClearSearchTimeout,
        Security_HideNotes,
        Security_LockDatabaseIdle,
        Security_LockDatabaseIdleSeconds,
        Security_LockDatabaseMinimize,
        Security_LockDatabaseScreenLock,
        Security_LockDatabaseOnUserSwitch,
        Security_RelockAutoType,
        Security_PasswordsHidden,
        Security_PasswordEmptyPlaceholder,
        Security_HidePasswordPreviewPanel,
        Security_HideTotpPreviewPanel,
        Security_AutoTypeAsk,
        Security_IconDownloadFallback,
        Security_NoConfirmMoveEntryToRecycleBin,
        Security_EnableCopyOnDoubleClick,
        Security_QuickUnlock,
        Security_DatabasePasswordMinimumQuality,

        Browser_Enabled,
        Browser_ShowNotification,
        Browser_BestMatchOnly,
        Browser_UnlockDatabase,
        Browser_MatchUrlScheme,
        Browser_SupportBrowserProxy,
        Browser_UseCustomProxy,
        Browser_CustomProxyLocation,
        Browser_UpdateBinaryPath,
        Browser_AllowGetDatabaseEntriesRequest,
        Browser_AllowExpiredCredentials,
        Browser_AlwaysAllowAccess,
        Browser_AlwaysAllowUpdate,
        Browser_HttpAuthPermission,
        Browser_SearchInAllDatabases,
        Browser_SupportKphFields,
        Browser_NoMigrationPrompt,
        Browser_UseCustomBrowser,
        Browser_CustomBrowserType,
        Browser_CustomBrowserLocation,
        Browser_AllowLocalhostWithPasskeys,
#ifdef QT_DEBUG
        Browser_CustomExtensionId,
#endif

        SSHAgent_Enabled,
        SSHAgent_UseOpenSSH,
        SSHAgent_UsePageant,
        SSHAgent_AuthSockOverride,
        SSHAgent_SecurityKeyProviderOverride,

        FdoSecrets_Enabled,
        FdoSecrets_ShowNotification,
        FdoSecrets_ConfirmDeleteItem,
        FdoSecrets_ConfirmAccessItem,
        FdoSecrets_UnlockBeforeSearch,

        KeeShare_QuietSuccess,
        KeeShare_Own,
        KeeShare_Foreign,
        KeeShare_Active,

        PasswordGenerator_LowerCase,
        PasswordGenerator_UpperCase,
        PasswordGenerator_Numbers,
        PasswordGenerator_EASCII,
        PasswordGenerator_AdvancedMode,
        PasswordGenerator_SpecialChars,
        PasswordGenerator_AdditionalChars,
        PasswordGenerator_Braces,
        PasswordGenerator_Punctuation,
        PasswordGenerator_Quotes,
        PasswordGenerator_Dashes,
        PasswordGenerator_Math,
        PasswordGenerator_Logograms,
        PasswordGenerator_ExcludedChars,
        PasswordGenerator_ExcludeAlike,
        PasswordGenerator_EnsureEvery,
        PasswordGenerator_Length,
        PasswordGenerator_WordCount,
        PasswordGenerator_WordSeparator,
        PasswordGenerator_WordList,
        PasswordGenerator_WordCase,
        PasswordGenerator_Type,

        Messages_NoLegacyKeyFileWarning,
        Messages_HidePreReleaseWarning,

        // Special internal value
        Deleted
    };

    struct ShortcutEntry
    {
        QString name;
        QString shortcut;
    };

    ~Config() override;
    QVariant get(ConfigKey key);
    QVariant getDefault(ConfigKey key);
    QString getFileName();
    void set(ConfigKey key, const QVariant& value);
    void remove(ConfigKey key);
    bool hasAccessError();
    void sync();
    void resetToDefaults();

    bool importSettings(const QString& fileName);
    void exportSettings(const QString& fileName) const;

    QList<ShortcutEntry> getShortcuts() const;
    void setShortcuts(const QList<ShortcutEntry>& shortcuts);

    static Config* instance();
    static void createConfigFromFile(const QString& configFileName, const QString& localConfigFileName = {});
    static void createTempFileInstance();
    static bool isPortable();
    static QString portableConfigDir();

signals:
    void changed(ConfigKey key);

private:
    Config(const QString& configFileName, const QString& localConfigFileName, QObject* parent);
    explicit Config(QObject* parent);
    void init(const QString& configFileName, const QString& localConfigFileName);
    void migrate();
    static QPair<QString, QString> defaultConfigFiles();

    static QPointer<Config> m_instance;

    QScopedPointer<QSettings> m_settings;
    QScopedPointer<QSettings> m_localSettings;
    QHash<QString, QVariant> m_defaults;
};

inline Config* config()
{
    return Config::instance();
}

#endif // KEEPASSX_CONFIG_H
