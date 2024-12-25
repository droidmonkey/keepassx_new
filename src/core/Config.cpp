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

#include "Config.h"
#include "Global.h"

#include <QCoreApplication>
#include <QDir>
#include <QHash>
#include <QProcessEnvironment>
#include <QSettings>
#include <QSize>
#include <QStandardPaths>
#include <QTemporaryFile>

#define CONFIG_VERSION 2
#define QS QStringLiteral

enum ConfigType
{
    Local,
    Roaming
};

struct ConfigDirective
{
    QString name;
    ConfigType type;
    QVariant defaultValue;
};

// clang-format off

/**
 * Map of legal config values, their type and default value.
 */
static const QHash<Config::ConfigKey, ConfigDirective> configStrings = {
    // General
    {Config::SingleInstance,{QS("SingleInstance"), Roaming, true}},
    {Config::RememberLastDatabases,{QS("RememberLastDatabases"), Roaming, true}},
    {Config::NumberOfRememberedLastDatabases,{QS("NumberOfRememberedLastDatabases"), Roaming, 5}},
    {Config::RememberLastKeyFiles,{QS("RememberLastKeyFiles"), Roaming, true}},
    {Config::OpenPreviousDatabasesOnStartup,{QS("OpenPreviousDatabasesOnStartup"), Roaming, true}},
    {Config::AutoSaveAfterEveryChange,{QS("AutoSaveAfterEveryChange"), Roaming, true}},
    {Config::AutoReloadOnChange,{QS("AutoReloadOnChange"), Roaming, true}},
    {Config::AutoSaveOnExit,{QS("AutoSaveOnExit"), Roaming, true}},
    {Config::AutoSaveNonDataChanges,{QS("AutoSaveNonDataChanges"), Roaming, true}},
    {Config::BackupBeforeSave,{QS("BackupBeforeSave"), Roaming, false}},
    {Config::BackupFilePathPattern,{QS("BackupFilePathPattern"), Roaming, QString("{DB_FILENAME}.old.kdbx")}},
    {Config::UseAtomicSaves,{QS("UseAtomicSaves"), Roaming, true}},
    {Config::UseDirectWriteSaves,{QS("UseDirectWriteSaves"), Local, false}},
    {Config::SearchLimitGroup,{QS("SearchLimitGroup"), Roaming, false}},
    {Config::MinimizeOnOpenUrl,{QS("MinimizeOnOpenUrl"), Roaming, false}},
    {Config::OpenURLOnDoubleClick, {QS("OpenURLOnDoubleClick"), Roaming, true}},
    {Config::HideWindowOnCopy,{QS("HideWindowOnCopy"), Roaming, false}},
    {Config::MinimizeOnCopy,{QS("MinimizeOnCopy"), Roaming, true}},
    {Config::MinimizeAfterUnlock,{QS("MinimizeAfterUnlock"), Roaming, false}},
    {Config::DropToBackgroundOnCopy,{QS("DropToBackgroundOnCopy"), Roaming, false}},
    {Config::UseGroupIconOnEntryCreation,{QS("UseGroupIconOnEntryCreation"), Roaming, true}},
    {Config::AutoTypeEntryTitleMatch,{QS("AutoTypeEntryTitleMatch"), Roaming, true}},
    {Config::AutoTypeEntryURLMatch,{QS("AutoTypeEntryURLMatch"), Roaming, true}},
    {Config::AutoTypeDelay,{QS("AutoTypeDelay"), Roaming, 25}},
    {Config::AutoTypeStartDelay,{QS("AutoTypeStartDelay"), Roaming, 500}},
    {Config::AutoTypeHideExpiredEntry,{QS("AutoTypeHideExpiredEntry"), Roaming, false}},
    {Config::GlobalAutoTypeKey,{QS("GlobalAutoTypeKey"), Roaming, 0}},
    {Config::GlobalAutoTypeModifiers,{QS("GlobalAutoTypeModifiers"), Roaming, 0}},
    {Config::GlobalAutoTypeRetypeTime,{QS("GlobalAutoTypeRetypeTime"), Roaming, 15}},
    {Config::FaviconDownloadTimeout,{QS("FaviconDownloadTimeout"), Roaming, 10}},
    {Config::UpdateCheckMessageShown,{QS("UpdateCheckMessageShown"), Roaming, false}},
    {Config::DefaultDatabaseFileName,{QS("DefaultDatabaseFileName"), Roaming, {}}},

    {Config::LastDatabases, {QS("LastDatabases"), Local, {}}},
    {Config::LastKeyFiles, {QS("LastKeyFiles"), Local, {}}},
    {Config::LastChallengeResponse, {QS("LastChallengeResponse"), Local, {}}},
    {Config::LastActiveDatabase, {QS("LastActiveDatabase"), Local, {}}},
    {Config::LastOpenedDatabases, {QS("LastOpenedDatabases"), Local, {}}},
    {Config::LastDir, {QS("LastDir"), Local, QDir::homePath()}},

    // GUI
    {Config::GUI_Language, {QS("GUI/Language"), Roaming, QS("system")}},
    {Config::GUI_HideMenubar, {QS("GUI/HideMenubar"), Roaming, false}},
    {Config::GUI_HideToolbar, {QS("GUI/HideToolbar"), Roaming, false}},
    {Config::GUI_MovableToolbar, {QS("GUI/MovableToolbar"), Roaming, false}},
    {Config::GUI_HideGroupPanel, {QS("GUI/HideGroupPanel"), Roaming, false}},
    {Config::GUI_HidePreviewPanel, {QS("GUI/HidePreviewPanel"), Roaming, false}},
    {Config::GUI_AlwaysOnTop, {QS("GUI/GUI_AlwaysOnTop"), Local, false}},
    {Config::GUI_ToolButtonStyle, {QS("GUI/ToolButtonStyle"), Roaming, Qt::ToolButtonIconOnly}},
    {Config::GUI_LaunchAtStartup, {QS("GUI/LaunchAtStartup"), Roaming, false}},
    {Config::GUI_ShowTrayIcon, {QS("GUI/ShowTrayIcon"), Roaming, false}},
    {Config::GUI_TrayIconAppearance, {QS("GUI/TrayIconAppearance"), Roaming, {}}},
    {Config::GUI_MinimizeToTray, {QS("GUI/MinimizeToTray"), Roaming, false}},
    {Config::GUI_MinimizeOnStartup, {QS("GUI/MinimizeOnStartup"), Roaming, false}},
    {Config::GUI_MinimizeOnClose, {QS("GUI/MinimizeOnClose"), Roaming, false}},
    {Config::GUI_HideUsernames, {QS("GUI/HideUsernames"), Roaming, false}},
    {Config::GUI_HidePasswords, {QS("GUI/HidePasswords"), Roaming, true}},
    {Config::GUI_ColorPasswords, {QS("GUI/ColorPasswords"), Roaming, false}},
    {Config::GUI_MonospaceNotes, {QS("GUI/MonospaceNotes"), Roaming, false}},
    {Config::GUI_ApplicationTheme, {QS("GUI/ApplicationTheme"), Roaming, QS("auto")}},
    {Config::GUI_CompactMode, {QS("GUI/CompactMode"), Roaming, false}},
    {Config::GUI_CheckForUpdates, {QS("GUI/CheckForUpdates"), Roaming, true}},
    {Config::GUI_CheckForUpdatesNextCheck, {QS("GUI/CheckForUpdatesNextCheck"), Local, 0}},
    {Config::GUI_CheckForUpdatesIncludeBetas, {QS("GUI/CheckForUpdatesIncludeBetas"), Roaming, false}},
    {Config::GUI_ShowExpiredEntriesOnDatabaseUnlock, {QS("GUI/ShowExpiredEntriesOnDatabaseUnlock"), Roaming, true}},
    {Config::GUI_ShowExpiredEntriesOnDatabaseUnlockOffsetDays, {QS("GUI/ShowExpiredEntriesOnDatabaseUnlockOffsetDays"), Roaming, 3}},

    {Config::GUI_MainWindowGeometry, {QS("GUI/MainWindowGeometry"), Local, {}}},
    {Config::GUI_MainWindowState, {QS("GUI/MainWindowState"), Local, {}}},
    {Config::GUI_ListViewState, {QS("GUI/ListViewState"), Local, {}}},
    {Config::GUI_SearchViewState, {QS("GUI/SearchViewState"), Local, {}}},
    {Config::GUI_SplitterState, {QS("GUI/SplitterState"), Local, {}}},
    {Config::GUI_GroupSplitterState, {QS("GUI/GroupSplitterState"), Local, {}}},
    {Config::GUI_PreviewSplitterState, {QS("GUI/PreviewSplitterState"), Local, {}}},
    {Config::GUI_AutoTypeSelectDialogSize, {QS("GUI/AutoTypeSelectDialogSize"), Local, QSize(600, 250)}},

    // Security
    {Config::Security_ClearClipboard, {QS("Security/ClearClipboard"), Roaming, true}},
    {Config::Security_ClearClipboardTimeout, {QS("Security/ClearClipboardTimeout"), Roaming, 10}},
    {Config::Security_ClearSearch, {QS("Security/ClearSearch"), Roaming, false}},
    {Config::Security_ClearSearchTimeout, {QS("Security/ClearSearchTimeout"), Roaming, 5}},
    {Config::Security_HideNotes, {QS("Security/Security_HideNotes"), Roaming, false}},
    {Config::Security_LockDatabaseIdle, {QS("Security/LockDatabaseIdle"), Roaming, false}},
    {Config::Security_LockDatabaseIdleSeconds, {QS("Security/LockDatabaseIdleSeconds"), Roaming, 240}},
    {Config::Security_LockDatabaseMinimize, {QS("Security/LockDatabaseMinimize"), Roaming, false}},
    {Config::Security_LockDatabaseScreenLock, {QS("Security/LockDatabaseScreenLock"), Roaming, true}},
    {Config::Security_LockDatabaseOnUserSwitch, {QS("Security/LockDatabaseOnUserSwitch"), Roaming, true}},
    {Config::Security_RelockAutoType, {QS("Security/RelockAutoType"), Roaming, false}},
    {Config::Security_PasswordsHidden, {QS("Security/PasswordsHidden"), Roaming, true}},
    {Config::Security_PasswordEmptyPlaceholder, {QS("Security/PasswordEmptyPlaceholder"), Roaming, false}},
    {Config::Security_HidePasswordPreviewPanel, {QS("Security/HidePasswordPreviewPanel"), Roaming, true}},
    {Config::Security_HideTotpPreviewPanel, {QS("Security/HideTotpPreviewPanel"), Roaming, false}},
    {Config::Security_AutoTypeAsk, {QS("Security/AutotypeAsk"), Roaming, true}},
    {Config::Security_IconDownloadFallback, {QS("Security/IconDownloadFallback"), Roaming, false}},
    {Config::Security_NoConfirmMoveEntryToRecycleBin,{QS("Security/NoConfirmMoveEntryToRecycleBin"), Roaming, true}},
    {Config::Security_EnableCopyOnDoubleClick,{QS("Security/EnableCopyOnDoubleClick"), Roaming, false}},
    {Config::Security_QuickUnlock, {QS("Security/QuickUnlock"), Local, true}},
    {Config::Security_DatabasePasswordMinimumQuality, {QS("Security/DatabasePasswordMinimumQuality"), Local, 0}},

    // Browser
    {Config::Browser_Enabled, {QS("Browser/Enabled"), Roaming, false}},
    {Config::Browser_ShowNotification, {QS("Browser/ShowNotification"), Roaming, true}},
    {Config::Browser_BestMatchOnly, {QS("Browser/BestMatchOnly"), Roaming, false}},
    {Config::Browser_UnlockDatabase, {QS("Browser/UnlockDatabase"), Roaming, true}},
    {Config::Browser_MatchUrlScheme, {QS("Browser/MatchUrlScheme"), Roaming, true}},
    {Config::Browser_SupportBrowserProxy, {QS("Browser/SupportBrowserProxy"), Roaming, true}},
    {Config::Browser_UseCustomProxy, {QS("Browser/UseCustomProxy"), Roaming, false}},
    {Config::Browser_CustomProxyLocation, {QS("Browser/CustomProxyLocation"), Roaming, {}}},
    {Config::Browser_UpdateBinaryPath, {QS("Browser/UpdateBinaryPath"), Roaming, true}},
    {Config::Browser_AllowGetDatabaseEntriesRequest, {QS("Browser/AllowGetDatabaseEntriesRequest"), Roaming, false}},
    {Config::Browser_AllowExpiredCredentials, {QS("Browser/AllowExpiredCredentials"), Roaming, false}},
    {Config::Browser_AlwaysAllowAccess, {QS("Browser/AlwaysAllowAccess"), Roaming, false}},
    {Config::Browser_AlwaysAllowUpdate, {QS("Browser/AlwaysAllowUpdate"), Roaming, false}},
    {Config::Browser_HttpAuthPermission, {QS("Browser/HttpAuthPermission"), Roaming, false}},
    {Config::Browser_SearchInAllDatabases, {QS("Browser/SearchInAllDatabases"), Roaming, false}},
    {Config::Browser_SupportKphFields, {QS("Browser/SupportKphFields"), Roaming, true}},
    {Config::Browser_NoMigrationPrompt, {QS("Browser/NoMigrationPrompt"), Roaming, false}},
    {Config::Browser_UseCustomBrowser, {QS("Browser/UseCustomBrowser"), Local, false}},
    {Config::Browser_CustomBrowserType, {QS("Browser/CustomBrowserType"), Local, -1}},
    {Config::Browser_CustomBrowserLocation, {QS("Browser/CustomBrowserLocation"), Local, {}}},
    {Config::Browser_AllowLocalhostWithPasskeys, {QS("Browser/Browser_AllowLocalhostWithPasskeys"), Roaming, false}},
#ifdef QT_DEBUG
    {Config::Browser_CustomExtensionId, {QS("Browser/CustomExtensionId"), Local, {}}},
#endif

    // SSHAgent
    {Config::SSHAgent_Enabled, {QS("SSHAgent/Enabled"), Roaming, false}},
    {Config::SSHAgent_UseOpenSSH, {QS("SSHAgent/UseOpenSSH"), Roaming, false}},
    {Config::SSHAgent_UsePageant, {QS("SSHAgent/UsePageant"), Roaming, true} },
    {Config::SSHAgent_AuthSockOverride, {QS("SSHAgent/AuthSockOverride"), Local, {}}},
    {Config::SSHAgent_SecurityKeyProviderOverride, {QS("SSHAgent/SecurityKeyProviderOverride"), Local, {}}},

    // FdoSecrets
    {Config::FdoSecrets_Enabled, {QS("FdoSecrets/Enabled"), Roaming, false}},
    {Config::FdoSecrets_ShowNotification, {QS("FdoSecrets/ShowNotification"), Roaming, true}},
    {Config::FdoSecrets_ConfirmDeleteItem, {QS("FdoSecrets/ConfirmDeleteItem"), Roaming, true}},
    {Config::FdoSecrets_ConfirmAccessItem, {QS("FdoSecrets/ConfirmAccessItem"), Roaming, true}},
    {Config::FdoSecrets_UnlockBeforeSearch, {QS("FdoSecrets/UnlockBeforeSearch"), Roaming, true}},

    // KeeShare
    {Config::KeeShare_QuietSuccess, {QS("KeeShare/QuietSuccess"), Roaming, false}},
    {Config::KeeShare_Own, {QS("KeeShare/Own"), Roaming, {}}},
    {Config::KeeShare_Foreign, {QS("KeeShare/Foreign"), Roaming, {}}},
    {Config::KeeShare_Active, {QS("KeeShare/Active"), Roaming, {}}},

    // PasswordGenerator
    {Config::PasswordGenerator_LowerCase, {QS("PasswordGenerator/LowerCase"), Roaming, true}},
    {Config::PasswordGenerator_UpperCase, {QS("PasswordGenerator/UpperCase"), Roaming, true}},
    {Config::PasswordGenerator_Numbers, {QS("PasswordGenerator/Numbers"), Roaming, true}},
    {Config::PasswordGenerator_EASCII, {QS("PasswordGenerator/EASCII"), Roaming, false}},
    {Config::PasswordGenerator_AdvancedMode, {QS("PasswordGenerator/AdvancedMode"), Roaming, false}},
    {Config::PasswordGenerator_SpecialChars, {QS("PasswordGenerator/SpecialChars"), Roaming, true}},
    {Config::PasswordGenerator_Braces, {QS("PasswordGenerator/Braces"), Roaming, false}},
    {Config::PasswordGenerator_Punctuation, {QS("PasswordGenerator/Punctuation"), Roaming, false}},
    {Config::PasswordGenerator_Quotes, {QS("PasswordGenerator/Quotes"), Roaming, false}},
    {Config::PasswordGenerator_Dashes, {QS("PasswordGenerator/Dashes"), Roaming, false}},
    {Config::PasswordGenerator_Math, {QS("PasswordGenerator/Math"), Roaming, false}},
    {Config::PasswordGenerator_Logograms, {QS("PasswordGenerator/Logograms"), Roaming, false}},
    {Config::PasswordGenerator_AdditionalChars, {QS("PasswordGenerator/AdditionalChars"), Roaming, {}}},
    {Config::PasswordGenerator_ExcludedChars, {QS("PasswordGenerator/ExcludedChars"), Roaming, {}}},
    {Config::PasswordGenerator_ExcludeAlike, {QS("PasswordGenerator/ExcludeAlike"), Roaming, true}},
    {Config::PasswordGenerator_EnsureEvery, {QS("PasswordGenerator/EnsureEvery"), Roaming, true}},
    {Config::PasswordGenerator_Length, {QS("PasswordGenerator/Length"), Roaming, 20}},
    {Config::PasswordGenerator_WordCount, {QS("PasswordGenerator/WordCount"), Roaming, 7}},
    {Config::PasswordGenerator_WordSeparator, {QS("PasswordGenerator/WordSeparator"), Roaming, QS(" ")}},
    {Config::PasswordGenerator_WordList, {QS("PasswordGenerator/WordList"), Roaming, QS("eff_large.wordlist")}},
    {Config::PasswordGenerator_WordCase, {QS("PasswordGenerator/WordCase"), Roaming, 0}},
    {Config::PasswordGenerator_Type, {QS("PasswordGenerator/Type"), Roaming, 0}},

    // Messages
    {Config::Messages_NoLegacyKeyFileWarning, {QS("Messages/NoLegacyKeyFileWarning"), Roaming, false}},
    {Config::Messages_HidePreReleaseWarning, {QS("Messages/HidePreReleaseWarning"), Local, {}}}};

