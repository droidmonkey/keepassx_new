/*
 *  Copyright (C) 2024 KeePassXC Team <team@keepassxc.org>
 *  Copyright (C) 2017 Sami Vänttinen <sami.vanttinen@protonmail.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "NativeMessageInstaller.h"
#include "BrowserSettings.h"
#include "config-keepassx.h"
#include "core/Config.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>

using namespace BrowserShared;

namespace
{
    const QString HOST_NAME = QStringLiteral("org.keepassxc.keepassxc_browser");
    const QStringList ALLOWED_EXTENSIONS = QStringList() << QStringLiteral("keepassxc-browser@keepassxc.org");
    const QStringList ALLOWED_ORIGINS = QStringList()
                                        << QStringLiteral("chrome-extension://pdffhmdngciaglkoonimfcmckehcpafo/")
                                        << QStringLiteral("chrome-extension://oboonakemofpalcgghocfoadofidjkkk/");
#if defined(Q_OS_MACOS)
    const QString TARGET_DIR_CHROME = QStringLiteral("/Library/Application Support/Google/Chrome/NativeMessagingHosts");
    const QString TARGET_DIR_CHROMIUM = QStringLiteral("/Library/Application Support/Chromium/NativeMessagingHosts");
    const QString TARGET_DIR_FIREFOX = QStringLiteral("/Library/Application Support/Mozilla/NativeMessagingHosts");
    const QString TARGET_DIR_VIVALDI = QStringLiteral("/Library/Application Support/Vivaldi/NativeMessagingHosts");
    const QString TARGET_DIR_TOR_BROWSER =
        QStringLiteral("/Library/Application Support/TorBrowser-Data/Browser/Mozilla/NativeMessagingHosts");
    const QString TARGET_DIR_BRAVE =
        QStringLiteral("/Library/Application Support/BraveSoftware/Brave-Browser/NativeMessagingHosts");
    const QString TARGET_DIR_EDGE = QStringLiteral("/Library/Application Support/Microsoft Edge/NativeMessagingHosts");
#elif defined(Q_OS_WIN)
    const QString TARGET_DIR_CHROME = QStringLiteral(
        "HKEY_CURRENT_USER\\Software\\Google\\Chrome\\NativeMessagingHosts\\org.keepassxc.keepassxc_browser");
    const QString TARGET_DIR_CHROMIUM =
        QStringLiteral("HKEY_CURRENT_USER\\Software\\Chromium\\NativeMessagingHosts\\org.keepassxc.keepassxc_browser");
    const QString TARGET_DIR_FIREFOX =
        QStringLiteral("HKEY_CURRENT_USER\\Software\\Mozilla\\NativeMessagingHosts\\org.keepassxc.keepassxc_browser");
    const QString TARGET_DIR_VIVALDI = TARGET_DIR_CHROME;
    const QString TARGET_DIR_TOR_BROWSER = TARGET_DIR_FIREFOX;
    const QString TARGET_DIR_BRAVE = TARGET_DIR_CHROME;
    const QString TARGET_DIR_EDGE = QStringLiteral(
        "HKEY_CURRENT_USER\\Software\\Microsoft\\Edge\\NativeMessagingHosts\\org.keepassxc.keepassxc_browser");
#else
    const QString TARGET_DIR_CHROME = QStringLiteral("/google-chrome/NativeMessagingHosts");
    const QString TARGET_DIR_CHROMIUM = QStringLiteral("/chromium/NativeMessagingHosts");
    const QString TARGET_DIR_FIREFOX = QStringLiteral("/.mozilla/native-messaging-hosts");
    const QString TARGET_DIR_VIVALDI = QStringLiteral("/vivaldi/NativeMessagingHosts");
    const QString TARGET_DIR_TOR_BROWSER = QStringLiteral(
        "/torbrowser/tbb/x86_64/tor-browser_en-US/Browser/TorBrowser/Data/Browser/.mozilla/native-messaging-hosts");
    const QString TARGET_DIR_BRAVE = QStringLiteral("/BraveSoftware/Brave-Browser/NativeMessagingHosts");
    const QString TARGET_DIR_EDGE = QStringLiteral("/microsoft-edge/NativeMessagingHosts");
#endif
} // namespace

/**
 * Checks if the selected browser has native messaging host properly installed
 *
 * @param browser Selected browser
 * @return bool Script is installed correctly
 */
bool NativeMessageInstaller::isBrowserEnabled(SupportedBrowsers browser)
{
#ifdef Q_OS_WIN
    QSettings settings(getTargetPath(browser), QSettings::NativeFormat);
    return !settings.value("Default").isNull();
#else
    return QFile::exists(getNativeMessagePath(browser));
#endif
}

/**
 * Installs native messaging JSON script for the selected browser
 *
 * @param browser Selected browser
 * @param enabled Is browser integration enabled
 */
