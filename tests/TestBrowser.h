/*
 *  Copyright (C) 2023 KeePassXC Team <team@keepassxc.org>
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

#ifndef KEEPASSXC_TESTBROWSER_H
#define KEEPASSXC_TESTBROWSER_H

#include <QObject>

#include "browser/BrowserAction.h"
#include "browser/BrowserService.h"

class TestBrowser : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void init();

    void testChangePublicKeys();
    void testEncryptMessage();
    void testDecryptMessage();
    void testGetBase64FromKey();
    void testIncrementNonce();
    void testBuildResponse();
    void testSortPriority();
    void testSortPriority_data();
    void testSearchEntries();
    void testSearchEntriesByPath();
    void testSearchEntriesByUUID();
    void testSearchEntriesByReference();
    void testSearchEntriesWithPort();
    void testSearchEntriesWithAdditionalURLs();
    void testSearchEntriesWithWildcardURLs();
    void testInvalidEntries();
    void testSubdomainsAndPaths();
    void testBestMatchingCredentials();
    void testBestMatchingWithAdditionalURLs();
    void testRestrictBrowserKey();

private:
    QList<Entry*> createEntries(QStringList& urls, Group* root) const;
    void compareEntriesByPath(QSharedPointer<Database> db, QList<Entry*> entries, QString path);

    QScopedPointer<BrowserAction> m_browserAction;
    QPointer<BrowserService> m_browserService;
};
#endif // KEEPASSXC_TESTBROWSER_H