// clang-format on

QPointer<Config> Config::m_instance(nullptr);

QVariant Config::get(ConfigKey key)
{
    auto cfg = configStrings[key];
    auto defaultValue = configStrings[key].defaultValue;
    if (m_localSettings && cfg.type == Local) {
        return m_localSettings->value(cfg.name, defaultValue);
    }
    return m_settings->value(cfg.name, defaultValue);
}

QVariant Config::getDefault(Config::ConfigKey key)
{
    return configStrings[key].defaultValue;
}

bool Config::hasAccessError()
{
    return m_settings->status() & QSettings::AccessError;
}

QString Config::getFileName()
{
    return m_settings->fileName();
}

void Config::set(ConfigKey key, const QVariant& value)
{
    if (get(key) == value) {
        return;
    }

    auto cfg = configStrings[key];
    if (cfg.type == Local && m_localSettings) {
        m_localSettings->setValue(cfg.name, value);
    } else {
        m_settings->setValue(cfg.name, value);
    }

    emit changed(key);
}

void Config::remove(ConfigKey key)
{
    auto cfg = configStrings[key];
    if (cfg.type == Local && m_localSettings) {
        m_localSettings->remove(cfg.name);
    } else {
        m_settings->remove(cfg.name);
    }

    emit changed(key);
}

