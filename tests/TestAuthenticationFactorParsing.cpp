/*
 *  Copyright (C) 2024 KeePassXC Team <team@keepassxc.org>
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

#include "TestAuthenticationFactorParsing.h"

#include <QTest>

QTEST_GUILESS_MAIN(TestAuthenticationFactorParsing)

void TestAuthenticationFactorParsing::testNotXML()
{
    m_reader.readAuthenticationFactors(nullptr, "blargh");

    QVERIFY(m_reader.hasError());
}

void TestAuthenticationFactorParsing::testMalformedXML()
{
    m_reader.readAuthenticationFactors(nullptr, "<FactorInfo><blah></FactorInfo>");

    QVERIFY(m_reader.hasError());
}

void TestAuthenticationFactorParsing::testMissingFactorInfo()
{
    m_reader.readAuthenticationFactors(nullptr, "<Something></Something>");

    QVERIFY(m_reader.hasError());
}

void TestAuthenticationFactorParsing::testNoCompatVersion()
{
    m_reader.readAuthenticationFactors(nullptr, "<FactorInfo></FactorInfo>");

    QVERIFY(m_reader.hasError());
}

void TestAuthenticationFactorParsing::testUnsupportedCompatVersion()
{
    m_reader.readAuthenticationFactors(nullptr, "<FactorInfo><CompatVersion>2</CompatVersion></FactorInfo>");

    QVERIFY(m_reader.hasError());
}

void TestAuthenticationFactorParsing::testNoGroups()
{
    auto res = m_reader.readAuthenticationFactors(nullptr, "<FactorInfo><CompatVersion>1</CompatVersion></FactorInfo>");

    QVERIFY(!m_reader.hasError());
    QCOMPARE(res->getGroups().size(), 0);
}

void TestAuthenticationFactorParsing::testGroupWithNoFactors()
{
    m_reader.readAuthenticationFactors(nullptr,
                                       "<FactorInfo><CompatVersion>1</CompatVersion><Group></Group></FactorInfo>");

    QVERIFY(m_reader.hasError());
}

void TestAuthenticationFactorParsing::testUnsupportedFactorTypeAlone()
{
    m_reader.readAuthenticationFactors(nullptr,
                                       "<FactorInfo><CompatVersion>1</CompatVersion><Group>"
                                       "<Factor><KeyType>AES-CBC</KeyType><TypeUUID>bogus</TypeUUID>"
                                       "<WrappedKey>B4pHAoQomD8728UKeST2HOxglrjzwyq2M/IPEOV4xo8=</WrappedKey></Factor>"
                                       "</Group></FactorInfo>");
    QVERIFY(m_reader.hasError());
}

void TestAuthenticationFactorParsing::testUnsupportedFactorTypeAndSupportedTogether()
{
    auto res = m_reader.readAuthenticationFactors(
        nullptr,
        "<FactorInfo><CompatVersion>1</CompatVersion><Group>"
        "<Factor><KeyType>AES-CBC</KeyType><TypeUUID>bogus</TypeUUID>"
        "<WrappedKey>B4pHAoQomD8728UKeST2HOxglrjzwyq2M/IPEOV4xo8=</WrappedKey></Factor>"
        "<Factor><KeyType>AES-CBC</KeyType><TypeUUID>" FACTOR_TYPE_PASSWORD_SHA256 "</TypeUUID>"
        "<WrappedKey>B4pHAoQomD8728UKeST2HOxglrjzwyq2M/IPEOV4xo8=</WrappedKey></Factor>"
        "</Group></FactorInfo>");
    QVERIFY(!m_reader.hasError());
    QCOMPARE(res->getGroups().first()->getFactors().size(), 2);
}

void TestAuthenticationFactorParsing::testUnsupportedVerificationMethod()
{
    auto res = m_reader.readAuthenticationFactors(
        nullptr,
        "<FactorInfo><CompatVersion>1</CompatVersion><Group>"
        "<ValidationType>bogus</ValidationType>"
        "<Factor><KeyType>AES-CBC</KeyType><TypeUUID>" FACTOR_TYPE_PASSWORD_SHA256 "</TypeUUID>"
        "<WrappedKey>B4pHAoQomD8728UKeST2HOxglrjzwyq2M/IPEOV4xo8=</WrappedKey></Factor>"
        "</Group></FactorInfo>");
    QVERIFY(!m_reader.hasError());
    QCOMPARE(res->getGroups().first()->getValidationType(), AuthenticationFactorGroupValidationType::NONE);
}

void TestAuthenticationFactorParsing::testOmittedVerification()
{
    m_reader.readAuthenticationFactors(nullptr,
                                       "<FactorInfo><CompatVersion>1</CompatVersion><Group>"
                                       "<Factor><KeyType>AES-CBC</KeyType><TypeUUID>" FACTOR_TYPE_PASSWORD_SHA256
                                       "</TypeUUID>"
                                       "<WrappedKey>B4pHAoQomD8728UKeST2HOxglrjzwyq2M/IPEOV4xo8=</WrappedKey></Factor>"
                                       "</Group></FactorInfo>");
    QVERIFY(!m_reader.hasError());
}

void TestAuthenticationFactorParsing::testInvalidBase64()
{
    m_reader.readAuthenticationFactors(nullptr,
                                       "<FactorInfo><CompatVersion>1</CompatVersion><Group>"
                                       "<Factor><KeyType>AES-CBC</KeyType><TypeUUID>" FACTOR_TYPE_PASSWORD_SHA256
                                       "</TypeUUID>"
                                       "<WrappedKey>_</WrappedKey></Factor>"
                                       "</Group></FactorInfo>");
    QVERIFY(m_reader.hasError());
}

void TestAuthenticationFactorParsing::testMissingRequiredFields()
{
    m_reader.readAuthenticationFactors(nullptr,
                                       "<FactorInfo><CompatVersion>1</CompatVersion><Group>"
                                       "<Factor><TypeUUID>" FACTOR_TYPE_PASSWORD_SHA256 "</TypeUUID>"
                                       "<WrappedKey>B4pHAoQomD8728UKeST2HOxglrjzwyq2M/IPEOV4xo8=</WrappedKey></Factor>"
                                       "</Group></FactorInfo>");
    QVERIFY(m_reader.hasError());
}
