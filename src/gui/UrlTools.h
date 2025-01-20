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

#ifndef KEEPASSXC_URLTOOLS_H
#define KEEPASSXC_URLTOOLS_H

#include "config-keepassx.h"
#include <QObject>
#include <QUrl>
#include <QVariant>
#if defined(WITH_XC_NETWORKING) || defined(WITH_XC_BROWSER)
#include <QNetworkReply>
#endif

class UrlTools : public QObject
{
    Q_OBJECT

public:
    explicit UrlTools() = default;
    static UrlTools* instance();

#if defined(WITH_XC_NETWORKING) || defined(WITH_XC_BROWSER)
    QUrl getRedirectTarget(QNetworkReply* reply) const;
    QString getBaseDomainFromUrl(const QString& url) const;
    QString getTopLevelDomainFromUrl(const QString& url) const;
    bool isIpAddress(const QString& host) const;
#endif
    bool isUrlIdentical(const QString& first, const QString& second) const;
    bool isUrlValid(const QString& urlField) const;
    bool domainHasIllegalCharacters(const QString& domain) const;

    static const QString URL_WILDCARD;

private:
    QUrl convertVariantToUrl(const QVariant& var) const;

private:
    Q_DISABLE_COPY(UrlTools);
};

static inline UrlTools* urlTools()
{
    return UrlTools::instance();
}

#endif // KEEPASSXC_URLTOOLS_H
