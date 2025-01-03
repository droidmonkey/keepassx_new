/*
 * Copyright (C) 2024 KeePassXC Team <team@keepassxc.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 or (at your option)
 * version 3 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef KEEPASSXC_KDBXXMLAUTHENTICATIONFACTORREADER_H
#define KEEPASSXC_KDBXXMLAUTHENTICATIONFACTORREADER_H

#include <QCoreApplication>
#include <QPointer>
#include <QXmlStreamReader>

#include "core/Database.h"
#include "format/multifactor/AuthenticationFactor.h"
#include "format/multifactor/AuthenticationFactorGroup.h"
#include "format/multifactor/AuthenticationFactorInfo.h"

/**
 * KDBX XML payload reader.
 */
class KdbxXmlAuthenticationFactorReader
{
    Q_DECLARE_TR_FUNCTIONS(KdbxXmlAuthenticationFactorReader)

public:
    explicit KdbxXmlAuthenticationFactorReader() = default;
    virtual ~KdbxXmlAuthenticationFactorReader() = default;

    virtual QSharedPointer<AuthenticationFactorInfo> readAuthenticationFactors(Database* db,
                                                                               const QString& authenticationFactorXml);

    [[nodiscard]] bool hasError() const;
    [[nodiscard]] QString errorString() const;

protected:
    void raiseError(const QString& errorMessage);

    bool parseFactorInfo(const QSharedPointer<AuthenticationFactorInfo>& info);
    bool parseFactorGroup(const QSharedPointer<AuthenticationFactorInfo>& info);
    bool parseFactor(AuthenticationFactorGroup* group);

    bool m_error = false;
    QString m_errorStr = "";
    QXmlStreamReader m_xml;
};

#endif // KEEPASSXC_KDBXXMLAUTHENTICATIONFACTORREADER_H
