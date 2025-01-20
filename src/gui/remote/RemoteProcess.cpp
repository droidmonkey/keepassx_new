/*
 *  Copyright (C) 2023 KeePassXC Team <team@keepassxc.org>
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

#include "RemoteProcess.h"

#include <QUuid>

RemoteProcess::RemoteProcess(QObject* parent)
    : m_process(new QProcess(parent))
{
}

RemoteProcess::~RemoteProcess()
{
}

void RemoteProcess::setTempFileLocation(const QString& tempFile)
{
    m_tempFileLocation = tempFile;
}

void RemoteProcess::start(const QString& command)
{
    const QString commandResolved = resolveTemplateVariables(command);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    QStringList cmdList = QProcess::splitCommand(commandResolved);
    if (!cmdList.isEmpty()) {
        const QString program = cmdList.takeFirst();
        m_process->start(program, cmdList);
    }
#else
    m_process->start(resolveTemplateVariables(commandResolved));
#endif

    m_process->waitForStarted();
}

qint64 RemoteProcess::write(const QString& input)
{
    auto resolved = resolveTemplateVariables(input);
    return m_process->write(resolved.toUtf8());
}

bool RemoteProcess::waitForBytesWritten()
{
    return m_process->waitForBytesWritten();
}

void RemoteProcess::closeWriteChannel()
{
    m_process->closeWriteChannel();
}

bool RemoteProcess::waitForFinished(int msecs)
{
    return m_process->waitForFinished(msecs);
}

int RemoteProcess::exitCode() const
{
    return m_process->exitCode();
}

QString RemoteProcess::readOutput()
{
    return m_process->readAllStandardOutput();
}

QString RemoteProcess::readError()
{
    return m_process->readAllStandardError();
}

void RemoteProcess::kill() const
{
    m_process->kill();
}

QString RemoteProcess::resolveTemplateVariables(const QString& input) const
{
    QString resolved = input;
    return resolved.replace("{TEMP_DATABASE}", m_tempFileLocation);
}
