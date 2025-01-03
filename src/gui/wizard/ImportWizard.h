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

#ifndef KEEPASSXC_IMPORTWIZARD_H
#define KEEPASSXC_IMPORTWIZARD_H

#include <QPointer>
#include <QWizard>

class Database;
class ImportWizardPageSelect;
class ImportWizardPageReview;

/**
 * Setup wizard for importing a file into a database.
 */
class ImportWizard : public QWizard
{
    Q_OBJECT

public:
    explicit ImportWizard(QWidget* parent = nullptr);
    ~ImportWizard() override;

    bool validateCurrentPage() override;

    QSharedPointer<Database> database();
    QPair<QUuid, QUuid> importInto();

    enum ImportType
    {
        IMPORT_NONE = 0,
        IMPORT_CSV,
        IMPORT_OPVAULT,
        IMPORT_OPUX,
        IMPORT_BITWARDEN,
        IMPORT_PROTONPASS,
        IMPORT_KEEPASS1
    };

private:
    QSharedPointer<Database> m_db;
    QPointer<ImportWizardPageSelect> m_pageSelect;
    QPointer<ImportWizardPageReview> m_pageReview;
};

#endif // KEEPASSXC_IMPORTWIZARD_H
