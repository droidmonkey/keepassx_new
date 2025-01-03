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

#ifndef KEEPASSXC_TEST_AUTHENTICATIONFACTORPARSING_H
#define KEEPASSXC_TEST_AUTHENTICATIONFACTORPARSING_H

#include <QObject>

#include "format/KdbxXmlAuthenticationFactorReader.h"

/**
 * Tests of the parsing of different authentication-factor information headers.
 */
class TestAuthenticationFactorParsing : public QObject
{
    Q_OBJECT

private slots:
    void testNotXML();
    void testMalformedXML();
    void testMissingFactorInfo();
    void testNoCompatVersion();
    void testUnsupportedCompatVersion();
    void testNoGroups();
    void testGroupWithNoFactors();
    void testUnsupportedFactorTypeAlone();
    void testUnsupportedFactorTypeAndSupportedTogether();
    void testUnsupportedVerificationMethod();
    void testOmittedVerification();
    void testInvalidBase64();
    void testMissingRequiredFields();

private:
    KdbxXmlAuthenticationFactorReader m_reader;
};

#endif // KEEPASSXC_TEST_AUTHENTICATIONFACTORPARSING_H
