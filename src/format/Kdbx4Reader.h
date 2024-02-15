/*
 *  Copyright (C) 2017 KeePassXC Team <team@keepassxc.org>
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

#ifndef KEEPASSX_KDBX4READER_H
#define KEEPASSX_KDBX4READER_H

#define AUTHENTICATION_FACTORS_HEADER_KEY "authentication_factors"

#include "format/KdbxReader.h"

/**
 * KDBX4 reader implementation.
 */
class Kdbx4Reader : public KdbxReader
{
    Q_DECLARE_TR_FUNCTIONS(Kdbx4Reader)

public:
    bool readDatabaseImpl(QIODevice* device,
                          const QByteArray& headerData,
                          QSharedPointer<const CompositeKey> key,
                          Database* db) override;
    QHash<QString, QByteArray> binaryPool() const;

protected:
    bool readHeaderField(StoreDataStream& headerStream, Database* db) override;

private:
    bool readInnerHeaderField(QIODevice* device);
    QVariantMap readVariantMap(QIODevice* device);

    QHash<QString, QByteArray> m_binaryPool;
};

#endif // KEEPASSX_KDBX4READER_H
