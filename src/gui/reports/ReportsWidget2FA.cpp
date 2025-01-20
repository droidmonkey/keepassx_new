//
// Created by Thomas on 1/04/2022.
//

#include <QNetworkRequest>
#include <QNetworkAccessManager>
#include <QJsonObject>
#include <QJsonArray>
#include <utility>
#include <QDesktopServices>
#include "ReportsWidget2FA.h"
#include "ui_ReportsWidget2FA.h"
#include "gui/Icons.h"
#include "core/Group.h"
#include "core/NetworkManager.h"

ReportsWidget2FA::ReportsWidget2FA(QWidget* parent)
    : QWidget(parent)
    , m_ui(new Ui::ReportsWidget2FA)
{
    m_ui->setupUi(this);

    m_ui->downloadingDirectory->hide();
    m_ui->downloadDirectoryError->hide();

#ifdef WITH_XC_NETWORKING
    m_ui->noNetwork->hide();
#endif
    m_referencesModel.reset(new QStandardItemModel());
    m_ui->mfaTableView->setModel(m_referencesModel.data());
    m_ui->mfaTableView->setSelectionMode(QAbstractItemView::NoSelection);
    m_ui->mfaTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    connect(m_ui->enabledFilter, SIGNAL(clicked(bool)), this, SLOT(refresh2FATable()));
    connect(m_ui->disabledFilter, SIGNAL(clicked(bool)), this, SLOT(refresh2FATable()));
    connect(m_ui->notSupportedFilter, SIGNAL(clicked(bool)), this, SLOT(refresh2FATable()));
    connect(m_ui->supportedFilter, SIGNAL(clicked(bool)), this, SLOT(refresh2FATable()));

    connect(m_ui->mfaTableView, SIGNAL(doubleClicked(QModelIndex)), SLOT(cellDoubleClick(QModelIndex)));
}
ReportsWidget2FA::~ReportsWidget2FA()
{
    delete m_ui;
}

void ReportsWidget2FA::saveSettings()
{
    // no settings to save
}

void ReportsWidget2FA::loadSettings(QSharedPointer<Database> db)
{
    m_db = std::move(db);

    m_referencesModel->clear();

    auto row = QList<QStandardItem*>();
    row << new QStandardItem("Please wait while your database is analysed.");
    m_referencesModel->appendRow(row);

#ifdef WITH_XC_NETWORKING
    fetchDirectory();
#else
    make2FATable(false);
#endif
}

void ReportsWidget2FA::fetchDirectory()
{
    m_ui->downloadingDirectory->show();
    m_ui->downloadDirectoryError->hide();

    m_bytesReceived.clear();

    QUrl directoryUrl = QUrl("https://2fa.directory/api/v1/data.json");

    QNetworkRequest request(directoryUrl);

    m_reply = getNetMgr()->get(request);

    connect(m_reply, &QNetworkReply::finished, this, &ReportsWidget2FA::fetchDirectoryFinished);
    connect(m_reply, &QIODevice::readyRead, this, &ReportsWidget2FA::fetchDirectoryReadyRead);
}

void ReportsWidget2FA::fetchDirectoryReadyRead()
{
    m_bytesReceived += m_reply->readAll();
}

void ReportsWidget2FA::fetchDirectoryFinished()
{
    bool error = (m_reply->error() != QNetworkReply::NoError);

    m_reply->deleteLater();
    m_reply = nullptr;

    if(!error){
        QJsonDocument jsonResponse = QJsonDocument::fromJson(m_bytesReceived);
        QJsonObject jsonObject = jsonResponse.object();

        // object is a dictionary of category->entryName->entry
        m_2faDirectoryEntries.clear();

        for(auto category : jsonObject.keys()) {
            if(!jsonObject.value(category).isObject())
                goto error; // The schema appears to have changed

            auto categoryEntries = jsonObject.value(category).toObject();

            for(const auto& categoryEntriesName : categoryEntries.keys()){
                auto jsonEntry = categoryEntries.value(categoryEntriesName);
                if(!jsonEntry.isObject())
                    goto error;

                auto entry = jsonEntry.toObject();

                auto entryInstance = new MFADirectoryEntry(
                    entry.contains("tfa") && entry.value("tfa").toBool(),
                    entry.value("name").toString(),
                    entry.value("url").toString(),
                    category,
                    entry.value("doc").toString()
                    );

                m_2faDirectoryEntries.append(*entryInstance);
            }
        }

        make2FATable(true);
    }else{
        goto error;
    }

    m_ui->downloadingDirectory->hide();
    return;

    error:
        m_ui->downloadDirectoryError->show();
        make2FATable(false);
}