/**
 * Sync configuration with persistent storage.
 *
 * Usually, you don't need to call this method manually, but if you are writing
 * configurations after an emitted \link QCoreApplication::aboutToQuit() signal,
 * use it to guarantee your config values are persisted.
 */
void Config::sync()
{
    m_settings->sync();
    if (m_localSettings) {
        m_localSettings->sync();
    }
}

void Config::resetToDefaults()
{
    m_settings->clear();
    if (m_localSettings) {
        m_localSettings->clear();
    }
}

bool Config::importSettings(const QString& fileName)
{
    // Ensure file is valid ini with values
    QSettings settings(fileName, QSettings::IniFormat);
    if (settings.status() != QSettings::NoError || settings.allKeys().isEmpty()) {
        return false;
    }

    // Only import valid roaming settings
    auto isValidSetting = [](const QString& key) {
        for (const auto& value : configStrings.values()) {
            if (value.type == ConfigType::Roaming && value.name == key) {
                return true;
            }
        }
        return false;
    };

    // Clear existing settings and set valid items
    m_settings->clear();
    for (const auto& key : settings.allKeys()) {
        if (isValidSetting(key)) {
            m_settings->setValue(key, settings.value(key));
        }
    }

    sync();

    return true;
}

void Config::exportSettings(const QString& fileName) const
{
    QSettings settings(fileName, QSettings::IniFormat);
    for (const auto& key : m_settings->allKeys()) {
        settings.setValue(key, m_settings->value(key));
    }
}

