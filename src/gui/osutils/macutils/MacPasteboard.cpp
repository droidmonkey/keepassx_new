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

#include "MacPasteboard.h"
#include <QStringConverter>

QString MacPasteboard::utiForMime(const QString& mime) const
{
    if (mime == QLatin1String("text/plain")) {
        return QLatin1String("public.utf8-plain-text");
    } else if (mime == QLatin1String("application/x-nspasteboard-concealed-type")) {
        return QLatin1String("org.nspasteboard.ConcealedType");
    }

    int i = mime.indexOf(QLatin1String("charset="));

    if (i >= 0) {
        QString cs(mime.mid(i + 8).toLower());
        i = cs.indexOf(QLatin1Char(';'));

        if (i >= 0) {
            cs = cs.left(i);
        }

        if (cs == QLatin1String("system")) {
            return QLatin1String("public.utf8-plain-text");
        } else if (cs == QLatin1String("iso-10646-ucs-2") || cs == QLatin1String("utf16")) {
            return QLatin1String("public.utf16-plain-text");
        }
    }
    return {};
}

QString MacPasteboard::mimeForUti(const QString& uti) const
{
    if (uti == QLatin1String("public.utf8-plain-text"))
        return QLatin1String("text/plain");
    if (uti == QLatin1String("org.nspasteboard.ConcealedType"))
        return QLatin1String("application/x-nspasteboard-concealed-type");
    if (uti == QLatin1String("public.utf16-plain-text"))
        return QLatin1String("text/plain;charset=utf16");
    return {};
}

bool MacPasteboard::canConvert(const QString& mime, const QString& uti) const
{
    Q_UNUSED(mime);
    Q_UNUSED(uti);
    return true;
}

QVariant MacPasteboard::convertToMime(const QString& mime, const QList<QByteArray>& data, const QString& uti) const
{
    if (data.count() > 1)
        qWarning("QMime::convertToMime: Cannot handle multiple member data");
    const QByteArray& firstData = data.first();
    QVariant ret;
    if (uti == QLatin1String("public.utf8-plain-text")) {
        ret = QString::fromUtf8(firstData);
    } else if (uti == QLatin1String("org.nspasteboard.ConcealedType")) {
        ret = QString::fromUtf8(firstData);
    } else if (uti == QLatin1String("public.utf16-plain-text")) {
        auto toUtf16 = QStringDecoder(QStringDecoder::Utf16);
        QVariant var{toUtf16(firstData)};
        return var;
    } else {
        qWarning("QMime::convertToMime: unhandled mimetype: %s", qPrintable(mime));
    }
    return ret;
}

QList<QByteArray> MacPasteboard::convertFromMime(const QString& mime, const QVariant& data, const QString& uti) const
{
    Q_UNUSED(mime);

    QList<QByteArray> ret;
    QString dataString = data.toString();
    if (uti == QLatin1String("public.utf8-plain-text")) {
        ret.append(dataString.toUtf8());
    } else if (uti == QLatin1String("org.nspasteboard.ConcealedType")) {
        ret.append(dataString.toUtf8());
    } else if (uti == QLatin1String("public.utf16-plain-text")) {
        auto toUtf16 = QStringEncoder(QStringDecoder::Utf16);
        QByteArray baUtf16 = toUtf16(dataString);
        ret.append(baUtf16);
    }
    return ret;
}
