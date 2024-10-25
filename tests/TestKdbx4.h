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

#ifndef KEEPASSXC_TEST_KDBX4_H
#define KEEPASSXC_TEST_KDBX4_H

#include "TestKeePass2Format.h"

class TestKdbx4Argon2 : public TestKeePass2Format
{
    Q_OBJECT

protected:
    void initTestCaseImpl() override;

    QSharedPointer<Database> readXml(QBuffer* buf, bool strictMode, bool& hasError, QString& errorString) override;
    QSharedPointer<Database>
    readXml(const QString& path, bool strictMode, bool& hasError, QString& errorString) override;
    void writeXml(QBuffer* buf, Database* db, bool& hasError, QString& errorString) override;

    void readKdbx(QIODevice* device,
                  QSharedPointer<const CompositeKey> key,
                  QSharedPointer<Database> db,
                  bool& hasError,
                  QString& errorString) override;
    void writeKdbx(QIODevice* device, Database* db, bool& hasError, QString& errorString) override;
};

class TestKdbx4AesKdf : public TestKdbx4Argon2
{
    Q_OBJECT

protected:
    void initTestCaseImpl() override;
};

/**
 * KDF-independent KDBX4 format tests.
 */
class TestKdbx4Format : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();
    void testFormat400();
    void testFormat400Upgrade();
    void testFormat400Upgrade_data();
    void testFormat410Upgrade();
#ifdef WITH_XC_ZSTD
    void testFormat410ZstdCompressed();
#endif
    void testUpgradeMasterKeyIntegrity();
    void testUpgradeMasterKeyIntegrity_data();
    void testAttachmentIndexStability();
    void testCustomData();
};

#endif // KEEPASSXC_TEST_KDBX4_H