/**
 * Map of configuration file settings that are either deprecated, or have
 * had their name changed to their new config enum values.
 *
 * Set a value to Deleted to remove the setting.
 */
static const QHash<QString, Config::ConfigKey> deprecationMap = {
    // 2.3.4
    {QS("security/hidepassworddetails"), Config::Security_HidePasswordPreviewPanel},
    {QS("GUI/HideDetailsView"), Config::GUI_HidePreviewPanel},
    {QS("GUI/DetailSplitterState"), Config::GUI_PreviewSplitterState},
    {QS("security/IconDownloadFallbackToGoogle"), Config::Security_IconDownloadFallback},

    // 2.6.0
    {QS("security/autotypeask"), Config::Security_AutoTypeAsk},
    {QS("security/clearclipboard"), Config::Security_ClearClipboard},
    {QS("security/clearclipboardtimeout"), Config::Security_ClearClipboardTimeout},
    {QS("security/clearsearch"), Config::Security_ClearSearch},
    {QS("security/clearsearchtimeout"), Config::Security_ClearSearchTimeout},
    {QS("security/lockdatabaseidle"), Config::Security_LockDatabaseIdle},
    {QS("security/lockdatabaseidlesec"), Config::Security_LockDatabaseIdleSeconds},
    {QS("security/lockdatabaseminimize"), Config::Security_LockDatabaseMinimize},
    {QS("security/lockdatabasescreenlock"), Config::Security_LockDatabaseScreenLock},
    {QS("security/relockautotype"), Config::Security_RelockAutoType},
    {QS("security/IconDownloadFallback"), Config::Security_IconDownloadFallback},
    {QS("security/passwordscleartext"), Config::Security_PasswordsHidden},
    {QS("security/passwordemptynodots"), Config::Security_PasswordEmptyPlaceholder},
    {QS("security/HidePasswordPreviewPanel"), Config::Security_HidePasswordPreviewPanel},
    {QS("security/passwordsrepeat"), Config::Deleted},
    {QS("security/hidenotes"), Config::Security_HideNotes},
    {QS("KeeShare/Settings.own"), Config::KeeShare_Own},
    {QS("KeeShare/Settings.foreign"), Config::KeeShare_Foreign},
    {QS("KeeShare/Settings.active"), Config::KeeShare_Active},
    {QS("SSHAgent"), Config::SSHAgent_Enabled},
    {QS("SSHAgentOpenSSH"), Config::SSHAgent_UseOpenSSH},
    {QS("SSHAuthSockOverride"), Config::SSHAgent_AuthSockOverride},
    {QS("generator/LowerCase"), Config::PasswordGenerator_LowerCase},
    {QS("generator/UpperCase"), Config::PasswordGenerator_UpperCase},
    {QS("generator/Numbers"), Config::PasswordGenerator_Numbers},
    {QS("generator/EASCII"), Config::PasswordGenerator_EASCII},
    {QS("generator/AdvancedMode"), Config::PasswordGenerator_AdvancedMode},
    {QS("generator/SpecialChars"), Config::PasswordGenerator_SpecialChars},
    {QS("generator/AdditionalChars"), Config::PasswordGenerator_AdditionalChars},
    {QS("generator/Braces"), Config::PasswordGenerator_Braces},
    {QS("generator/Punctuation"), Config::PasswordGenerator_Punctuation},
    {QS("generator/Quotes"), Config::PasswordGenerator_Quotes},
    {QS("generator/Dashes"), Config::PasswordGenerator_Dashes},
    {QS("generator/Math"), Config::PasswordGenerator_Math},
    {QS("generator/Logograms"), Config::PasswordGenerator_Logograms},
    {QS("generator/ExcludedChars"), Config::PasswordGenerator_ExcludedChars},
    {QS("generator/ExcludeAlike"), Config::PasswordGenerator_ExcludeAlike},
    {QS("generator/EnsureEvery"), Config::PasswordGenerator_EnsureEvery},
    {QS("generator/Length"), Config::PasswordGenerator_Length},
    {QS("generator/WordCount"), Config::PasswordGenerator_WordCount},
    {QS("generator/WordSeparator"), Config::PasswordGenerator_WordSeparator},
    {QS("generator/WordList"), Config::PasswordGenerator_WordList},
    {QS("generator/WordCase"), Config::PasswordGenerator_WordCase},
    {QS("generator/Type"), Config::PasswordGenerator_Type},
    {QS("QtErrorMessageShown"), Config::Deleted},
    {QS("GUI/HidePasswords"), Config::Deleted},
    {QS("GUI/DarkTrayIcon"), Config::Deleted},

    // 2.7.0
    {QS("GUI/HideGroupsPanel"), Config::Deleted},
    {QS("LastAttachmentDir"), Config::Deleted},
    {QS("KeeShare/LastDir"), Config::Deleted},
    {QS("KeeShare/LastKeyDir"), Config::Deleted},
    {QS("KeeShare/LastShareDir"), Config::Deleted},
    {QS("UseTouchID"), Config::Deleted},
    {QS("Security/ResetTouchId"), Config::Deleted},
    {QS("Security/ResetTouchIdTimeout"), Config::Deleted},
    {QS("Security/ResetTouchIdScreenlock"), Config::Deleted},

    // 2.8.0
    {QS("GUI/AdvancedSettings"), Config::Deleted},
    {QS("Security/PasswordsRepeatVisible"), Config::Deleted}};

