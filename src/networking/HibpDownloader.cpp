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

#include "HibpDownloader.h"
#include "NetworkManager.h"

#include <QCryptographicHash>
#include <QNetworkReply>
#include <QStringView>

namespace
{
    /*
     * Return the SHA1 hash of the specified password in upper-case hex.
     *
     * The result is always exactly 40 characters long.
     */
    QString sha1Hex(const QString& password)
    {
        // Get the binary SHA1
        const auto sha1 = QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha1);
        return sha1.toHex().toUpper();
    }

    /*
     * Search a password's hash in the output of the HIBP web service.
     *
     * Returns the number of times the password is found in breaches, or
     * 0 if the password is not in the HIBP result.
     */
    int pwnCount(const QString& password, const QString& hibpResult)
    {
        // The first 5 characters of the hash are in the URL already,
        // the HIBP result contains the remainder
        auto pos = hibpResult.indexOf(sha1Hex(password).mid(5));
        if (pos < 0) {
            return 0;
        }

        // Skip past the sha1 and ':'
        pos += 36;

        // Find where the count ends
        auto end = hibpResult.indexOf('\n', pos);
        if (end < 0) {
            end = hibpResult.size();
        }

        // Extract the count, remove remaining whitespace, and convert to int
        return QStringView{hibpResult}.mid(pos, end - pos).trimmed().toInt();
    }
} // namespace

HibpDownloader::HibpDownloader(QObject* parent)
    : QObject(parent)
{
}

HibpDownloader::~HibpDownloader()
{
    abort();
}

/*
 * Add one password to the list list of passwords to check.
 *
 * Invoke this function once for every password to check,
 * then call validate().
 */
void HibpDownloader::add(const QString& password)
{
    if (!m_pwdsToTry.contains(password)) {
        m_pwdsToTry << password;
    }
}

/*
 * Start validating the passwords against HIBP.
 */
void HibpDownloader::validate()
{
    for (auto password : m_pwdsToTry) {
        // The URL we query is https://api.pwnedpasswords.com/range/XXXXX,
        // where XXXXX is the first five bytes of the hex representation of
        // the password's SHA1.
        const auto url = QString("https://api.pwnedpasswords.com/range/") + sha1Hex(password).left(5);

        // HIBP requires clients to specify a user agent in the request
        // (https://haveibeenpwned.com/API/v3#UserAgent); however, in order
        // to minimize the amount of information we expose about ourselves,
        // we don't add the KeePassXC version number or platform.
        auto request = QNetworkRequest(url);
        request.setRawHeader("User-Agent", "KeePassXC");

        // Finally, submit the request to HIBP.
        auto reply = getNetMgr()->get(request);
        connect(reply, &QNetworkReply::finished, this, &HibpDownloader::fetchFinished);
        connect(reply, &QIODevice::readyRead, this, &HibpDownloader::fetchReadyRead);
        m_replies.insert(reply, {password, {}});
    }

    m_pwdsToTry.clear();
}

int HibpDownloader::passwordsToValidate() const
{
    return m_pwdsToTry.size();
}

int HibpDownloader::passwordsRemaining() const
{
    return m_replies.size();
}

/*
 * Abort the current online activity (if any).
 */
void HibpDownloader::abort()
{
    for (auto reply : m_replies.keys()) {
        reply->abort();
        reply->deleteLater();
    }
    m_replies.clear();
}

/*
 * Called when new data has been loaded from the HIBP server.
 */
void HibpDownloader::fetchReadyRead()
{
    const auto reply = qobject_cast<QNetworkReply*>(sender());
    auto entry = m_replies.find(reply);
    if (entry != m_replies.end()) {
        entry->second += reply->readAll();
    }
}

/*
 * Called after all data has been loaded from the HIBP server.
 */
void HibpDownloader::fetchFinished()
{
    const auto reply = qobject_cast<QNetworkReply*>(sender());
    const auto entry = m_replies.find(reply);
    if (entry == m_replies.end()) {
        return;
    }

    // Get result status
    const auto ok = reply->error() == QNetworkReply::NoError;
    const auto err = reply->errorString();

    const auto password = entry->first;
    const auto hibpReply = entry->second;

    reply->deleteLater();
    m_replies.remove(reply);

    // If there was an error, assume it's permanent and abort
    // (don't process the rest of the password list).
    if (!ok) {
        auto msg = tr("Online password validation failed") + ":\n" + err;
        if (!hibpReply.isEmpty()) {
            msg += "\n" + hibpReply;
        }
        abort();
        emit fetchFailed(msg);
        return;
    }

    // Current password validated, send the result to the caller
    emit hibpResult(password, pwnCount(password, hibpReply));
}
