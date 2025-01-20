/*
 *  Copyright (C) 2012 Felix Geyer <debfx@fobos.de>
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

#ifndef KEEPASSX_AUTOTYPE_H
#define KEEPASSX_AUTOTYPE_H

#include "AutoTypeAction.h"

#include <QMutex>
#include <QTimer>
#include <QWidget>

#include "AutoTypeAction.h"
#include "AutoTypeMatch.h"

class AutoTypePlatformInterface;
class Database;
class Entry;
class QPluginLoader;

class AutoType : public QObject
{
    Q_OBJECT

public:
    QStringList windowTitles();
    bool registerGlobalShortcut(Qt::Key key, Qt::KeyboardModifiers modifiers, QString* error = nullptr);
    void unregisterGlobalShortcut();
    void performAutoType(const Entry* entry);
    void performAutoTypeWithSequence(const Entry* entry, const QString& sequence);

    static bool verifyAutoTypeSyntax(const QString& sequence, const Entry* entry, QString& error);

    inline bool isAvailable()
    {
        return m_plugin;
    }

    static AutoType* instance();
    static void createTestInstance();

public slots:
    void performGlobalAutoType(const QList<QSharedPointer<Database>>& dbList, const QString& search = {});
    void raiseWindow();

signals:
    void globalAutoTypeTriggered(const QString& search);
    void autotypeFinished();
    void autotypeRetypeTimeout();

private slots:
    void startGlobalAutoType(const QString& search);
    void unloadPlugin();
    void resetAutoTypeState();

private:
    enum WindowState
    {
        Normal,
        Minimized,
        Hidden
    };

    explicit AutoType(QObject* parent = nullptr, bool test = false);
    ~AutoType() override;
    void loadPlugin(const QString& pluginPath);
    void executeAutoTypeActions(const Entry* entry,
                                const QString& sequence = QString(),
                                WId window = 0,
                                AutoTypeExecutor::Mode mode = AutoTypeExecutor::Mode::NORMAL);
    void restoreWindowState();

    static QList<QSharedPointer<AutoTypeAction>>
    parseSequence(const QString& entrySequence, const Entry* entry, QString& error, bool syntaxOnly = false);

    QMutex m_inAutoType;
    QMutex m_inGlobalAutoTypeDialog;
    QPluginLoader* m_pluginLoader;
    AutoTypePlatformInterface* m_plugin;
    AutoTypeExecutor* m_executor;
    static AutoType* m_instance;

    QString m_windowTitleForGlobal;
    WindowState m_windowState;
    WId m_windowForGlobal;
    AutoTypeMatch m_lastMatch;
    QTimer m_lastMatchRetypeTimer;

    Q_DISABLE_COPY(AutoType)
};

inline AutoType* autoType()
{
    return AutoType::instance();
}

#endif // KEEPASSX_AUTOTYPE_H