/**
 * Migrate settings from previous versions.
 */
void Config::migrate()
{
    int previousVersion = m_settings->value("ConfigVersion").toInt();
    if (CONFIG_VERSION <= previousVersion) {
        return;
    }

    // Update renamed keys and drop obsolete keys
    for (const auto& setting : deprecationMap.keys()) {
        QVariant value;
        if (m_settings->contains(setting)) {
            if (setting == QS("IgnoreGroupExpansion") || setting == QS("security/passwordsrepeat")
                || setting == QS("security/passwordscleartext") || setting == QS("security/passwordemptynodots")) {
                // Keep user's original setting for boolean settings whose meanings were reversed
                value = !m_settings->value(setting).toBool();
            } else {
                value = m_settings->value(setting);
            }
            m_settings->remove(setting);
        } else if (m_localSettings && m_localSettings->contains(setting)) {
            value = m_localSettings->value(setting);
            m_localSettings->remove(setting);
        } else {
            continue;
        }

        if (deprecationMap[setting] == Config::Deleted) {
            continue;
        }

        set(deprecationMap[setting], value);
    }

    // Move local settings to separate file
    if (m_localSettings) {
        for (const auto& setting : asConst(configStrings)) {
            if (setting.type == Local && m_settings->contains(setting.name)) {
                m_localSettings->setValue(setting.name, m_settings->value(setting.name));
                m_settings->remove(setting.name);
            }
        }
    }

    // Detailed version migrations

    // pre 2.6.0 (no versioned configs)
    if (previousVersion < 1) {

        // 2.3.4
        if (get(AutoSaveAfterEveryChange).toBool()) {
            set(AutoSaveOnExit, true);
        }

        // Setting defaults for 'hide window on copy' behavior, keeping the user's original setting
        if (get(HideWindowOnCopy).isNull()) {
            set(HideWindowOnCopy, get(MinimizeOnCopy).toBool());
            set(MinimizeOnCopy, true);
        }

        // Reset database columns if upgrading from pre 2.6.0
        remove(GUI_ListViewState);
    }

    m_settings->setValue("ConfigVersion", CONFIG_VERSION);
    sync();
}

