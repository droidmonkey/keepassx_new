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

#include "UrlTools.h"
#if defined(WITH_XC_NETWORKING) || defined(WITH_XC_BROWSER)
#include <QHostAddress>
#include <QNetworkCookie>
#include <QNetworkCookieJar>
#endif
#include <QRegularExpression>
#include <QUrl>

const QString UrlTools::URL_WILDCARD = "1kpxcwc1";

Q_GLOBAL_STATIC(UrlTools, s_urlTools)

UrlTools* UrlTools::instance()
{
    return s_urlTools;
}

QUrl UrlTools::convertVariantToUrl(const QVariant& var) const
{
    QUrl url;
    if (var.canConvert<QUrl>()) {
        url = var.toUrl();
    }
    return url;
}

#if defined(WITH_XC_NETWORKING) || defined(WITH_XC_BROWSER)
QUrl UrlTools::getRedirectTarget(QNetworkReply* reply) const
{
    QVariant var = reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
    QUrl url = convertVariantToUrl(var);
    return url;
}

/**
 * Gets the base domain of URL or hostname.
 *
 * Returns the base domain, e.g. https://another.example.co.uk -> example.co.uk
 * Up-to-date list can be found: https://publicsuffix.org/list/public_suffix_list.dat
 */
QString UrlTools::getBaseDomainFromUrl(const QString& url) const
{
    auto qUrl = QUrl::fromUserInput(url);

    auto host = qUrl.host();
    if (isIpAddress(host)) {
        return host;
    }

    const auto tld = getTopLevelDomainFromUrl(qUrl.toString());
    if (tld.isEmpty() || tld.length() + 1 >= host.length()) {
        return host;
    }

    // Remove the top level domain part from the hostname, e.g. https://another.example.co.uk -> https://another.example
    host.chop(tld.length() + 1);
    // Split the URL and select the last part, e.g. https://another.example -> example
    QString baseDomain = host.split('.').last();
    // Append the top level domain back to the URL, e.g. example -> example.co.uk
    baseDomain.append(QString(".%1").arg(tld));

    return baseDomain;
}

/**
 * Gets the top level domain from URL.
 *
 * Returns the TLD e.g. https://another.example.co.uk -> co.uk
 */
QString UrlTools::getTopLevelDomainFromUrl(const QString& url) const
{
    auto host = QUrl::fromUserInput(url).host();
    if (isIpAddress(host)) {
        return host;
    }

    const auto numberOfDomainParts = host.split('.').length();
    static const auto dummy = QByteArrayLiteral("");

    // Only loop the amount of different parts found
    for (auto i = 0; i < numberOfDomainParts; ++i) {
        // Cut the first part from host
        host = host.mid(host.indexOf('.') + 1);

        QNetworkCookie cookie(dummy, dummy);
        cookie.setDomain(host);

        // Check if dummy cookie's domain/TLD matches with public suffix list
        if (!QNetworkCookieJar{}.setCookiesFromUrl(QList{cookie}, QUrl::fromUserInput(url))) {
            return host;
        }
    }

    return host;
}

bool UrlTools::isIpAddress(const QString& host) const
{
    // Handle IPv6 host with brackets, e.g [::1]
    const auto hostAddress = host.startsWith('[') && host.endsWith(']') ? host.mid(1, host.length() - 2) : host;
    QHostAddress address(hostAddress);
    return address.protocol() == QAbstractSocket::IPv4Protocol || address.protocol() == QAbstractSocket::IPv6Protocol;
}
#endif

// Returns true if URLs are identical. Paths with "/" are removed during comparison.
// URLs without scheme reverts to https.
// Special handling is needed because QUrl::matches() with QUrl::StripTrailingSlash does not strip "/" paths.
bool UrlTools::isUrlIdentical(const QString& first, const QString& second) const
{
    auto trimUrl = [](QString url) {
        url = url.trimmed();
        if (url.endsWith("/")) {
            url.remove(url.length() - 1, 1);
        }

        return url;
    };

    if (first.isEmpty() || second.isEmpty()) {
        return false;
    }

    // Replace URL wildcards for comparison if found
    const auto firstUrl = trimUrl(QString(first).replace("*", UrlTools::URL_WILDCARD));
    const auto secondUrl = trimUrl(QString(second).replace("*", UrlTools::URL_WILDCARD));
    if (firstUrl == secondUrl) {
        return true;
    }

    return QUrl(firstUrl).matches(QUrl(secondUrl), QUrl::StripTrailingSlash);
}

bool UrlTools::isUrlValid(const QString& urlField) const
{
    if (urlField.isEmpty() || urlField.startsWith("cmd://", Qt::CaseInsensitive)
        || urlField.startsWith("kdbx://", Qt::CaseInsensitive) || urlField.startsWith("{REF:A", Qt::CaseInsensitive)) {
        return true;
    }

    QUrl url;
    if (urlField.contains("://")) {
        url = urlField;
    } else {
        url = QUrl::fromUserInput(urlField);
    }

    if (url.scheme() != "file" && url.host().isEmpty()) {
        return false;
    }

    // Check for illegal characters. Adds also the wildcard * to the list
    QRegularExpression re("[<>\\^`{|}\\*]");
    auto match = re.match(urlField);
    if (match.hasMatch()) {
        return false;
    }

    return true;
}

bool UrlTools::domainHasIllegalCharacters(const QString& domain) const
{
    QRegularExpression re(R"([\s\^#|/:<>\?@\[\]\\])");
    return re.match(domain).hasMatch();
}
