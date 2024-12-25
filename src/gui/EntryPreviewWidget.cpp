/*
 *  Copyright (C) 2023 KeePassXC Team <team@keepassxc.org>
 *  Copyright (C) 2012 Felix Geyer <debfx@fobos.de>
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

#include "EntryPreviewWidget.h"
#include "ui_EntryPreviewWidget.h"

#include "Application.h"
#include "core/Clock.h"
#include "core/Config.h"
#include "core/Totp.h"
#include "gui/Font.h"
#include "gui/Icons.h"
#if defined(WITH_XC_KEESHARE)
#include "keeshare/KeeShare.h"
#include "keeshare/KeeShareSettings.h"
#endif

#include <QScrollBar>
#include <QTabWidget>
namespace
{
    constexpr int GeneralTabIndex = 0;
}

EntryPreviewWidget::EntryPreviewWidget(QWidget* parent)
    : QWidget(parent)
    , m_ui(new Ui::EntryPreviewWidget())
    , m_locked(false)
    , m_currentEntry(nullptr)
    , m_currentGroup(nullptr)
    , m_selectedTabEntry(0)
    , m_selectedTabGroup(0)
{
    m_ui->setupUi(this);

    // Entry
    m_ui->entryTotpButton->setIcon(icons()->icon("totp"));
    m_ui->entryCloseButton->setIcon(icons()->icon("arrow-collapse-down"));
    m_ui->toggleUsernameButton->setIcon(icons()->onOffIcon("password-show", true));
    m_ui->togglePasswordButton->setIcon(icons()->onOffIcon("password-show", true));
    m_ui->toggleEntryNotesButton->setIcon(icons()->onOffIcon("password-show", true));
    m_ui->toggleGroupNotesButton->setIcon(icons()->onOffIcon("password-show", true));

    m_ui->entryAttachmentsWidget->setReadOnly(true);
    m_ui->entryAttachmentsWidget->setButtonsVisible(false);

    // Match background of read-only text edit fields with the window
    m_ui->entryPasswordLabel->setBackgroundRole(QPalette::Window);
    m_ui->entryUsernameLabel->setBackgroundRole(QPalette::Window);
    m_ui->entryNotesTextEdit->setBackgroundRole(QPalette::Window);
    m_ui->groupNotesTextEdit->setBackgroundRole(QPalette::Window);
    // Align notes text with label text
    m_ui->entryNotesTextEdit->document()->setDocumentMargin(0);
    m_ui->groupNotesTextEdit->document()->setDocumentMargin(0);

    m_ui->entryTotpLabel->installEventFilter(this);

    connect(m_ui->entryTotpButton, SIGNAL(toggled(bool)), m_ui->entryTotpLabel, SLOT(setVisible(bool)));
    connect(m_ui->entryTotpButton, SIGNAL(toggled(bool)), m_ui->entryTotpProgress, SLOT(setVisible(bool)));
    connect(m_ui->entryCloseButton, SIGNAL(clicked()), SLOT(hide()));
    connect(m_ui->toggleUsernameButton, SIGNAL(clicked(bool)), SLOT(setUsernameVisible(bool)));
    connect(m_ui->togglePasswordButton, SIGNAL(clicked(bool)), SLOT(setPasswordVisible(bool)));
    connect(m_ui->toggleEntryNotesButton, SIGNAL(clicked(bool)), SLOT(setEntryNotesVisible(bool)));
    connect(m_ui->toggleGroupNotesButton, SIGNAL(clicked(bool)), SLOT(setGroupNotesVisible(bool)));
    connect(m_ui->entryTabWidget, SIGNAL(tabBarClicked(int)), SLOT(updateTabIndexes()), Qt::QueuedConnection);
    // Prevent the url from being focused after clicked to allow the Copy Password button to work properly
    connect(m_ui->entryUrlLabel, &QLabel::linkActivated, this, [this] {
        openEntryUrl();
        m_ui->entryTabWidget->setFocus();
    });
    connect(&m_totpTimer, SIGNAL(timeout()), SLOT(updateTotpLabel()));

    connect(m_ui->entryAttributesTable, &QTableWidget::itemDoubleClicked, this, [this](QTableWidgetItem* item) {
        auto userData = item->data(Qt::UserRole);
        if (userData.isValid()) {
            emit copyTextRequested(userData.toString());
        }
    });

    connect(config(), &Config::changed, this, [this](Config::ConfigKey key) {
        if (key == Config::GUI_HidePreviewPanel) {
            setVisible(!config()->get(Config::GUI_HidePreviewPanel).toBool());
        }
        refresh();
    });

    // Group
    m_ui->groupCloseButton->setIcon(icons()->icon("dialog-close"));
    connect(m_ui->groupCloseButton, SIGNAL(clicked()), SLOT(hide()));
    connect(m_ui->groupTabWidget, SIGNAL(tabBarClicked(int)), SLOT(updateTabIndexes()), Qt::QueuedConnection);

    setFocusProxy(m_ui->entryTabWidget);

#if !defined(WITH_XC_KEESHARE)
    removeTab(m_ui->groupTabWidget, m_ui->groupShareTab);
#endif
}

EntryPreviewWidget::~EntryPreviewWidget() = default;

bool EntryPreviewWidget::eventFilter(QObject* object, QEvent* event)
{
    if (object == m_ui->entryTotpLabel && event->type() == QEvent::MouseButtonDblClick) {
        if (m_currentEntry && m_currentEntry->hasTotp()) {
            emit copyTextRequested(m_currentEntry->totp());
            m_ui->entryTotpLabel->clearFocus();
            return true;
        }
    }
    return QWidget::eventFilter(object, event);
}

void EntryPreviewWidget::clear()
{
    hide();
    m_currentEntry = nullptr;
    m_currentGroup = nullptr;
    m_ui->entryAttachmentsWidget->unlinkAttachments();
}

void EntryPreviewWidget::setEntry(Entry* selectedEntry)
{
    if (m_currentEntry == selectedEntry) {
        return;
    }

    if (m_currentEntry) {
        disconnect(m_currentEntry, nullptr, this, nullptr);
    }
    if (m_currentGroup) {
        disconnect(m_currentGroup, nullptr, this, nullptr);
    }

    m_currentEntry = selectedEntry;
    m_currentGroup = nullptr;

    if (!m_currentEntry) {
        hide();
        return;
    }

    connect(m_currentEntry, &Entry::modified, this, &EntryPreviewWidget::refresh);
    refresh();

    if (m_currentEntry->hasTotp()) {
        m_ui->entryTotpButton->setChecked(!config()->get(Config::Security_HideTotpPreviewPanel).toBool());
    }
}

void EntryPreviewWidget::setGroup(Group* selectedGroup)
{
    if (m_currentGroup == selectedGroup) {
        return;
    }

    if (m_currentEntry) {
        disconnect(m_currentEntry, nullptr, this, nullptr);
    }
    if (m_currentGroup) {
        disconnect(m_currentGroup, nullptr, this, nullptr);
    }

    m_currentEntry = nullptr;
    m_currentGroup = selectedGroup;

    if (!m_currentGroup) {
        hide();
        return;
    }

    connect(m_currentGroup, &Group::modified, this, &EntryPreviewWidget::refresh);
    refresh();
}

void EntryPreviewWidget::refresh()
{
    if (m_currentEntry) {
        updateEntryHeaderLine();
        updateEntryTotp();
        updateEntryGeneralTab();
        updateEntryAdvancedTab();
        updateEntryAutotypeTab();

        setVisible(!config()->get(Config::GUI_HidePreviewPanel).toBool());

        m_ui->stackedWidget->setCurrentWidget(m_ui->pageEntry);
        const int tabIndex =
            m_ui->entryTabWidget->isTabEnabled(m_selectedTabEntry) ? m_selectedTabEntry : GeneralTabIndex;
        Q_ASSERT(m_ui->entryTabWidget->isTabEnabled(GeneralTabIndex));
        m_ui->entryTabWidget->setCurrentIndex(tabIndex);
    } else if (m_currentGroup) {
        updateGroupHeaderLine();
        updateGroupGeneralTab();

#if defined(WITH_XC_KEESHARE)
        updateGroupSharingTab();
#endif

        setVisible(!config()->get(Config::GUI_HidePreviewPanel).toBool());

        m_ui->stackedWidget->setCurrentWidget(m_ui->pageGroup);
        const int tabIndex =
            m_ui->groupTabWidget->isTabEnabled(m_selectedTabGroup) ? m_selectedTabGroup : GeneralTabIndex;
        Q_ASSERT(m_ui->groupTabWidget->isTabEnabled(GeneralTabIndex));
        m_ui->groupTabWidget->setCurrentIndex(tabIndex);
    } else {
        hide();
    }
}

void EntryPreviewWidget::setDatabaseMode(DatabaseWidget::Mode mode)
{
    m_locked = mode == DatabaseWidget::Mode::LockedMode;
    if (m_locked) {
        return;
    }

    if (mode == DatabaseWidget::Mode::ViewMode) {
        if (m_currentGroup && m_ui->stackedWidget->currentWidget() == m_ui->pageGroup) {
            setGroup(m_currentGroup);
        } else if (m_currentEntry) {
            setEntry(m_currentEntry);
        } else {
            hide();
        }
    }
}

void EntryPreviewWidget::updateEntryHeaderLine()
{
    Q_ASSERT(m_currentEntry);
    const QString title = m_currentEntry->resolveMultiplePlaceholders(m_currentEntry->title());
    m_ui->entryTitleLabel->setRawText(hierarchy(m_currentEntry->group(), title.toHtmlEscaped()));
    m_ui->entryIcon->setPixmap(Icons::entryIconPixmap(m_currentEntry, IconSize::Large));
}

void EntryPreviewWidget::updateEntryTotp()
{
    Q_ASSERT(m_currentEntry);
    const bool hasTotp = m_currentEntry->hasTotp();
    m_ui->entryTotpButton->setVisible(hasTotp);

    if (hasTotp) {
        m_totpTimer.start(1000);
        m_ui->entryTotpProgress->setMaximum(m_currentEntry->totpSettings()->step);
        updateTotpLabel();
    } else {
        m_ui->entryTotpLabel->hide();
        m_ui->entryTotpProgress->hide();
        m_ui->entryTotpButton->setChecked(false);
        m_ui->entryTotpLabel->clear();
        m_totpTimer.stop();
    }
}

void EntryPreviewWidget::setUsernameVisible(bool state)
{
    if (state) {
        auto username = m_currentEntry->resolveMultiplePlaceholders(m_currentEntry->username());
        m_ui->entryUsernameLabel->setText(username);
        m_ui->entryUsernameLabel->setFont(Font::defaultFont());
        m_ui->entryUsernameLabel->setCursorPosition(0);
    } else {
        m_ui->entryUsernameLabel->setText(QString("\u25cf").repeated(6));
        m_ui->entryUsernameLabel->setFont(Font::fixedFont());
    }

    m_ui->toggleUsernameButton->setIcon(icons()->onOffIcon("password-show", state));
}

void EntryPreviewWidget::setPasswordVisible(bool state)
{
    m_ui->entryPasswordLabel->setFont(Font::fixedFont());

    const QString password = m_currentEntry->resolveMultiplePlaceholders(m_currentEntry->password());
    if (state) {
        if (config()->get(Config::GUI_ColorPasswords).toBool()) {
            // Show the password in color
            // clang-format off
            QString html;
            const auto dark = kpxcApp->isDarkTheme();
            for (const auto c : password) {
                const auto color = c.isDigit()   ? (dark ? "lightblue" : "blue")
                                   : c.isUpper() ? (dark ? "lightgreen" : "darkgreen")
                                   : c.isLower() ? (dark ? "yellow" : "red")
                                                 : (dark ? "white" : "black");
                html += "<span style=\"color: " + QString(color) + ";\">" + QString(c).toHtmlEscaped() + "</span>";
            }
            // clang-format on
            m_ui->entryPasswordLabel->setText(html);
        } else {
            // No color
            m_ui->entryPasswordLabel->setText(password.toHtmlEscaped());
        }
    } else if (password.isEmpty() && !config()->get(Config::Security_PasswordEmptyPlaceholder).toBool()) {
        m_ui->entryPasswordLabel->setText("");
    } else {
        m_ui->entryPasswordLabel->setText(QString("\u25cf").repeated(6));
    }

    m_ui->passwordScrollArea->setMaximumHeight(m_ui->entryPasswordLabel->sizeHint().height()
                                               + m_ui->passwordScrollArea->horizontalScrollBar()->sizeHint().height());

    m_ui->togglePasswordButton->setIcon(icons()->onOffIcon("password-show", state));
}

void EntryPreviewWidget::setEntryNotesVisible(bool state)
{
    setNotesVisible(
        m_ui->entryNotesTextEdit, m_currentEntry->resolveMultiplePlaceholders(m_currentEntry->notes()), state);
    m_ui->toggleEntryNotesButton->setIcon(icons()->onOffIcon("password-show", state));
}

void EntryPreviewWidget::setGroupNotesVisible(bool state)
{
    setNotesVisible(m_ui->groupNotesTextEdit, m_currentGroup->notes(), state);
    m_ui->toggleGroupNotesButton->setIcon(icons()->onOffIcon("password-show", state));
}

void EntryPreviewWidget::setNotesVisible(QTextEdit* notesWidget, const QString& notes, bool state)
{
    if (state) {
        notesWidget->setPlainText(notes);
        notesWidget->moveCursor(QTextCursor::Start);
        notesWidget->ensureCursorVisible();
    } else {
        if (!notes.isEmpty()) {
            notesWidget->setPlainText(QString("\u25cf").repeated(6));
        } else {
            notesWidget->setPlainText("");
        }
    }
}

void EntryPreviewWidget::updateEntryGeneralTab()
{
    Q_ASSERT(m_currentEntry);

    if (config()->get(Config::GUI_HideUsernames).toBool()) {
        setUsernameVisible(false);
        // Show the username toggle button
        m_ui->toggleUsernameButton->setVisible(!m_currentEntry->username().isEmpty());
        m_ui->toggleUsernameButton->setChecked(false);
    } else {
        setUsernameVisible(true);
        m_ui->toggleUsernameButton->setVisible(false);
    }

    if (config()->get(Config::Security_HidePasswordPreviewPanel).toBool()) {
        // Hide password
        setPasswordVisible(false);
        // Show the password toggle button if there are dots in the label
        m_ui->togglePasswordButton->setVisible(!m_currentEntry->password().isEmpty());
        m_ui->togglePasswordButton->setChecked(false);
    } else {
        // Show password
        setPasswordVisible(true);
        m_ui->togglePasswordButton->setVisible(false);
    }

    auto hasNotes = !m_currentEntry->notes().isEmpty();
    auto hideNotes = config()->get(Config::Security_HideNotes).toBool();

    setEntryNotesVisible(hasNotes && !hideNotes);
    m_ui->toggleEntryNotesButton->setVisible(hasNotes && hideNotes
                                             && !m_ui->entryNotesTextEdit->toPlainText().isEmpty());
    m_ui->toggleEntryNotesButton->setChecked(false);

    if (config()->get(Config::GUI_MonospaceNotes).toBool()) {
        m_ui->entryNotesTextEdit->setFont(Font::fixedFont());
    } else {
        m_ui->entryNotesTextEdit->setFont(Font::defaultFont());
    }

    m_ui->entryUrlLabel->setRawText(m_currentEntry->displayUrl().toHtmlEscaped());
    const QString url = m_currentEntry->url();
    if (!url.isEmpty()) {
        // URL is well formed and can be opened in a browser
        m_ui->entryUrlLabel->setUrl(m_currentEntry->resolveMultiplePlaceholders(url));
        m_ui->entryUrlLabel->setCursor(Qt::PointingHandCursor);
        m_ui->entryUrlLabel->setOpenExternalLinks(false);
    } else {
        m_ui->entryUrlLabel->setUrl({});
        m_ui->entryUrlLabel->setCursor(Qt::ArrowCursor);
    }

    const TimeInfo entryTime = m_currentEntry->timeInfo();
    const QString expires = entryTime.expires() ? Clock::toString(entryTime.expiryTime().toLocalTime()) : tr("Never");
    m_ui->entryExpirationLabel->setText(expires);
    m_ui->entryTagsList->tags(m_currentEntry->tagList());
    m_ui->entryTagsList->setReadOnly(true);
}

void EntryPreviewWidget::updateEntryAdvancedTab()
{
    Q_ASSERT(m_currentEntry);
    m_ui->entryAttributesTable->clear();

    const EntryAttributes* attributes = m_currentEntry->attributes();
    const QStringList customAttributes = attributes->customKeys();
    const bool hasAttributes = !customAttributes.isEmpty();
    const bool hasAttachments = !m_currentEntry->attachments()->isEmpty();
    m_ui->entryAttributesTable->setRowCount(customAttributes.size());
    m_ui->entryAttributesTable->setColumnCount(3);

    setTabEnabled(m_ui->entryTabWidget, m_ui->entryAdvancedTab, hasAttributes || hasAttachments);
    if (hasAttributes) {
        auto i = 0;
        QFont font;
        font.setBold(true);
        for (const QString& key : customAttributes) {
            m_ui->entryAttributesTable->setItem(i, 0, new QTableWidgetItem(key));
            m_ui->entryAttributesTable->item(i, 0)->setFont(font);
            m_ui->entryAttributesTable->item(i, 0)->setTextAlignment(Qt::AlignTop | Qt::AlignLeft);

            auto value = m_currentEntry->resolveMultiplePlaceholders(attributes->value(key));

            if (attributes->isProtected(key)) {
                // only show the reveal button on protected attributes
                auto button = new QToolButton();
                button->setCheckable(true);
                button->setChecked(false);
                button->setIcon(icons()->onOffIcon("password-show", false));
                button->setProperty("row", i);
                button->setIconSize({12, 12});
                connect(button, &QToolButton::clicked, this, [this](bool state) {
                    auto btn = qobject_cast<QToolButton*>(sender());
                    btn->setIcon(icons()->onOffIcon("password-show", state));
                    auto item = m_ui->entryAttributesTable->item(btn->property("row").toInt(), 2);
                    if (state) {
                        item->setText(item->data(Qt::UserRole).toString());
                    } else {
                        item->setText(QString("\u25cf").repeated(6));
                    }
                    // Maintain button height while showing contents of cell
                    auto size = btn->size();
                    m_ui->entryAttributesTable->resizeRowToContents(item->row());
                    btn->setFixedSize(size);
                });

                m_ui->entryAttributesTable->setCellWidget(i, 1, button);
                m_ui->entryAttributesTable->setItem(i, 2, new QTableWidgetItem(QString("\u25cf").repeated(6)));
            } else {
                m_ui->entryAttributesTable->setItem(i, 2, new QTableWidgetItem(value));
            }

            m_ui->entryAttributesTable->item(i, 2)->setData(Qt::UserRole, value);
            m_ui->entryAttributesTable->item(i, 2)->setToolTip(tr("Double click to copy value"));
            m_ui->entryAttributesTable->item(i, 2)->setTextAlignment(Qt::AlignTop | Qt::AlignLeft);

            ++i;
        }
    }

    m_ui->entryAttributesTable->horizontalHeader()->setStretchLastSection(true);
    m_ui->entryAttributesTable->resizeColumnsToContents();
    m_ui->entryAttributesTable->resizeRowsToContents();
    m_ui->entryAttachmentsWidget->linkAttachments(m_currentEntry->attachments());
}

void EntryPreviewWidget::updateEntryAutotypeTab()
{
    Q_ASSERT(m_currentEntry);

    m_ui->entrySequenceLabel->setText(m_currentEntry->effectiveAutoTypeSequence());
    m_ui->entryAutotypeTree->clear();
    QList<QTreeWidgetItem*> items;
    const AutoTypeAssociations* autotypeAssociations = m_currentEntry->autoTypeAssociations();
    const auto associations = autotypeAssociations->getAll();
    for (const auto& assoc : associations) {
        const QString sequence =
            assoc.sequence.isEmpty() ? m_currentEntry->effectiveAutoTypeSequence() : assoc.sequence;
        items.append(new QTreeWidgetItem(m_ui->entryAutotypeTree, {assoc.window, sequence}));
    }

    m_ui->entryAutotypeTree->addTopLevelItems(items);
    setTabEnabled(m_ui->entryTabWidget,
                  m_ui->entryAutotypeTab,
                  m_currentEntry->autoTypeEnabled() && m_currentEntry->groupAutoTypeEnabled());
}

void EntryPreviewWidget::updateGroupHeaderLine()
{
    Q_ASSERT(m_currentGroup);
    m_ui->groupTitleLabel->setRawText(hierarchy(m_currentGroup, {}));
    m_ui->groupIcon->setPixmap(Icons::groupIconPixmap(m_currentGroup, IconSize::Large));
}

void EntryPreviewWidget::updateGroupGeneralTab()
{
    Q_ASSERT(m_currentGroup);
    const QString searchingText = m_currentGroup->resolveSearchingEnabled() ? tr("Enabled") : tr("Disabled");
    m_ui->groupSearchingLabel->setText(searchingText);

    const QString autotypeText = m_currentGroup->resolveAutoTypeEnabled() ? tr("Enabled") : tr("Disabled");
    m_ui->groupAutotypeLabel->setText(autotypeText);

    const TimeInfo groupTime = m_currentGroup->timeInfo();
    const QString expiresText = groupTime.expires() ? Clock::toString(groupTime.expiryTime()) : tr("Never");
    m_ui->groupExpirationLabel->setText(expiresText);

    if (config()->get(Config::Security_HideNotes).toBool()) {
        setGroupNotesVisible(false);
        m_ui->toggleGroupNotesButton->setVisible(!m_ui->groupNotesTextEdit->toPlainText().isEmpty());
        m_ui->toggleGroupNotesButton->setChecked(false);
    } else {
        setGroupNotesVisible(true);
        m_ui->toggleGroupNotesButton->setVisible(false);
    }

    if (config()->get(Config::GUI_MonospaceNotes).toBool()) {
        m_ui->groupNotesTextEdit->setFont(Font::fixedFont());
    } else {
        m_ui->groupNotesTextEdit->setFont(Font::defaultFont());
    }
}

#if defined(WITH_XC_KEESHARE)
void EntryPreviewWidget::updateGroupSharingTab()
{
    Q_ASSERT(m_currentGroup);
    setTabEnabled(m_ui->groupTabWidget, m_ui->groupShareTab, KeeShare::isShared(m_currentGroup));
    auto reference = KeeShare::referenceOf(m_currentGroup);
    m_ui->groupShareTypeLabel->setText(KeeShare::referenceTypeLabel(reference));
    m_ui->groupSharePathLabel->setText(reference.path);
}
#endif

void EntryPreviewWidget::updateTotpLabel()
{
    if (!m_locked && m_currentEntry && m_currentEntry->hasTotp()) {
        auto totpCode = m_currentEntry->totp();
        totpCode.insert(totpCode.size() / 2, " ");
        m_ui->entryTotpLabel->setText(totpCode);

        auto step = m_currentEntry->totpSettings()->step;
        auto timeleft = step - (Clock::currentSecondsSinceEpoch() % step);
        m_ui->entryTotpProgress->setValue(timeleft);
        m_ui->entryTotpProgress->update();
    } else {
        m_ui->entryTotpLabel->clear();
        m_totpTimer.stop();
    }
}

void EntryPreviewWidget::updateTabIndexes()
{
    m_selectedTabEntry = m_ui->entryTabWidget->currentIndex();
    m_selectedTabGroup = m_ui->groupTabWidget->currentIndex();
}

void EntryPreviewWidget::openEntryUrl()
{
    if (m_currentEntry) {
        emit entryUrlActivated(m_currentEntry);
    }
}

void EntryPreviewWidget::removeTab(QTabWidget* tabWidget, QWidget* widget)
{
    const int tabIndex = tabWidget->indexOf(widget);
    Q_ASSERT(tabIndex != -1);
    tabWidget->removeTab(tabIndex);
}

void EntryPreviewWidget::setTabEnabled(QTabWidget* tabWidget, QWidget* widget, bool enabled)
{
    const int tabIndex = tabWidget->indexOf(widget);
    Q_ASSERT(tabIndex != -1);
    tabWidget->setTabEnabled(tabIndex, enabled);
}

QString EntryPreviewWidget::hierarchy(const Group* group, const QString& title)
{
    if (group) {
        QString groupList = QString("%1").arg(group->hierarchy().join(" / "));
        return title.isEmpty() ? groupList : QString("%1 / %2").arg(groupList, title);
    }
    return {};
}