void NativeMessageInstaller::setBrowserEnabled(SupportedBrowsers browser, bool enabled)
{
    if (enabled) {
#ifdef Q_OS_WIN
        // Create a registry key
        QSettings settings(getTargetPath(browser), QSettings::NativeFormat);
        settings.setValue("Default", getNativeMessagePath(browser));
#endif
        // Always create the script file
        if (!createNativeMessageFile(browser)) {
            QMessageBox::critical(
                nullptr,
                QObject::tr("Browser Plugin Failure"),
                QObject::tr("Could not save the native messaging script file for %1.").arg(getBrowserName(browser)),
                QMessageBox::Ok);
        }
    } else {
        // Remove the script file
        const QString fileName = getNativeMessagePath(browser);
        QFile::remove(fileName);
#ifdef Q_OS_WIN
        // Remove the registry entry
        QSettings settings(getTargetPath(browser), QSettings::NativeFormat);
        settings.remove("Default");
#endif
    }
}

/**
 * Updates the paths to native messaging host for each browser that has been enabled
 */
void NativeMessageInstaller::updateBinaryPaths()
{
    for (int i = 0; i < SupportedBrowsers::MAX_SUPPORTED; ++i) {
        if (isBrowserEnabled(static_cast<SupportedBrowsers>(i))) {
            setBrowserEnabled(static_cast<SupportedBrowsers>(i), true);
        }
    }
}

/**
 * Returns the target path for each browser. Windows uses a registry path instead of a file path
 *
 * @param browser Selected browser
 * @return QString Current target path for the selected browser
 */
QString NativeMessageInstaller::getTargetPath(SupportedBrowsers browser) const
{
    switch (browser) {
    case SupportedBrowsers::CHROME:
        return TARGET_DIR_CHROME;
    case SupportedBrowsers::CHROMIUM:
        return TARGET_DIR_CHROMIUM;
    case SupportedBrowsers::FIREFOX:
        return TARGET_DIR_FIREFOX;
    case SupportedBrowsers::VIVALDI:
        return TARGET_DIR_VIVALDI;
    case SupportedBrowsers::TOR_BROWSER:
        return TARGET_DIR_TOR_BROWSER;
    case SupportedBrowsers::BRAVE:
        return TARGET_DIR_BRAVE;
    case SupportedBrowsers::EDGE:
        return TARGET_DIR_EDGE;
    case SupportedBrowsers::CUSTOM:
        return browserSettings()->customBrowserLocation();
    default:
        return {};
    }
}

/**
 * Returns the browser name
 * Needed for Windows to separate Chromium- or Firefox-based scripts
 *
 * @param browser Selected browser
 * @return QString Name of the selected browser
 */
QString NativeMessageInstaller::getBrowserName(SupportedBrowsers browser) const
{
    switch (browser) {
    case SupportedBrowsers::CHROME:
        return QStringLiteral("chrome");
    case SupportedBrowsers::CHROMIUM:
        return QStringLiteral("chromium");
    case SupportedBrowsers::FIREFOX:
        return QStringLiteral("firefox");
    case SupportedBrowsers::VIVALDI:
        return QStringLiteral("vivaldi");
    case SupportedBrowsers::TOR_BROWSER:
        return QStringLiteral("tor-browser");
    case SupportedBrowsers::BRAVE:
        return QStringLiteral("brave");
    case SupportedBrowsers::EDGE:
        return QStringLiteral("edge");
    case SupportedBrowsers::CUSTOM:
        return QStringLiteral("custom");
    default:
        return {};
    }
}

/**
 * Returns the path of native messaging JSON script for the selected browser
 *
 * @param browser Selected browser
 * @return QString JSON script path for the selected browser
 */
QString NativeMessageInstaller::getNativeMessagePath(SupportedBrowsers browser) const
{
    QString basePath;
#if defined(Q_OS_WIN)
    // If portable settings file exists save the JSON scripts to the application folder
    if (Config::isPortable()) {
        basePath = Config::portableConfigDir();
    } else {
        basePath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    }
    return QStringLiteral("%1/%2_%3.json").arg(basePath, HOST_NAME, getBrowserName(browser));
#elif defined(KEEPASSXC_DIST_FLATPAK)
    // Flatpak sandboxes do not have access to the XDG_DATA_HOME and XDG_CONFIG_HOME variables
    // defined in the host, so we must hardcode them here.
    if (browser == SupportedBrowsers::TOR_BROWSER) {
        basePath = QDir::homePath() + "/.local/share";
    } else if (browser == SupportedBrowsers::FIREFOX) {
        basePath = QDir::homePath();
    } else {
        basePath = QDir::homePath() + "/.config";
    }
#elif defined(KEEPASSXC_DIST_SNAP)
    // Same as Flatpak above, with the exception that Snap also redefines $HOME
    // Therefore we must explicitly reference $SNAP_REAL_HOME
    if (browser == SupportedBrowsers::TOR_BROWSER) {
        basePath = qEnvironmentVariable("SNAP_REAL_HOME") + "/.local/share";
    } else if (browser == SupportedBrowsers::FIREFOX) {
        basePath = qEnvironmentVariable("SNAP_REAL_HOME");
    } else {
        basePath = qEnvironmentVariable("SNAP_REAL_HOME") + "/.config";
    }
#elif defined(Q_OS_LINUX) || (defined(Q_OS_UNIX) && !defined(Q_OS_MACOS))
    if (browser == SupportedBrowsers::TOR_BROWSER) {
        basePath = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    } else if (browser == SupportedBrowsers::FIREFOX) {
        basePath = QDir::homePath();
    } else {
        basePath = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    }
#else
    basePath = QDir::homePath();
#endif
    if (browser == SupportedBrowsers::CUSTOM) {
        return QString("%1/%2.json").arg(getTargetPath(browser), HOST_NAME);
    }

    return QStringLiteral("%1%2/%3.json").arg(basePath, getTargetPath(browser), HOST_NAME);
}

