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

#include "HashingStream.h"

HashingStream::HashingStream(QIODevice* baseDevice)
    : LayeredStream(baseDevice)
    , m_hash(QCryptographicHash::Md5)
    , m_sizeToHash(0)
{
    init();
}

HashingStream::HashingStream(QIODevice* baseDevice, QCryptographicHash::Algorithm hashAlgo, qint64 sizeToHash)
    : LayeredStream(baseDevice)
    , m_hash(hashAlgo)
    , m_sizeToHash(sizeToHash)
{
    init();
}

HashingStream::~HashingStream()
{
    close();
}

void HashingStream::init()
{
    m_sizeHashed = 0;
    m_sizeStreamed = 0;
    m_hashFinalized = false;
}

bool HashingStream::reset()
{
    init();
    m_hash.reset();
    return LayeredStream::reset();
}

QByteArray HashingStream::hashingResult()
{
    if (m_sizeHashed <= 0 || (m_sizeToHash > 0 && m_sizeHashed != m_sizeToHash)) {
        setErrorString("Not enough data to compute hash");
        return {};
    }
    m_hashFinalized = true;
    return m_hash.result();
}

qint64 HashingStream::readData(char* data, qint64 maxSize)
{
    auto sizeRead = LayeredStream::readData(data, maxSize);
    if (sizeRead > 0) {
        if (!m_hashFinalized) {
            qint64 sizeToHash = sizeRead;
            if (m_sizeToHash > 0) {
                sizeToHash = qMin(m_sizeToHash - m_sizeStreamed, sizeRead);
            }
            if (sizeToHash > 0) {
                m_hash.addData(data, sizeToHash);
                m_sizeHashed += sizeToHash;
            }
        }
        m_sizeStreamed += sizeRead;
    }
    return sizeRead;
}

qint64 HashingStream::writeData(const char* data, qint64 maxSize)
{
    auto sizeWritten = LayeredStream::writeData(data, maxSize);
    if (sizeWritten > 0) {
        if (!m_hashFinalized) {
            qint64 sizeToHash = sizeWritten;
            if (m_sizeToHash > 0) {
                sizeToHash = qMin(m_sizeToHash - m_sizeStreamed, sizeWritten);
            }
            if (sizeToHash > 0) {
                m_hash.addData(data, sizeToHash);
                m_sizeHashed += sizeToHash;
            }
        }
        m_sizeStreamed += sizeWritten;
    }
    return sizeWritten;
}
