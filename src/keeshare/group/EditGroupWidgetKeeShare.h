/*
 *  Copyright (C) 2018 KeePassXC Team <team@keepassxc.org>
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

#ifndef KEEPASSXC_EDITGROUPWIDGETKEESHARE_H
#define KEEPASSXC_EDITGROUPWIDGETKEESHARE_H

#include <QPointer>
#include <QWidget>

class Group;
class Database;

namespace Ui
{
    class EditGroupWidgetKeeShare;
}

class EditGroupWidgetKeeShare : public QWidget
{
    Q_OBJECT
public:
    explicit EditGroupWidgetKeeShare(QWidget* parent = nullptr);
    ~EditGroupWidgetKeeShare();

    void setGroup(Group* temporaryGroup, QSharedPointer<Database> database);

private slots:
    void updateSharingState();

private slots:
    void update();
    void clearInputs();
    void selectType();
    void selectPassword();
    void launchPathSelectionDialog();
    void selectPath();
    void recurseIntoGroupsToggled(bool);

private:
    QScopedPointer<Ui::EditGroupWidgetKeeShare> m_ui;
    QPointer<Group> m_temporaryGroup;
    QSharedPointer<Database> m_database;
};

#endif // KEEPASSXC_EDITGROUPWIDGETKEESHARE_H
