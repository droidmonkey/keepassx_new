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

#ifndef KEEPASSX_EDITWIDGETICONS_H
#define KEEPASSX_EDITWIDGETICONS_H

#include <QMenu>
#include <QUuid>

#include "config-keepassx.h"
#include "gui/MessageWidget.h"

class Database;
class DefaultIconModel;
class CustomIconModel;
#ifdef KPXC_FEATURE_NETWORK
class IconDownloader;
#endif

namespace Ui
{
    class EditWidgetIcons;
}

enum ApplyIconToOptions
{
    THIS_ONLY = 0b00,
    CHILD_GROUPS = 0b10,
    CHILD_ENTRIES = 0b01,
    ALL_CHILDREN = 0b11
};

Q_DECLARE_METATYPE(ApplyIconToOptions)

struct IconStruct
{
    IconStruct();

    QUuid uuid;
    int number;
    ApplyIconToOptions applyTo;
};

class EditWidgetIcons : public QWidget
{
    Q_OBJECT

public:
    explicit EditWidgetIcons(QWidget* parent = nullptr);
    ~EditWidgetIcons() override;

    IconStruct state();
    void reset();
    void load(const QUuid& currentUuid,
              const QSharedPointer<Database>& database,
              const IconStruct& iconStruct,
              const QString& url = "");
    void setShowApplyIconToButton(bool state);

protected:
    void keyPressEvent(QKeyEvent* event) override;

public slots:
    void setUrl(const QString& url);
    void abortRequests();

signals:
    void messageEditEntry(QString, MessageWidget::MessageType);
    void messageEditEntryDismiss();
    void widgetUpdated();

private slots:
    void downloadFavicon();
    void iconReceived(const QString& url, const QImage& icon);
    void addCustomIconFromFile();
    bool addCustomIcon(const QImage& icon, const QString& name = {});
    void updateWidgetsDefaultIcons(bool checked);
    void updateWidgetsCustomIcons(bool checked);
    void updateRadioButtonDefaultIcons();
    void updateRadioButtonCustomIcons();
    void confirmApplyIconTo(QAction* action);

private:
    QMenu* createApplyIconToMenu();

    const QScopedPointer<Ui::EditWidgetIcons> m_ui;
    QSharedPointer<Database> m_db;
    QUuid m_currentUuid;
    ApplyIconToOptions m_applyIconTo;
    DefaultIconModel* const m_defaultIconModel;
    CustomIconModel* const m_customIconModel;
#ifdef KPXC_FEATURE_NETWORK
    QSharedPointer<IconDownloader> m_downloader;
#endif

    Q_DISABLE_COPY(EditWidgetIcons)
};

#endif // KEEPASSX_EDITWIDGETICONS_H
