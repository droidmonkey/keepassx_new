/*
 *  Copyright (C) 2020 KeePassXC Team <team@keepassxc.org>
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

#include "BrowserSettingsWidget.h"
#include "ui_BrowserSettingsWidget.h"

#include "BrowserSettings.h"
#include "config-keepassx.h"
#include "gui/styles/StateColorPalette.h"

#include <QFileDialog>

BrowserSettingsWidget::BrowserSettingsWidget(QWidget* parent)
    : QWidget(parent)
    , m_ui(new Ui::BrowserSettingsWidget())
{
    m_ui->setupUi(this);

    // clang-format off
    m_ui->extensionLabel->setOpenExternalLinks(true);
    m_ui->extensionLabel->setText(
        tr("KeePassXC-Browser is needed for the browser integration to work. <br />Download it for %1 and %2 and %3.")
            .arg("<a href=\"https://addons.mozilla.org/firefox/addon/keepassxc-browser/\">Firefox</a>",
                 "<a href=\"https://chromewebstore.google.com/detail/keepassxc-browser/oboonakemofpalcgghocfoadofidjkkk\">"
                 "Google Chrome / Chromium / Vivaldi / Brave</a>",
                 "<a href=\"https://microsoftedge.microsoft.com/addons/detail/pdffhmdngciaglkoonimfcmckehcpafo\">Microsoft Edge</a>"));
    // clang-format on

    m_ui->tabWidget->setEnabled(m_ui->enableBrowserSupport->isChecked());
    connect(m_ui->enableBrowserSupport, SIGNAL(toggled(bool)), m_ui->tabWidget, SLOT(setEnabled(bool)));
    connect(m_ui->enableBrowserSupport, SIGNAL(toggled(bool)), SLOT(validateProxyLocation()));

    // Custom Browser option
#ifdef Q_OS_WIN
    // TODO: Custom browser is disabled on Windows
    m_ui->customBrowserSupport->setVisible(false);
    m_ui->customBrowserGroupBox->setVisible(false);
#else
    connect(m_ui->customBrowserLocationBrowseButton, SIGNAL(clicked()), SLOT(showCustomBrowserLocationFileDialog()));
    connect(m_ui->customBrowserSupport, SIGNAL(toggled(bool)), m_ui->customBrowserGroupBox, SLOT(setEnabled(bool)));
#endif

    // Custom Proxy option
    m_ui->customProxyLocation->setEnabled(m_ui->useCustomProxy->isChecked());
    m_ui->customProxyLocationBrowseButton->setEnabled(m_ui->useCustomProxy->isChecked());

    connect(m_ui->useCustomProxy, SIGNAL(toggled(bool)), m_ui->customProxyLocation, SLOT(setEnabled(bool)));
    connect(m_ui->useCustomProxy, SIGNAL(toggled(bool)), m_ui->customProxyLocationBrowseButton, SLOT(setEnabled(bool)));
    connect(m_ui->useCustomProxy, SIGNAL(toggled(bool)), SLOT(validateProxyLocation()));
    connect(m_ui->customProxyLocation, SIGNAL(editingFinished()), SLOT(validateProxyLocation()));
    connect(m_ui->customProxyLocationBrowseButton, SIGNAL(clicked()), this, SLOT(showProxyLocationFileDialog()));

    m_ui->messageWidget->setVisible(false);
    m_ui->messageWidget->setCloseButtonVisible(false);
    m_ui->messageWidget->setWordWrap(true);
    m_ui->messageWidget->setAutoHideTimeout(MessageWidget::DisableAutoHide);

#ifndef Q_OS_LINUX
    m_ui->snapWarningLabel->setVisible(false);
#endif

#ifdef Q_OS_WIN
    // Brave uses Chrome's registry settings
    m_ui->braveSupport->setHidden(true);
    // Vivaldi uses Chrome's registry settings
    m_ui->vivaldiSupport->setHidden(true);
    m_ui->chromeSupport->setText("Chrome, Vivaldi, and Brave");
    // Tor Browser uses Firefox's registry settings
    m_ui->torBrowserSupport->setHidden(true);
    m_ui->firefoxSupport->setText("Firefox and Tor Browser");
#endif

#ifndef QT_DEBUG
    m_ui->customExtensionId->setVisible(false);
    m_ui->customExtensionLabel->setVisible(false);
#endif
}

BrowserSettingsWidget::~BrowserSettingsWidget()
{
}

void BrowserSettingsWidget::loadSettings()
{
    auto settings = browserSettings();
    m_ui->enableBrowserSupport->setChecked(settings->isEnabled());

    m_ui->showNotification->setChecked(settings->showNotification());
    m_ui->bestMatchOnly->setChecked(settings->bestMatchOnly());
    m_ui->unlockDatabase->setChecked(settings->unlockDatabase());
    m_ui->matchUrlScheme->setChecked(settings->matchUrlScheme());

    // hide unimplemented options
    // TODO: fix this
    m_ui->showNotification->hide();

    m_ui->alwaysAllowAccess->setChecked(settings->alwaysAllowAccess());
    m_ui->alwaysAllowUpdate->setChecked(settings->alwaysAllowUpdate());
    m_ui->httpAuthPermission->setChecked(settings->httpAuthPermission());
    m_ui->searchInAllDatabases->setChecked(settings->searchInAllDatabases());
    m_ui->supportKphFields->setChecked(settings->supportKphFields());
    m_ui->allowLocalhostWithPasskeys->setChecked(settings->allowLocalhostWithPasskeys());
    m_ui->noMigrationPrompt->setChecked(settings->noMigrationPrompt());
    m_ui->useCustomProxy->setChecked(settings->useCustomProxy());
    m_ui->customProxyLocation->setText(settings->replaceHomePath(settings->customProxyLocation()));
    m_ui->updateBinaryPath->setChecked(settings->updateBinaryPath());
    m_ui->allowGetDatabaseEntriesRequest->setChecked(settings->allowGetDatabaseEntriesRequest());
    m_ui->allowExpiredCredentials->setChecked(settings->allowExpiredCredentials());
    m_ui->chromeSupport->setChecked(settings->browserSupport(BrowserShared::CHROME));
    m_ui->chromiumSupport->setChecked(settings->browserSupport(BrowserShared::CHROMIUM));
    m_ui->firefoxSupport->setChecked(settings->browserSupport(BrowserShared::FIREFOX));
    m_ui->edgeSupport->setChecked(settings->browserSupport(BrowserShared::EDGE));
#ifndef Q_OS_WIN
    m_ui->braveSupport->setChecked(settings->browserSupport(BrowserShared::BRAVE));
    m_ui->vivaldiSupport->setChecked(settings->browserSupport(BrowserShared::VIVALDI));
    m_ui->torBrowserSupport->setChecked(settings->browserSupport(BrowserShared::TOR_BROWSER));
#endif
#ifndef Q_OS_LINUX
    m_ui->snapWarningLabel->setVisible(false);
#endif

#ifdef KEEPASSXC_DIST_SNAP
    // Disable settings that will not work
    m_ui->useCustomProxy->setChecked(false);
    m_ui->useCustomProxy->setVisible(false);
    m_ui->customProxyLocation->setVisible(false);
    m_ui->customProxyLocationBrowseButton->setVisible(false);
    m_ui->updateBinaryPath->setChecked(false);
    m_ui->updateBinaryPath->setVisible(false);
    // No custom browser for snaps
    m_ui->customBrowserSupport->setVisible(false);
    m_ui->customBrowserGroupBox->setVisible(false);
#endif
#ifdef KEEPASSXC_DIST_FLATPAK
    // The sandbox makes custom proxy locations very unintuitive
    m_ui->useCustomProxy->setChecked(false);
    m_ui->useCustomProxy->setEnabled(false);
    m_ui->useCustomProxy->setVisible(false);
    m_ui->customProxyLocation->setVisible(false);
    // Won't work with xdg portals and executables that must be browser accessible
    m_ui->customProxyLocationBrowseButton->setVisible(false);
#endif

    const auto customBrowserSet = settings->customBrowserSupport();
    m_ui->customBrowserSupport->setChecked(customBrowserSet);
    m_ui->customBrowserGroupBox->setEnabled(customBrowserSet);
    m_ui->browserTypeComboBox->clear();
    m_ui->browserTypeComboBox->addItem(tr("Firefox"), BrowserShared::SupportedBrowsers::FIREFOX);
    m_ui->browserTypeComboBox->addItem(tr("Chromium"), BrowserShared::SupportedBrowsers::CHROMIUM);
    auto typeIndex = m_ui->browserTypeComboBox->findData(settings->customBrowserType());
    if (typeIndex >= 0) {
        m_ui->browserTypeComboBox->setCurrentIndex(typeIndex);
    }
    m_ui->customBrowserLocation->setText(settings->replaceHomePath(settings->customBrowserLocation()));

#ifdef QT_DEBUG
    m_ui->customExtensionId->setText(settings->customExtensionId());
#endif
    // Validate the complete proxy location dependency - not only in case it is custom,
    // to make trouble-shooting for both developer and user easier
    validateProxyLocation();
}

QString BrowserSettingsWidget::resolveCustomProxyLocation()
{
    auto settings = browserSettings();
    auto proxyLocation = m_ui->customProxyLocation->text().trimmed();
    proxyLocation = settings->replaceTildeHomePath(proxyLocation);
    return proxyLocation;
}

void BrowserSettingsWidget::validateProxyLocation()
{
#if !defined(KEEPASSXC_DIST_SNAP) && !defined(KEEPASSXC_DIST_FLATPAK)
    // Reset the UI
    m_ui->messageWidget->setVisible(false);
    m_ui->customProxyLocation->setStyleSheet("");
    m_ui->customProxyLocation->setToolTip("");

    if (m_ui->enableBrowserSupport->isChecked()) {
        // If we are using a custom proxy location, check if it exists and display warning if not
        if (m_ui->useCustomProxy->isChecked()) {
            if (!QFile::exists(resolveCustomProxyLocation())) {
                StateColorPalette statePalette;
                auto color = statePalette.color(StateColorPalette::ColorRole::Error);
                m_ui->customProxyLocation->setStyleSheet(QString("QLineEdit { background: %1; }").arg(color.name()));
                m_ui->customProxyLocation->setToolTip(tr("The custom proxy location does not exist."));

                m_ui->messageWidget->showMessage(tr("<b>Error:</b> The custom proxy location does not exist. Correct "
                                                    "this in the advanced settings tab."),
                                                 MessageWidget::Error);
            }
        } else {
            // Otherwise check if the installed proxy exists
            auto expectedProxyLocation = browserSettings()->proxyLocationAsInstalled();
            if (!QFile::exists(expectedProxyLocation)) {
                m_ui->messageWidget->showMessage(
                    tr("<b>Error:</b> The installed proxy executable is missing from the expected location: %1<br/>"
                       "Please set a custom proxy location in the advanced settings or reinstall the application.")
                        .arg(expectedProxyLocation),
                    MessageWidget::Error);
            }
        }
    }
#endif
}

void BrowserSettingsWidget::saveSettings()
{
    auto settings = browserSettings();
    settings->setEnabled(m_ui->enableBrowserSupport->isChecked());
    settings->setShowNotification(m_ui->showNotification->isChecked());
    settings->setBestMatchOnly(m_ui->bestMatchOnly->isChecked());
    settings->setUnlockDatabase(m_ui->unlockDatabase->isChecked());
    settings->setMatchUrlScheme(m_ui->matchUrlScheme->isChecked());

    settings->setUseCustomProxy(m_ui->useCustomProxy->isChecked());
    settings->setCustomProxyLocation(resolveCustomProxyLocation());

    settings->setUpdateBinaryPath(m_ui->updateBinaryPath->isChecked());
    settings->setAllowGetDatabaseEntriesRequest(m_ui->allowGetDatabaseEntriesRequest->isChecked());
    settings->setAllowExpiredCredentials(m_ui->allowExpiredCredentials->isChecked());
    settings->setAlwaysAllowAccess(m_ui->alwaysAllowAccess->isChecked());
    settings->setAlwaysAllowUpdate(m_ui->alwaysAllowUpdate->isChecked());
    settings->setHttpAuthPermission(m_ui->httpAuthPermission->isChecked());
    settings->setSearchInAllDatabases(m_ui->searchInAllDatabases->isChecked());
    settings->setSupportKphFields(m_ui->supportKphFields->isChecked());
    settings->setAllowLocalhostWithPasskeys(m_ui->allowLocalhostWithPasskeys->isChecked());
    settings->setNoMigrationPrompt(m_ui->noMigrationPrompt->isChecked());

#ifdef QT_DEBUG
    settings->setCustomExtensionId(m_ui->customExtensionId->text());
#endif

    settings->setBrowserSupport(BrowserShared::CHROME, m_ui->chromeSupport->isChecked());
    settings->setBrowserSupport(BrowserShared::CHROMIUM, m_ui->chromiumSupport->isChecked());
    settings->setBrowserSupport(BrowserShared::FIREFOX, m_ui->firefoxSupport->isChecked());
    settings->setBrowserSupport(BrowserShared::EDGE, m_ui->edgeSupport->isChecked());
#ifndef Q_OS_WIN
    settings->setBrowserSupport(BrowserShared::BRAVE, m_ui->braveSupport->isChecked());
    settings->setBrowserSupport(BrowserShared::VIVALDI, m_ui->vivaldiSupport->isChecked());
    settings->setBrowserSupport(BrowserShared::TOR_BROWSER, m_ui->torBrowserSupport->isChecked());

    // Custom browser settings
    auto customBrowserEnabled = m_ui->customBrowserSupport->isChecked();
    settings->setCustomBrowserType(m_ui->browserTypeComboBox->currentData().toInt());
    settings->setCustomBrowserLocation(
        customBrowserEnabled ? browserSettings()->replaceTildeHomePath(m_ui->customBrowserLocation->text()) : "");
    settings->setCustomBrowserSupport(customBrowserEnabled);
    settings->setBrowserSupport(BrowserShared::CUSTOM, customBrowserEnabled);
#endif
}

void BrowserSettingsWidget::showProxyLocationFileDialog()
{
#ifdef Q_OS_WIN
    QString fileTypeFilter(QString("%1 (*.exe);;%2 (*.*)").arg(tr("Executable Files"), tr("All Files")));
#else
    QString fileTypeFilter(QString("%1 (*)").arg(tr("Executable Files")));
#endif

    auto initialFilePath = resolveCustomProxyLocation();
    if (QFileInfo::exists(initialFilePath)) {
        initialFilePath = QFileInfo(initialFilePath).filePath();
    } else {
        // ignore current status and set as it would be installed
        initialFilePath = QFileInfo(browserSettings()->proxyLocationAsInstalled()).filePath();
    }

    QString proxyLocation =
        QFileDialog::getOpenFileName(this, tr("Select custom proxy location"), initialFilePath, fileTypeFilter);

    if (!proxyLocation.isEmpty()) {
        proxyLocation = browserSettings()->replaceHomePath(proxyLocation);
        m_ui->customProxyLocation->setText(proxyLocation);
        validateProxyLocation();
    } else {
        // do not overwrite old proxy setting
    }
}

void BrowserSettingsWidget::showCustomBrowserLocationFileDialog()
{
    auto location = QFileDialog::getExistingDirectory(this,
                                                      tr("Select native messaging host folder location"),
                                                      QFileInfo(QCoreApplication::applicationDirPath()).filePath());

    location = browserSettings()->replaceHomePath(location);
    if (!location.isEmpty()) {
        m_ui->customBrowserLocation->setText(location);
    }
}
