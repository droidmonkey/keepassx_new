/*
 *  Copyright (C) 2025 KeePassXC Team <team@keepassxc.org>
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

#include "TestBase32.h"
#include "core/Base32.h"

#include <QTest>

QTEST_GUILESS_MAIN(TestBase32)

void TestBase32::testDecode()
{
    // 3 quanta, all upper case + padding
    QByteArray encodedData = "JBSWY3DPEB3W64TMMQXC4LQ=";
    QByteArray data = Base32::decode(encodedData);
    QString expectedData = "Hello world...";
    QVERIFY(!data.isEmpty());
    QCOMPARE(QString(data.toStdString().c_str()), expectedData);
    QVERIFY(data.size() == expectedData.size());

    // 4 quanta, all upper case
    encodedData = "GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ";
    data = Base32::decode(encodedData);
    expectedData = "12345678901234567890";
    QVERIFY(!data.isEmpty());
    QCOMPARE(QString(data.toStdString().c_str()), expectedData);
    QVERIFY(data.size() == expectedData.size());

    // 4 quanta, all lower case
    encodedData = "gezdgnbvgy3tqojqgezdgnbvgy3tqojq";
    data = Base32::decode(encodedData);
    expectedData = "12345678901234567890";
    QVERIFY(!data.isEmpty());
    QCOMPARE(QString(data.toStdString().c_str()), expectedData);
    QVERIFY(data.size() == expectedData.size());

    // 4 quanta, mixed upper and lower case
    encodedData = "Gezdgnbvgy3tQojqgezdGnbvgy3tQojQ";
    data = Base32::decode(encodedData);
    expectedData = "12345678901234567890";
    QVERIFY(!data.isEmpty());
    QCOMPARE(QString(data.toStdString().c_str()), expectedData);
    QVERIFY(data.size() == expectedData.size());

    // 1 pad characters
    encodedData = "ORSXG5A=";
    data = Base32::decode(encodedData);
    expectedData = "test";
    QVERIFY(!data.isEmpty());
    QCOMPARE(QString(data.toStdString().c_str()), expectedData);
    QVERIFY(data.size() == expectedData.size());

    // 3 pad characters
    encodedData = "L5PV6===";
    data = Base32::decode(encodedData);
    expectedData = "___";
    QVERIFY(!data.isEmpty());
    QCOMPARE(QString(data.toStdString().c_str()), expectedData);
    QVERIFY(data.size() == expectedData.size());

    // 4 pad characters
    encodedData = "MZXW6IDCMFZA====";
    data = Base32::decode(encodedData);
    expectedData = "foo bar";
    QVERIFY(!data.isEmpty());
    QCOMPARE(QString(data.toStdString().c_str()), expectedData);
    QVERIFY(data.size() == expectedData.size());

    // six pad characters
    encodedData = "MZXW6YTBOI======";
    data = Base32::decode(encodedData);
    expectedData = "foobar";
    QVERIFY(!data.isEmpty());
    QCOMPARE(QString(data.toStdString().c_str()), expectedData);
    QVERIFY(data.size() == expectedData.size());

    encodedData = "IA======";
    data = Base32::decode(encodedData);
    expectedData = "@";
    QVERIFY(!data.isEmpty());
    QCOMPARE(QString(data.toStdString().c_str()), expectedData);
    QVERIFY(data.size() == expectedData.size());

    // error: illegal character
    encodedData = "1MZXW6YTBOI=====";
    data = Base32::decode(encodedData);
    QVERIFY(data.isEmpty());

    // error: missing pad character
    encodedData = "MZXW6YTBOI=====";
    data = Base32::decode(encodedData);
    QVERIFY(data.isEmpty());

    // RFC 4648 test vectors
    encodedData = "";
    data = Base32::decode(encodedData);
    expectedData = "";
    QVERIFY(data.isEmpty());
    QCOMPARE(QString(data.toStdString().c_str()), expectedData);
    QVERIFY(data.size() == expectedData.size());

    encodedData = "MY======";
    data = Base32::decode(encodedData);
    expectedData = "f";
    QVERIFY(!data.isEmpty());
    QCOMPARE(QString(data.toStdString().c_str()), expectedData);
    QVERIFY(data.size() == expectedData.size());

    encodedData = "MZXQ====";
    data = Base32::decode(encodedData);
    expectedData = "fo";
    QVERIFY(!data.isEmpty());
    QCOMPARE(QString(data.toStdString().c_str()), expectedData);
    QVERIFY(data.size() == expectedData.size());

    encodedData = "MZXW6===";
    data = Base32::decode(encodedData);
    QVERIFY(!data.isEmpty());
    expectedData = "foo";
    QCOMPARE(QString(data.toStdString().c_str()), expectedData);
    QVERIFY(data.size() == expectedData.size());

    encodedData = "MZXW6YQ=";
    data = Base32::decode(encodedData);
    expectedData = "foob";
    QVERIFY(!data.isEmpty());
    QCOMPARE(QString(data.toStdString().c_str()), expectedData);
    QVERIFY(data.size() == expectedData.size());

    encodedData = "MZXW6YTB";
    expectedData = "fooba";
    data = Base32::decode(encodedData);
    QVERIFY(!data.isEmpty());
    QCOMPARE(QString(data.toStdString().c_str()), expectedData);
    QVERIFY(data.size() == expectedData.size());

    encodedData = "MZXW6YTBOI======";
    data = Base32::decode(encodedData);
    expectedData = "foobar";
    QVERIFY(!data.isEmpty());
    QCOMPARE(QString(data.toStdString().c_str()), expectedData);
    QVERIFY(data.size() == expectedData.size());
}

void TestBase32::testEncode()
{
    QByteArray data = "Hello world...";
    QByteArray encodedData = Base32::encode(data);
    QCOMPARE(encodedData, QByteArray("JBSWY3DPEB3W64TMMQXC4LQ="));

    data = "12345678901234567890";
    encodedData = Base32::encode(data);
    QCOMPARE(encodedData, QByteArray("GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ"));

    data = "012345678901234567890";
    encodedData = Base32::encode(data);
    QCOMPARE(encodedData, QByteArray("GAYTEMZUGU3DOOBZGAYTEMZUGU3DOOBZGA======"));

    data = "test";
    encodedData = Base32::encode(data);
    QCOMPARE(encodedData, QByteArray("ORSXG5A="));

    data = "___";
    encodedData = Base32::encode(data);
    QCOMPARE(encodedData, QByteArray("L5PV6==="));

    data = "foo bar";
    encodedData = Base32::encode(data);
    QCOMPARE(encodedData, QByteArray("MZXW6IDCMFZA===="));

    data = "@";
    encodedData = Base32::encode(data);
    QCOMPARE(encodedData, QByteArray("IA======"));

    // RFC 4648 test vectors
    data = "";
    encodedData = Base32::encode(data);
    QCOMPARE(encodedData, QByteArray(""));

    data = "f";
    encodedData = Base32::encode(data);
    QCOMPARE(encodedData, QByteArray("MY======"));

    data = "fo";
    encodedData = Base32::encode(data);
    QCOMPARE(encodedData, QByteArray("MZXQ===="));

    data = "foo";
    encodedData = Base32::encode(data);
    QCOMPARE(encodedData, QByteArray("MZXW6==="));

    data = "foob";
    encodedData = Base32::encode(data);
    QCOMPARE(encodedData, QByteArray("MZXW6YQ="));

    data = "fooba";
    encodedData = Base32::encode(data);
    QCOMPARE(encodedData, QByteArray("MZXW6YTB"));

    data = "foobar";
    encodedData = Base32::encode(data);
    QCOMPARE(encodedData, QByteArray("MZXW6YTBOI======"));
}

void TestBase32::testAddPadding()
{
    // Empty. Invalid, returns input.
    QByteArray data = "";
    QByteArray paddedData = Base32::addPadding(data);
    QCOMPARE(paddedData, data);

    // One byte of encoded data. Invalid, returns input.
    data = "B";
    paddedData = Base32::addPadding(data);
    QCOMPARE(paddedData, data);

    // Two bytes of encoded data.
    data = "BB";
    paddedData = Base32::addPadding(data);
    QCOMPARE(paddedData, QByteArray("BB======"));

    // Three bytes of encoded data. Invalid, returns input.
    data = "BBB";
    paddedData = Base32::addPadding(data);
    QCOMPARE(paddedData, data);

    // Four bytes of encoded data.
    data = "BBBB";
    paddedData = Base32::addPadding(data);
    QCOMPARE(paddedData, QByteArray("BBBB===="));

    // Five bytes of encoded data.
    data = "BBBBB";
    paddedData = Base32::addPadding(data);
    QCOMPARE(paddedData, QByteArray("BBBBB==="));

    // Six bytes of encoded data. Invalid, returns input.
    data = "BBBBBB";
    paddedData = Base32::addPadding(data);
    QCOMPARE(paddedData, data);

    // Seven bytes of encoded data.
    data = "BBBBBBB";
    paddedData = Base32::addPadding(data);
    QCOMPARE(paddedData, QByteArray("BBBBBBB="));

    // Eight bytes of encoded data. Valid, but returns same as input.
    data = "BBBBBBBB";
    paddedData = Base32::addPadding(data);
    QCOMPARE(paddedData, data);

    // More than eight bytes (8+5).
    data = "AAAAAAAABBBBB";
    paddedData = Base32::addPadding(data);
    QCOMPARE(paddedData, QByteArray("AAAAAAAABBBBB==="));
}

void TestBase32::testRemovePadding()
{
    QByteArray data = "";
    QByteArray unpaddedData = Base32::removePadding(data);
    QCOMPARE(unpaddedData, data);

    data = "AAAAAAAABB======";
    unpaddedData = Base32::removePadding(data);
    QCOMPARE(unpaddedData, QByteArray("AAAAAAAABB"));

    data = "BBBB====";
    unpaddedData = Base32::removePadding(data);
    QCOMPARE(unpaddedData, QByteArray("BBBB"));

    data = "AAAAAAAABBBBB===";
    unpaddedData = Base32::removePadding(data);
    QCOMPARE(unpaddedData, QByteArray("AAAAAAAABBBBB"));

    data = "BBBBBBB=";
    unpaddedData = Base32::removePadding(data);
    QCOMPARE(unpaddedData, QByteArray("BBBBBBB"));

    // Invalid: 7 bytes of data. Returns same as input.
    data = "IIIIIII";
    unpaddedData = Base32::removePadding(data);
    QCOMPARE(unpaddedData, data);

    // Invalid: more padding than necessary. Returns same as input.
    data = "AAAAAAAABBBB=====";
    unpaddedData = Base32::removePadding(data);
    QCOMPARE(unpaddedData, data);
}

void TestBase32::testSanitizeInput()
{
    // sanitize input (white space + missing padding)
    QByteArray encodedData = "JBSW Y3DP EB3W 64TM MQXC 4LQA";
    auto data = Base32::decode(Base32::sanitizeInput(encodedData));
    QVERIFY(!data.isEmpty());
    QCOMPARE(QString(data.toStdString().c_str()), QString("Hello world..."));

    // sanitize input (typo + missing padding)
    encodedData = "J8SWY3DPE83W64TMMQXC4LQA";
    data = Base32::decode(Base32::sanitizeInput(encodedData));
    QVERIFY(!data.isEmpty());
    QCOMPARE(QString(data.toStdString().c_str()), QString("Hello world..."));

    // sanitize input (other illegal characters)
    encodedData = "J8SWY3D[PE83W64TMMQ]XC!4LQA";
    data = Base32::decode(Base32::sanitizeInput(encodedData));
    QVERIFY(!data.isEmpty());
    QCOMPARE(QString(data.toStdString().c_str()), QString("Hello world..."));

    // sanitize input (NUL character)
    encodedData = "J8SWY3DPE83W64TMMQXC4LQA";
    encodedData.insert(3, '\0');
    data = Base32::decode(Base32::sanitizeInput(encodedData));
    QVERIFY(!data.isEmpty());
    QCOMPARE(QString(data.toStdString().c_str()), QString("Hello world..."));
}
