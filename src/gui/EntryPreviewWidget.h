/*
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

#ifndef KEEPASSX_DETAILSWIDGET_H
#define KEEPASSX_DETAILSWIDGET_H

#include "config-keepassx.h"
#include "gui/DatabaseWidget.h"

namespace Ui
{
    class EntryPreviewWidget;
}

class QTabWidget;
class QTextEdit;

class EntryPreviewWidget : public QWidget
{
    Q_OBJECT

public:
    explicit EntryPreviewWidget(QWidget* parent = nullptr);
    ~EntryPreviewWidget() override;

public slots:
    void setEntry(Entry* selectedEntry);
    void setGroup(Group* selectedGroup);
    void setDatabaseMode(DatabaseWidget::Mode mode);
    void refresh();
    void clear();

signals:
    void entryUrlActivated(Entry* entry);
    void copyTextRequested(const QString& text);

protected:
    bool eventFilter(QObject* object, QEvent* event) override;

private slots:
    void updateEntryHeaderLine();
    void updateEntryTotp();
    void updateEntryGeneralTab();
    void updateEntryAdvancedTab();
    void updateEntryAutotypeTab();
    void setUsernameVisible(bool state);
    void setPasswordVisible(bool state);
    void setEntryNotesVisible(bool state);
    void setGroupNotesVisible(bool state);
    void setNotesVisible(QTextEdit* notesWidget, const QString& notes, bool state);

    void updateGroupHeaderLine();
    void updateGroupGeneralTab();
    void updateGroupSharingTab();

    void updateTotpLabel();
    void updateTabIndexes();
    void openEntryUrl();

private:
    void removeTab(QTabWidget* tabWidget, QWidget* widget);
    void setTabEnabled(QTabWidget* tabWidget, QWidget* widget, bool enabled);

    static QString hierarchy(const Group* group, const QString& title);

    const QScopedPointer<Ui::EntryPreviewWidget> m_ui;
    bool m_locked;
    QPointer<Entry> m_currentEntry;
    QPointer<Group> m_currentGroup;
    QTimer m_totpTimer;
    quint8 m_selectedTabEntry;
    quint8 m_selectedTabGroup;
};

#endif // KEEPASSX_DETAILSWIDGET_H