Config::Config(const QString& configFileName, const QString& localConfigFileName, QObject* parent)
    : QObject(parent)
{
    init(configFileName, localConfigFileName);
}

Config::Config(QObject* parent)
    : QObject(parent)
{
    auto configFiles = defaultConfigFiles();
    init(configFiles.first, configFiles.second);
}

Config::~Config() = default;

void Config::init(const QString& configFileName, const QString& localConfigFileName)
{
    // Upgrade from previous KeePassXC version which stores its config
    // in AppData/Local on Windows instead of AppData/Roaming.
    // Move file to correct location before continuing.
    if (!localConfigFileName.isEmpty() && QFile::exists(localConfigFileName) && !QFile::exists(configFileName)) {
        QDir().mkpath(QFileInfo(configFileName).absolutePath());
        QFile::copy(localConfigFileName, configFileName);
        QFile::remove(localConfigFileName);
        QDir().rmdir(QFileInfo(localConfigFileName).absolutePath());
    }

#if defined(Q_OS_LINUX)
    // Upgrade from previous KeePassXC version which stores its config
    // in ~/.cache on Linux instead of ~/.local/state.
    // Move file to correct location before continuing.
    if (!QFile::exists(localConfigFileName)) {
        QString oldLocalConfigPath =
            QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + "/keepassxc";
        QString suffix;
#ifdef QT_DEBUG
        suffix = "_debug";
#endif
        oldLocalConfigPath += QString("/keepassxc%1.ini").arg(suffix);
        oldLocalConfigPath = QDir::toNativeSeparators(oldLocalConfigPath);
        if (QFile::exists(oldLocalConfigPath)) {
            QDir().mkpath(QFileInfo(localConfigFileName).absolutePath());
            QFile::copy(oldLocalConfigPath, localConfigFileName);
            QFile::remove(oldLocalConfigPath);
            QDir().rmdir(QFileInfo(oldLocalConfigPath).absolutePath());
        }
    }
#endif

    m_settings.reset(new QSettings(configFileName, QSettings::IniFormat));
    if (!localConfigFileName.isEmpty() && configFileName != localConfigFileName) {
        m_localSettings.reset(new QSettings(localConfigFileName, QSettings::IniFormat));
    }

    migrate();
    connect(qApp, &QCoreApplication::aboutToQuit, this, &Config::sync);
}

