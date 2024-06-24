/*
 *  Copyright (C) 2020 KeePassXC Team <team@keepassxc.org>
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

#ifndef KEEPASSXC_HIBPDOWNLOADER_H
#define KEEPASSXC_HIBPDOWNLOADER_H

#include "config-keepassx.h"
#include <QHash>
#include <QObject>

#ifndef KPXC_FEATURE_NETWORK
#error This file requires KeePassXC to be built with network support.
#endif

class QNetworkReply;

/*
 * Check if a password has been hacked by looking it up on the
 * "Have I Been Pwned" website (https://haveibeenpwned.com/)
 * in the background.
 *
 * Usage: Pass the password to check to the ctor and process
 * the `finished` signal to get the result. Process the
 * `failed` signal to handle errors.
 */
class HibpDownloader : public QObject
{
    Q_OBJECT

public:
    explicit HibpDownloader(QObject* parent = nullptr);
    ~HibpDownloader() override;

    void add(const QString& password);
    void validate();
    int passwordsToValidate() const;
    int passwordsRemaining() const;

signals:
    void hibpResult(const QString& password, int count);
    void fetchFailed(const QString& error);

public slots:
    void abort();

private slots:
    void fetchFinished();
    void fetchReadyRead();

private:
    QStringList m_pwdsToTry; // The list of remaining passwords to validate
    QHash<QNetworkReply*, QPair<QString, QByteArray>> m_replies;
};

#endif // KEEPASSXC_HIBPDOWNLOADER_H