void ReportsWidget2FA::make2FATable(bool useDirectory)
{
    m_directoryUsed = useDirectory;
    m_referencesModel->clear();

    if(useDirectory) {
        m_referencesModel->setHorizontalHeaderLabels(QStringList() << tr("Title") << tr("Path") << tr("2FA Enabled")
                                                                   << tr("2FA Supported") << tr("Matched Site")
                                                                   << tr("Documentation"));

        m_ui->supportedFilter->show();
        m_ui->notSupportedFilter->show();
    }else{
        m_referencesModel->setHorizontalHeaderLabels(QStringList() << tr("Title") << tr("Path") << tr("2FA Enabled"));

        m_ui->supportedFilter->hide();
        m_ui->notSupportedFilter->hide();
    }


    for(auto entry : m_db->rootGroup()->entriesRecursive()){
        if(!entry->isRecycled() && !entry->excludeFromReports()){
            auto group = entry->group();
            auto hasMFA = entry->hasTotp();
            MFADirectoryEntry* matchedDirectoryEntry = nullptr;

            auto row = QList<QStandardItem*>();
            row << new QStandardItem(Icons::entryIconPixmap(entry), entry->title())
                << new QStandardItem(Icons::groupIconPixmap(group), group->hierarchy().join("/"))
                << new QStandardItem(hasMFA ? tr("Enabled") : tr("Disabled"));

            if(hasMFA && !m_ui->enabledFilter->isChecked()) continue;
            if(!hasMFA && !m_ui->disabledFilter->isChecked()) continue;

            if(useDirectory){

                for(auto directoryEntry : m_2faDirectoryEntries){
                    if(QUrl(entry->url()).host() == QUrl(directoryEntry.url()).host()){ // highly likely this is a match
                        matchedDirectoryEntry = &directoryEntry;
                        break;
                    }
                }

                if(matchedDirectoryEntry == nullptr){
                    row << new QStandardItem(tr("Unknown"))
                        << new QStandardItem(tr("No match"));

                    if(!m_ui->notSupportedFilter->isChecked()) continue;
                }else{
                    row << new QStandardItem(matchedDirectoryEntry->supported() ? tr("Supported") : tr("Not Supported"))
                        << new QStandardItem(QString("%1 (%2)").arg(matchedDirectoryEntry->name(),
                                                                    matchedDirectoryEntry->url()))
                        << new QStandardItem(matchedDirectoryEntry->docs());

                    if(!m_ui->supportedFilter->isChecked()) continue;
                }
            }

            m_referencesModel->appendRow(row);
            m_tableData.append(new QPair(entry, matchedDirectoryEntry->docs()));
        }
    }
}

void ReportsWidget2FA::refresh2FATable()
{
    make2FATable(m_directoryUsed);
}

void ReportsWidget2FA::cellDoubleClick(const QModelIndex& index)
{
    if(!index.isValid()){
        return;
    }

    auto clickedRow = m_tableData[index.row()];
    if(index.column() == 5) { // docs column
        auto directoryUrl = clickedRow->second;
        QDesktopServices::openUrl(directoryUrl);
    }else{
        emit entryActivated(clickedRow->first);
    }
}

bool MFADirectoryEntry::supported()
{
    return m_2faSupported;
}

QString MFADirectoryEntry::url()
{
    return m_url;
}

QString MFADirectoryEntry::name()
{
    return m_name;
}

QString MFADirectoryEntry::category()
{
    return m_category;
}

QString MFADirectoryEntry::docs()
{
    return m_docs;
}
MFADirectoryEntry::MFADirectoryEntry(bool supported, QString name, QString url, QString category, QString docs)
{
    m_2faSupported = supported;
    m_name = std::move(name);
    m_url = std::move(url);
    m_category = std::move(category);
    m_docs = std::move(docs);
}