QPair<QString, QString> Config::defaultConfigFiles()
{
    // Check if we are running in portable mode, if so store the config files local to the app
    if (isPortable()) {
        return {portableConfigDir().append("/keepassxc.ini"), portableConfigDir().append("/keepassxc_local.ini")};
    }

    QString configPath;
    QString localConfigPath;

#if defined(Q_OS_WIN)
    configPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    localConfigPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
#elif defined(Q_OS_MACOS)
    configPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    localConfigPath = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
#else
    // On case-sensitive Operating Systems, force use of lowercase app directories
    configPath = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/keepassxc";
    // Qt does not support XDG_STATE_HOME yet, change this once XDG_STATE_HOME is added
    QString xdgStateHome = QFile::decodeName(qgetenv("XDG_STATE_HOME"));
    if (!xdgStateHome.startsWith(u'/')) {
        xdgStateHome.clear(); // spec says relative paths should be ignored
    }
    if (xdgStateHome.isEmpty()) {
        xdgStateHome = QDir::homePath() + "/.local/state";
    }

    localConfigPath = xdgStateHome + "/keepassxc";
#endif

    QString suffix;
#ifdef QT_DEBUG
    suffix = "_debug";
#endif

    configPath += QString("/keepassxc%1.ini").arg(suffix);
    localConfigPath += QString("/keepassxc%1.ini").arg(suffix);

    // Allow overriding the default location with env vars
    const auto& env = QProcessEnvironment::systemEnvironment();
    configPath = env.value("KPXC_CONFIG", configPath);
    localConfigPath = env.value("KPXC_CONFIG_LOCAL", localConfigPath);

    return {QDir::toNativeSeparators(configPath), QDir::toNativeSeparators(localConfigPath)};
}

