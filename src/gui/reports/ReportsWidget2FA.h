//
// Created by Thomas on 1/04/2022.
//

#ifndef KEEPASSXC_REPORTSWIDGET2FA_H
#define KEEPASSXC_REPORTSWIDGET2FA_H

#include <QNetworkReply>
#include <QWidget>
#include <QIcon>
#include <QStandardItemModel>
#include <QJsonDocument>

class Database;
class Entry;

QT_BEGIN_NAMESPACE
namespace Ui
{
    class ReportsWidget2FA;
}
QT_END_NAMESPACE

class MFADirectoryEntry {
public:
    MFADirectoryEntry(bool supported, QString name, QString url, QString category, QString docs);

    bool supported();
    QString url();
    QString name();
    QString category();
    QString docs();

private:
    bool m_2faSupported;
    QString m_url;
    QString m_name;
    QString m_category;
    QString m_docs;

};

class ReportsWidget2FA : public QWidget
{
    Q_OBJECT

public:
    explicit ReportsWidget2FA(QWidget* parent = nullptr);
    ~ReportsWidget2FA() override;

    void loadSettings(QSharedPointer<Database> db);
    void saveSettings();

signals:
    void entryActivated(Entry*);

private slots:
    void fetchDirectoryFinished();
    void fetchDirectoryReadyRead();

    void refresh2FATable();

    void cellDoubleClick(const QModelIndex& index);


private:
    void make2FATable(bool useDirectory);

    void fetchDirectory();

    Ui::ReportsWidget2FA* m_ui;

    bool m_directoryUsed = false;

    QByteArray m_bytesReceived;
    QList<QPair<Entry*, QString>*> m_tableData;
    QList<MFADirectoryEntry> m_2faDirectoryEntries;
    QScopedPointer<QStandardItemModel> m_referencesModel;
    QSharedPointer<Database> m_db;
    QNetworkReply* m_reply;
};

#endif // KEEPASSXC_REPORTSWIDGET2FA_H
