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

#include "RemoteHandler.h"

#include "RemoteProcess.h"
#include "RemoteSettings.h"

#include "core/AsyncTask.h"
#include "core/Database.h"

std::function<QScopedPointer<RemoteProcess>(QObject*)> RemoteHandler::m_createRemoteProcess([](QObject* parent) {
    return QScopedPointer<RemoteProcess>(new RemoteProcess(parent));
});

RemoteHandler::RemoteHandler(QObject* parent)
    : QObject(parent)
{
}

RemoteHandler::~RemoteHandler()
{
    QFileInfo file(m_tempFileLocation);
    if (file.absoluteDir().exists() && file.absoluteDir().dirName().startsWith(PREFIX)) {
        file.absoluteDir().removeRecursively();
    }
}

void RemoteHandler::setRemoteProcessFunc(std::function<QScopedPointer<RemoteProcess>(QObject*)> func)
{
    m_createRemoteProcess = std::move(func);
}

RemoteHandler::RemoteResult RemoteHandler::download(const RemoteParams* params)
{
    return AsyncTask::runAndWaitForFuture([this, params] {
        RemoteResult result;
        if (!params) {
            result.success = false;
            result.errorMessage = tr("Invalid download parameters provided.");
            return result;
        }

        QString error;
        m_tempFileLocation = getTempFileLocation(&error);
        result.filePath = m_tempFileLocation;
        if (!error.isEmpty()) {
            result.success = false;
            result.errorMessage = error;
            return result;
        }

        auto remoteProcess = m_createRemoteProcess(nullptr); // use nullptr parent, otherwise there is a warning
        remoteProcess->setTempFileLocation(m_tempFileLocation);
        remoteProcess->start(params->downloadCommand);
        if (!params->downloadInput.isEmpty()) {
            remoteProcess->write(params->downloadInput + "\n");
            remoteProcess->waitForBytesWritten();
            remoteProcess->closeWriteChannel();
        }

        bool finished = remoteProcess->waitForFinished(params->downloadTimeoutMsec);
        int statusCode = remoteProcess->exitCode();

        // TODO: For future use
        result.stdOutput = remoteProcess->readOutput();
        result.stdError = remoteProcess->readError();

        if (finished && statusCode == 0) {
            // Check if the file actually downloaded
            QFileInfo fileInfo(m_tempFileLocation);
            if (!fileInfo.exists() || fileInfo.size() == 0) {
                result.success = false;
                result.errorMessage = tr("Command `%1` failed to download database.").arg(params->downloadCommand);
            } else {
                result.success = true;
            }
        } else if (finished) {
            result.success = false;
            result.errorMessage =
                tr("Command `%1` exited with status code: %2").arg(params->downloadCommand).arg(statusCode);
        } else {
            remoteProcess->kill();
            result.success = false;
            result.errorMessage =
                tr("Command `%1` did not finish in time. Process was killed.").arg(params->downloadCommand);
        }

        return result;
    });
}

RemoteHandler::RemoteResult RemoteHandler::upload(const RemoteParams* params)
{
    return AsyncTask::runAndWaitForFuture([this, params] {
        RemoteResult result;
        result.filePath = m_tempFileLocation;
        if (!params) {
            result.success = false;
            result.errorMessage = tr("Invalid database pointer or upload parameters provided.");
            return result;
        }

        auto remoteProcess = m_createRemoteProcess(nullptr); // use nullptr parent, otherwise there is a warning
        remoteProcess->setTempFileLocation(m_tempFileLocation);
        remoteProcess->start(params->uploadCommand);
        if (!params->uploadInput.isEmpty()) {
            remoteProcess->write(params->uploadInput + "\n");
            remoteProcess->waitForBytesWritten();
            remoteProcess->closeWriteChannel();
        }

        bool finished = remoteProcess->waitForFinished(params->uploadTimeoutMsec);
        int statusCode = remoteProcess->exitCode();

        // TODO: For future use
        result.stdOutput = remoteProcess->readOutput();
        result.stdError = remoteProcess->readError();

        if (finished && statusCode == 0) {
            result.success = true;
        } else if (finished) {
            result.success = false;
            result.errorMessage = tr("Failed to upload merged database. Command `%1` exited with status code: %2")
                                      .arg(params->uploadCommand)
                                      .arg(statusCode);
        } else {
            remoteProcess->kill();
            result.success = false;
            result.errorMessage =
                tr("Failed to upload merged database. Command `%1` did not finish in time. Process was killed.")
                    .arg(params->uploadCommand);
        }

        return result;
    });
}

QString RemoteHandler::getTempFileLocation(QString* error)
{
    QString uuid = QUuid::createUuid().toString().remove(0, 1);
    uuid.chop(1);
    QString writableLocation = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (writableLocation.isEmpty()) {
        writableLocation = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    }

    QString tempDirLocation = QDir(writableLocation).absoluteFilePath(PREFIX + uuid);
    QString tempFileLocation =
        QDir::toNativeSeparators(QDir(tempDirLocation).absoluteFilePath("RemoteDatabase-" + uuid + ".kdbx"));

    if (!QDir().mkdir(tempDirLocation)) {
        *error = tr("Could not create temporary directory '%1'").arg(tempDirLocation);
        return "";
    }

    if (!QFile::setPermissions(tempDirLocation,
                               QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner)) {
        QDir(tempDirLocation).removeRecursively();
        *error = tr("Could not change permissions of temporary directory '%1' to owner").arg(tempDirLocation);
        return "";
    }

    return tempFileLocation;
}
