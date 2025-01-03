/*
 *  Copyright (C) 2012 Felix Geyer <debfx@fobos.de>
 *  Copyright (C) 2017 KeePassXC Team <team@keepassxc.org>
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

#include "EditWidgetIcons.h"
#include "ui_EditWidgetIcons.h"

#include "core/Clock.h"
#include "core/Config.h"
#include "core/Database.h"
#include "core/Metadata.h"
#include "core/Tools.h"
#include "gui/FileDialog.h"
#include "gui/IconModels.h"
#include "gui/Icons.h"
#include "gui/MessageBox.h"
#ifdef KPXC_FEATURE_NETWORK
#include "gui/IconDownloader.h"
#endif

#include <QKeyEvent>

IconStruct::IconStruct()
    : uuid(QUuid())
    , number(0)
    , applyTo(ApplyIconToOptions::THIS_ONLY)
{
}

EditWidgetIcons::EditWidgetIcons(QWidget* parent)
    : QWidget(parent)
    , m_ui(new Ui::EditWidgetIcons())
    , m_db(nullptr)
    , m_applyIconTo(ApplyIconToOptions::THIS_ONLY)
    , m_defaultIconModel(new DefaultIconModel(this))
    , m_customIconModel(new CustomIconModel(this))
#ifdef KPXC_FEATURE_NETWORK
    , m_downloader(new IconDownloader())
#endif
{
    m_ui->setupUi(this);

    m_ui->defaultIconsView->setModel(m_defaultIconModel);
    m_ui->customIconsView->setModel(m_customIconModel);

    m_ui->applyIconToPushButton->setMenu(createApplyIconToMenu());

    // clang-format off
    connect(m_ui->defaultIconsView, SIGNAL(clicked(QModelIndex)), this, SLOT(updateRadioButtonDefaultIcons()));
    connect(m_ui->customIconsView, SIGNAL(clicked(QModelIndex)), this, SLOT(updateRadioButtonCustomIcons()));
    connect(m_ui->defaultIconsRadio, SIGNAL(toggled(bool)), this, SLOT(updateWidgetsDefaultIcons(bool)));
    connect(m_ui->customIconsRadio, SIGNAL(toggled(bool)), this, SLOT(updateWidgetsCustomIcons(bool)));
    connect(m_ui->addButton, SIGNAL(clicked()), SLOT(addCustomIconFromFile()));
    connect(m_ui->faviconButton, SIGNAL(clicked()), SLOT(downloadFavicon()));
    connect(m_ui->applyIconToPushButton->menu(), SIGNAL(triggered(QAction*)), SLOT(confirmApplyIconTo(QAction*)));

    connect(m_ui->defaultIconsRadio, SIGNAL(toggled(bool)), this, SIGNAL(widgetUpdated()));
    connect(m_ui->defaultIconsView->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            this, SIGNAL(widgetUpdated()));
    connect(m_ui->customIconsView->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            this, SIGNAL(widgetUpdated()));
#ifdef KPXC_FEATURE_NETWORK
    connect(m_downloader.data(),
            SIGNAL(finished(const QString&, const QImage&)),
            SLOT(iconReceived(const QString&, const QImage&)));
#endif
    // clang-format on

#ifndef KPXC_FEATURE_NETWORK
    m_ui->faviconButton->setVisible(false);
    m_ui->faviconURL->setVisible(false);
#endif
}

EditWidgetIcons::~EditWidgetIcons() = default;

IconStruct EditWidgetIcons::state()
{
    Q_ASSERT(m_db);
    Q_ASSERT(!m_currentUuid.isNull());

    IconStruct iconStruct;
    if (m_ui->defaultIconsRadio->isChecked()) {
        QModelIndex index = m_ui->defaultIconsView->currentIndex();
        if (index.isValid()) {
            iconStruct.number = index.row();
        } else {
            Q_ASSERT(false);
        }
    } else {
        QModelIndex index = m_ui->customIconsView->currentIndex();
        if (index.isValid()) {
            iconStruct.uuid = m_customIconModel->uuidFromIndex(m_ui->customIconsView->currentIndex());
        } else {
            iconStruct.number = -1;
        }
    }

    iconStruct.applyTo = m_applyIconTo;

    return iconStruct;
}

void EditWidgetIcons::reset()
{
    m_db.reset();
    m_currentUuid = QUuid();
}

void EditWidgetIcons::load(const QUuid& currentUuid,
                           const QSharedPointer<Database>& database,
                           const IconStruct& iconStruct,
                           const QString& url)
{
    Q_ASSERT(database);
    Q_ASSERT(!currentUuid.isNull());

    m_db = database;
    m_currentUuid = currentUuid;
    setUrl(url);

    m_customIconModel->setIcons(Icons::customIconsPixmaps(database.data(), IconSize::Default),
                                database->metadata()->customIconsOrder());

    QUuid iconUuid = iconStruct.uuid;
    if (iconUuid.isNull()) {
        int iconNumber = iconStruct.number;
        m_ui->defaultIconsView->setCurrentIndex(m_defaultIconModel->index(iconNumber, 0));
        m_ui->defaultIconsRadio->setChecked(true);
    } else {
        QModelIndex index = m_customIconModel->indexFromUuid(iconUuid);
        if (index.isValid()) {
            m_ui->customIconsView->setCurrentIndex(index);
            m_ui->customIconsRadio->setChecked(true);
        } else {
            m_ui->defaultIconsView->setCurrentIndex(m_defaultIconModel->index(0, 0));
            m_ui->defaultIconsRadio->setChecked(true);
        }
    }

    m_applyIconTo = ApplyIconToOptions::THIS_ONLY;
    m_ui->applyIconToPushButton->menu()->defaultAction()->activate(QAction::Trigger);
}

void EditWidgetIcons::setShowApplyIconToButton(bool state)
{
    m_ui->applyIconToPushButton->setVisible(state);
}

QMenu* EditWidgetIcons::createApplyIconToMenu()
{
    auto* applyIconToMenu = new QMenu(this);
    QAction* defaultAction = applyIconToMenu->addAction(tr("Apply to this group only"));
    defaultAction->setData(QVariant::fromValue(ApplyIconToOptions::THIS_ONLY));
    applyIconToMenu->setDefaultAction(defaultAction);
    applyIconToMenu->addSeparator();
    applyIconToMenu->addAction(tr("Also apply to child groups"))
        ->setData(QVariant::fromValue(ApplyIconToOptions::CHILD_GROUPS));
    applyIconToMenu->addAction(tr("Also apply to child entries"))
        ->setData(QVariant::fromValue(ApplyIconToOptions::CHILD_ENTRIES));
    applyIconToMenu->addAction(tr("Also apply to all children"))
        ->setData(QVariant::fromValue(ApplyIconToOptions::ALL_CHILDREN));
    return applyIconToMenu;
}

void EditWidgetIcons::keyPressEvent(QKeyEvent* event)
{
    if (m_ui->faviconURL->hasFocus() && (event->key() == Qt::Key_Enter || event->key() == Qt::Key_Return)) {
        m_ui->faviconButton->animateClick();
    } else {
        QWidget::keyPressEvent(event);
    }
}

void EditWidgetIcons::setUrl(const QString& url)
{
#ifdef KPXC_FEATURE_NETWORK
    QUrl urlCheck(url);
    if (urlCheck.scheme().startsWith("http")) {
        m_ui->faviconURL->setText(urlCheck.url(QUrl::RemovePath | QUrl::RemoveQuery | QUrl::RemoveFragment));
        m_ui->faviconURL->setCursorPosition(0);
    } else {
        m_ui->faviconURL->setText("");
    }
#else
    Q_UNUSED(url);
#endif
}

void EditWidgetIcons::downloadFavicon()
{
#ifdef KPXC_FEATURE_NETWORK
    auto url = m_ui->faviconURL->text();
    if (!url.isEmpty()) {
        m_downloader->setUrl(url);
        m_downloader->download();
    }
#endif
}

void EditWidgetIcons::iconReceived(const QString& url, const QImage& icon)
{
#ifdef KPXC_FEATURE_NETWORK
    Q_UNUSED(url);
    if (icon.isNull()) {
        QString message(tr("Unable to fetch favicon."));
        if (!config()->get(Config::Security_IconDownloadFallback).toBool()) {
            message.append("\n").append(
                tr("You can enable the DuckDuckGo website icon service under Application Settings -> Security"));
        }
        emit messageEditEntry(message, MessageWidget::Error);
        return;
    }

    if (!addCustomIcon(icon)) {
        emit messageEditEntry(tr("Existing icon selected."), MessageWidget::Information);
    }
#else
    Q_UNUSED(url);
    Q_UNUSED(icon);
#endif
}

void EditWidgetIcons::abortRequests()
{
#ifdef KPXC_FEATURE_NETWORK
    if (m_downloader) {
        m_downloader->abortDownload();
    }
#endif
}

void EditWidgetIcons::addCustomIconFromFile()
{
    if (!m_db) {
        return;
    }

    auto filter = QString("%1 (%2);;%3 (*)").arg(tr("Images"), Icons::imageFormatsFilter(), tr("All files"));
    auto filenames =
        fileDialog()->getOpenFileNames(this, tr("Select Image(s)"), FileDialog::getLastDir("icons"), filter);
    if (!filenames.empty()) {
        QStringList errornames;
        int numexisting = 0;
        for (const auto& filename : filenames) {
            if (!filename.isEmpty()) {
                auto icon = QImage(filename);
                if (icon.isNull()) {
                    errornames << filename;
                } else if (!addCustomIcon(icon, QFileInfo(filename).baseName())) {
                    // Icon already exists in database
                    ++numexisting;
                }
            }
        }

        // Save last used directory using first image path
        FileDialog::saveLastDir("icons", filenames[0]);

        int numloaded = filenames.size() - errornames.size() - numexisting;
        QString msg;

        if (numloaded > 0) {
            msg = tr("Successfully loaded %1 of %n icon(s)", "", filenames.size()).arg(numloaded);
        } else {
            msg = tr("No icons were loaded");
        }

        if (numexisting > 0) {
            msg += "\n" + tr("%n icon(s) already exist in the database", "", numexisting);
        }

        if (!errornames.empty()) {
            // Show the first 8 icons that failed to load
            errornames = errornames.mid(0, 8);
            emit messageEditEntry(msg + "\n" + tr("The following icon(s) failed:", "", errornames.size()) + "\n"
                                      + errornames.join("\n"),
                                  MessageWidget::Error);
        } else if (numloaded > 0) {
            emit messageEditEntry(msg, MessageWidget::Positive);
        } else {
            emit messageEditEntry(msg, MessageWidget::Information);
        }
    }
}

bool EditWidgetIcons::addCustomIcon(const QImage& icon, const QString& name)
{
    bool added = false;
    if (m_db) {
        // Don't add an icon larger than 128x128, but retain original size if smaller
        auto scaledIcon = icon;
        if (icon.width() > 128 || icon.height() > 128) {
            scaledIcon = icon.scaled(128, 128);
        }

        QByteArray serializedIcon = Icons::saveToBytes(scaledIcon);
        QUuid uuid = m_db->metadata()->findCustomIcon(serializedIcon);
        if (uuid.isNull()) {
            uuid = QUuid::createUuid();
            m_db->metadata()->addCustomIcon(uuid, serializedIcon, name, Clock::currentDateTimeUtc());
            m_customIconModel->setIcons(Icons::customIconsPixmaps(m_db.data(), IconSize::Default),
                                        m_db->metadata()->customIconsOrder());
            added = true;
        }

        // Select the new or existing icon
        updateRadioButtonCustomIcons();
        QModelIndex index = m_customIconModel->indexFromUuid(uuid);
        m_ui->customIconsView->setCurrentIndex(index);

        emit widgetUpdated();
    }

    return added;
}

void EditWidgetIcons::updateWidgetsDefaultIcons(bool check)
{
    if (check) {
        QModelIndex index = m_ui->defaultIconsView->currentIndex();
        if (!index.isValid()) {
            m_ui->defaultIconsView->setCurrentIndex(m_defaultIconModel->index(0, 0));
        } else {
            m_ui->defaultIconsView->setCurrentIndex(index);
        }
        m_ui->customIconsView->selectionModel()->clearSelection();
    }
}

void EditWidgetIcons::updateWidgetsCustomIcons(bool check)
{
    if (check) {
        QModelIndex index = m_ui->customIconsView->currentIndex();
        if (!index.isValid()) {
            m_ui->customIconsView->setCurrentIndex(m_customIconModel->index(0, 0));
        } else {
            m_ui->customIconsView->setCurrentIndex(index);
        }
        m_ui->defaultIconsView->selectionModel()->clearSelection();
    }
}

void EditWidgetIcons::updateRadioButtonDefaultIcons()
{
    m_ui->defaultIconsRadio->setChecked(true);
}

void EditWidgetIcons::updateRadioButtonCustomIcons()
{
    m_ui->customIconsRadio->setChecked(true);
}

void EditWidgetIcons::confirmApplyIconTo(QAction* action)
{
    m_applyIconTo = action->data().value<ApplyIconToOptions>();
    m_ui->applyIconToPushButton->setText(action->text());
}