Config* Config::instance()
{
    if (!m_instance) {
        m_instance = new Config(qApp);
    }

    return m_instance;
}

void Config::createConfigFromFile(const QString& configFileName, const QString& localConfigFileName)
{
    if (m_instance) {
        delete m_instance;
    }

    auto defaultFiles = defaultConfigFiles();
    m_instance = new Config(configFileName.isEmpty() ? defaultFiles.first : configFileName,
                            localConfigFileName.isEmpty() ? defaultFiles.second : localConfigFileName,
                            qApp);
}

void Config::createTempFileInstance()
{
    if (m_instance) {
        delete m_instance;
    }
    auto* tmpFile = new QTemporaryFile();
    bool openResult = tmpFile->open();
    Q_ASSERT(openResult);
    Q_UNUSED(openResult);
    m_instance = new Config(tmpFile->fileName(), "", qApp);
    tmpFile->setParent(m_instance);
}

bool Config::isPortable()
{
#ifdef Q_OS_WIN
    // Enable QFileInfo::isWritable check on Windows
    extern Q_CORE_EXPORT int qt_ntfs_permission_lookup;
    qt_ntfs_permission_lookup++;
#endif
    auto portablePath = QCoreApplication::applicationDirPath().append("/%1");
    auto portableFile = portablePath.arg(".portable");
    auto isPortable = QFile::exists(portableFile) && QFileInfo(portableFile).isWritable();
#ifdef Q_OS_WIN
    qt_ntfs_permission_lookup--;
#endif
    return isPortable;
}

QString Config::portableConfigDir()
{
    return QCoreApplication::applicationDirPath().append("/config");
}

QList<Config::ShortcutEntry> Config::getShortcuts() const
{
    m_settings->beginGroup("Shortcuts");
    const auto keys = m_settings->childKeys();
    QList<ShortcutEntry> ret;
    ret.reserve(keys.size());
    for (const auto& key : keys) {
        const auto shortcut = m_settings->value(key).toString();
        ret.push_back(ShortcutEntry{key, shortcut});
    }
    m_settings->endGroup();
    return ret;
}

void Config::setShortcuts(const QList<ShortcutEntry>& shortcuts)
{
    m_settings->beginGroup("Shortcuts");
    m_settings->remove(""); // clear previous
    for (const auto& shortcutEntry : shortcuts) {
        m_settings->setValue(shortcutEntry.name, shortcutEntry.shortcut);
    }
    m_settings->endGroup();
}

#undef QS