#ifdef KEEPASSXC_DIST_FLATPAK
/** Constructs a host accessible proxy path for use with flatpak
 *
 * @return path Path to host accessible wrapper script (org.keepassxc.KeePassXC)
 */
QString constructFlatpakPath()
{
    // Find and extract the host flatpak data directory (in /var)
    QString path;
    QSettings settings("/.flatpak-info", QSettings::IniFormat);
    settings.beginGroup("Instance");
    QString appPath = settings.value("app-path").toString();

    QRegularExpression re("^((?:/[\\.\\w-]*)+)+/app");
    QRegularExpressionMatch match = re.match(appPath);
    if (match.hasMatch()) {
        // Construct a proxy path that should work with all flatpak installations
        path = match.captured(1) + "/exports/bin/" + "org.keepassxc.KeePassXC";
    } else {
        // Fallback to the most common and default flatpak installation path
        path = "/var/lib/flatpak/exports/bin/org.keepassxc.KeePassXC";
    }
    settings.endGroup();

    return path;
}
#endif

/**
 * Returns the effective proxy path used to build the native messaging JSON script
 */
QString NativeMessageInstaller::getProxyPath() const
{
    QString result;
    if (browserSettings()->useCustomProxy()) {
        result = browserSettings()->customProxyLocation();
    } else {
        result = getInstalledProxyPath();
    }
    return result;
}

/**
 * Returns the original proxy path at the time of installation
 */
QString NativeMessageInstaller::getInstalledProxyPath() const
{
    QString path;
#if defined(KEEPASSXC_DIST_APPIMAGE)
    path = QProcessEnvironment::systemEnvironment().value("APPIMAGE");
#elif defined(KEEPASSXC_DIST_FLATPAK)
    path = constructFlatpakPath();
#elif defined(KEEPASSXC_DIST_SNAP)
    path = "/snap/bin/keepassxc.proxy";
#else
    path = QCoreApplication::applicationDirPath() + QStringLiteral("/keepassxc-proxy");
#ifdef Q_OS_WIN
    path.append(QStringLiteral(".exe"));
#endif // #ifdef Q_OS_WIN

#endif // #ifdef KEEPASSXC_DIST_APPIMAGE
    return QDir::toNativeSeparators(path);
}

/**
 * Constructs the JSON script file used with native messaging
 *
 * @param browser Browser (Chromium- and Firefox-based browsers need a different parameters for the script)
 * @param location Custom proxy location
 * @return script The JSON script file
 */
QJsonObject NativeMessageInstaller::constructFile(SupportedBrowsers browser)
{
    QJsonObject script;
    script["name"] = HOST_NAME;
    script["description"] = QStringLiteral("KeePassXC integration with native messaging support");
    script["path"] = getProxyPath();
    script["type"] = QStringLiteral("stdio");

    QJsonArray arr;
    if (browser == SupportedBrowsers::FIREFOX || browser == SupportedBrowsers::TOR_BROWSER
        || (browser == SupportedBrowsers::CUSTOM
            && browserSettings()->customBrowserType() == SupportedBrowsers::FIREFOX)) {
        for (const QString& extension : ALLOWED_EXTENSIONS) {
            arr.append(extension);
        }
        script["allowed_extensions"] = arr;
    } else {
        for (const QString& origin : ALLOWED_ORIGINS) {
            arr.append(origin);
        }
#ifdef QT_DEBUG
        auto customId = browserSettings()->customExtensionId();
        if (!customId.isEmpty()) {
            arr.append(QString("chrome-extension://%1/").arg(customId));
        }
#endif
        script["allowed_origins"] = arr;
    }

    return script;
}

/**
 * Saves a JSON script file
 *
 * @param browser Selected browser
 * @param script JSON native messaging script object
 * @return bool Write succeeds
 */
bool NativeMessageInstaller::createNativeMessageFile(SupportedBrowsers browser)
{
    auto path = getNativeMessagePath(browser);

    // Make the parent directory path if necessary
    QDir().mkpath(QFileInfo(path).absolutePath());

    QFile scriptFile(path);
    if (!scriptFile.open(QIODevice::WriteOnly)) {
        qWarning() << "Browser Plugin: Failed to open native message file for writing at " << scriptFile.fileName();
        qWarning() << scriptFile.errorString();
        return false;
    }

    QJsonDocument doc(constructFile(browser));
    if (scriptFile.write(doc.toJson()) < 0) {
        qWarning() << "Browser Plugin: Failed to write native message file at " << scriptFile.fileName();
        qWarning() << scriptFile.errorString();
        return false;
    }
    return true;
}
