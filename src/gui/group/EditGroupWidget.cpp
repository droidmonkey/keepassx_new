/*
 *  Copyright (C) 2011 Felix Geyer <debfx@fobos.de>
 *  Copyright (C) 2022 KeePassXC Team <team@keepassxc.org>
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

#include "EditGroupWidget.h"
#include "ui_EditGroupWidgetMain.h"
#if defined(KPXC_FEATURE_BROWSER)
#include "browser/BrowserService.h"
#include "ui_EditGroupWidgetBrowser.h"
#endif

#include "core/Config.h"
#include "core/Metadata.h"
#include "gui/EditWidgetIcons.h"
#include "gui/EditWidgetProperties.h"
#include "gui/Font.h"
#include "gui/Icons.h"
#include "gui/MessageBox.h"
#include "keeshare/group/EditGroupPageKeeShare.h"

class EditGroupWidget::ExtraPage
{
public:
    ExtraPage(IEditGroupPage* page, QWidget* widget)
        : editPage(page)
        , widget(widget)
    {
    }

    void set(Group* temporaryGroup, QSharedPointer<Database> database) const
    {
        editPage->set(widget, temporaryGroup, database);
    }

    void assign() const
    {
        editPage->assign(widget);
    }

    QWidget* getWidget()
    {
        return widget;
    }

private:
    QSharedPointer<IEditGroupPage> editPage;
    QWidget* widget;
};

EditGroupWidget::EditGroupWidget(QWidget* parent)
    : EditWidget(parent)
    , m_mainUi(new Ui::EditGroupWidgetMain())
    , m_editGroupWidgetMain(new QScrollArea())
    , m_editGroupWidgetIcons(new EditWidgetIcons())
    , m_editWidgetProperties(new EditWidgetProperties())
#if defined(KPXC_FEATURE_BROWSER)
    , m_browserSettingsChanged(false)
    , m_browserUi(new Ui::EditGroupWidgetBrowser())
    , m_browserWidget(new QWidget(this))
#endif
    , m_group(nullptr)
{
    m_mainUi->setupUi(m_editGroupWidgetMain);

    addPage(tr("Group"), icons()->icon("document-edit"), m_editGroupWidgetMain);
    addPage(tr("Icon"), icons()->icon("preferences-desktop-icons"), m_editGroupWidgetIcons);
#if defined(KPXC_FEATURE_BROWSER)
    if (config()->get(Config::Browser_Enabled).toBool()) {
        initializeBrowserPage();
    }
#endif
    addEditPage(new EditGroupPageKeeShare(this));
    addPage(tr("Properties"), icons()->icon("document-properties"), m_editWidgetProperties);

    connect(m_mainUi->expireCheck, SIGNAL(toggled(bool)), m_mainUi->expireDatePicker, SLOT(setEnabled(bool)));
    connect(m_mainUi->autoTypeSequenceCustomRadio,
            SIGNAL(toggled(bool)),
            m_mainUi->autoTypeSequenceCustomEdit,
            SLOT(setEnabled(bool)));

    connect(this, SIGNAL(apply()), SLOT(apply()));
    connect(this, SIGNAL(accepted()), SLOT(save()));
    connect(this, SIGNAL(rejected()), SLOT(cancel()));

    // clang-format off
    connect(m_editGroupWidgetIcons,
            SIGNAL(messageEditEntry(QString,MessageWidget::MessageType)),
            SLOT(showMessage(QString,MessageWidget::MessageType)));
    // clang-format on

    connect(m_editGroupWidgetIcons, SIGNAL(messageEditEntryDismiss()), SLOT(hideMessage()));

    setupModifiedTracking();
}

EditGroupWidget::~EditGroupWidget() = default;

void EditGroupWidget::setupModifiedTracking()
{
    // Group tab
    connect(m_mainUi->editName, SIGNAL(textChanged(QString)), SLOT(setModified()));
    connect(m_mainUi->editNotes, SIGNAL(textChanged()), SLOT(setModified()));
    connect(m_mainUi->expireCheck, SIGNAL(stateChanged(int)), SLOT(setModified()));
    connect(m_mainUi->expireDatePicker, SIGNAL(dateTimeChanged(QDateTime)), SLOT(setModified()));
    connect(m_mainUi->searchComboBox, SIGNAL(currentIndexChanged(int)), SLOT(setModified()));
    connect(m_mainUi->autotypeComboBox, SIGNAL(currentIndexChanged(int)), SLOT(setModified()));
    connect(m_mainUi->autoTypeSequenceInherit, SIGNAL(toggled(bool)), SLOT(setModified()));
    connect(m_mainUi->autoTypeSequenceCustomRadio, SIGNAL(toggled(bool)), SLOT(setModified()));
    connect(m_mainUi->autoTypeSequenceCustomEdit, SIGNAL(textChanged(QString)), SLOT(setModified()));

    // Icon tab
    connect(m_editGroupWidgetIcons, SIGNAL(widgetUpdated()), SLOT(setModified()));

#if defined(KPXC_FEATURE_BROWSER)
    if (config()->get(Config::Browser_Enabled).toBool()) {
        setupBrowserModifiedTracking();
    }
#endif
}

void EditGroupWidget::loadGroup(Group* group, bool create, const QSharedPointer<Database>& database)
{
    m_group = group;
    m_db = database;

    m_temporaryGroup.reset(group->clone(Entry::CloneNoFlags, Group::CloneNoFlags));
    connect(m_temporaryGroup->customData(), &CustomData::modified, this, [this]() { setModified(true); });

    if (create) {
        setHeadline(tr("Add group"));
    } else {
        setHeadline(tr("Edit group"));
    }

    if (m_group->parentGroup()) {
        addTriStateItems(m_mainUi->searchComboBox, m_group->parentGroup()->resolveSearchingEnabled());
        addTriStateItems(m_mainUi->autotypeComboBox, m_group->parentGroup()->resolveAutoTypeEnabled());
    } else {
        addTriStateItems(m_mainUi->searchComboBox, true);
        addTriStateItems(m_mainUi->autotypeComboBox, true);
    }

    m_mainUi->editName->setText(m_group->name());
    m_mainUi->editNotes->setPlainText(m_group->notes());
    m_mainUi->expireCheck->setChecked(group->timeInfo().expires());
    m_mainUi->expireDatePicker->setDateTime(group->timeInfo().expiryTime().toLocalTime());
    m_mainUi->searchComboBox->setCurrentIndex(indexFromTriState(group->searchingEnabled()));
    m_mainUi->autotypeComboBox->setCurrentIndex(indexFromTriState(group->autoTypeEnabled()));
    if (group->defaultAutoTypeSequence().isEmpty()) {
        m_mainUi->autoTypeSequenceInherit->setChecked(true);
    } else {
        m_mainUi->autoTypeSequenceCustomRadio->setChecked(true);
    }
    m_mainUi->autoTypeSequenceCustomEdit->setText(group->effectiveAutoTypeSequence());

    if (config()->get(Config::GUI_MonospaceNotes).toBool()) {
        m_mainUi->editNotes->setFont(Font::fixedFont());
    } else {
        m_mainUi->editNotes->setFont(Font::defaultFont());
    }

    IconStruct iconStruct;
    iconStruct.uuid = m_temporaryGroup->iconUuid();
    iconStruct.number = m_temporaryGroup->iconNumber();
    m_editGroupWidgetIcons->load(m_temporaryGroup->uuid(), m_db, iconStruct);
    m_editWidgetProperties->setFields(m_temporaryGroup->timeInfo(), m_temporaryGroup->uuid());
    m_editWidgetProperties->setCustomData(m_temporaryGroup->customData());

    for (const ExtraPage& page : asConst(m_extraPages)) {
        page.set(m_temporaryGroup.data(), m_db);
    }

#ifdef KPXC_FEATURE_BROWSER
    if (config()->get(Config::Browser_Enabled).toBool()) {
        auto inheritHideEntries = false;
        auto inheritSkipSubmit = false;
        auto inheritOnlyHttp = false;
        auto inheritNoHttp = false;
        auto inheritOmitWww = false;
        auto inheritRestrictKey = QString();

        auto parent = group->parentGroup();
        if (parent) {
            inheritHideEntries = parent->resolveCustomDataTriState(BrowserService::OPTION_HIDE_ENTRY);
            inheritSkipSubmit = parent->resolveCustomDataTriState(BrowserService::OPTION_SKIP_AUTO_SUBMIT);
            inheritOnlyHttp = parent->resolveCustomDataTriState(BrowserService::OPTION_ONLY_HTTP_AUTH);
            inheritNoHttp = parent->resolveCustomDataTriState(BrowserService::OPTION_NOT_HTTP_AUTH);
            inheritOmitWww = parent->resolveCustomDataTriState(BrowserService::OPTION_OMIT_WWW);
            inheritRestrictKey = parent->resolveCustomDataString(BrowserService::OPTION_RESTRICT_KEY);
        }

        // If the page has not been created at all, some of the elements are null
        if (m_browserUi->browserIntegrationHideEntriesComboBox == nullptr
            && config()->get(Config::Browser_Enabled).toBool()) {
            initializeBrowserPage();
            setupBrowserModifiedTracking();
        }

        setPageHidden(m_browserWidget, false);
        addTriStateItems(m_browserUi->browserIntegrationHideEntriesComboBox, inheritHideEntries);
        addTriStateItems(m_browserUi->browserIntegrationSkipAutoSubmitComboBox, inheritSkipSubmit);
        addTriStateItems(m_browserUi->browserIntegrationOnlyHttpAuthComboBox, inheritOnlyHttp);
        addTriStateItems(m_browserUi->browserIntegrationNotHttpAuthComboBox, inheritNoHttp);
        addTriStateItems(m_browserUi->browserIntegrationOmitWwwCombobox, inheritOmitWww);
        addRestrictKeyComboBoxItems(m_db->metadata()->customData()->keys(), inheritRestrictKey);

        m_browserUi->browserIntegrationHideEntriesComboBox->setCurrentIndex(
            indexFromTriState(group->resolveCustomDataTriState(BrowserService::OPTION_HIDE_ENTRY, false)));
        m_browserUi->browserIntegrationSkipAutoSubmitComboBox->setCurrentIndex(
            indexFromTriState(group->resolveCustomDataTriState(BrowserService::OPTION_SKIP_AUTO_SUBMIT, false)));
        m_browserUi->browserIntegrationOnlyHttpAuthComboBox->setCurrentIndex(
            indexFromTriState(group->resolveCustomDataTriState(BrowserService::OPTION_ONLY_HTTP_AUTH, false)));
        m_browserUi->browserIntegrationNotHttpAuthComboBox->setCurrentIndex(
            indexFromTriState(group->resolveCustomDataTriState(BrowserService::OPTION_NOT_HTTP_AUTH, false)));
        m_browserUi->browserIntegrationOmitWwwCombobox->setCurrentIndex(
            indexFromTriState(group->resolveCustomDataTriState(BrowserService::OPTION_OMIT_WWW, false)));
        setRestrictKeyComboBoxIndex(group);
    } else if (hasPage(m_browserWidget)) {
        setPageHidden(m_browserWidget, true);
    }
#endif

    setCurrentPage(0);

    m_mainUi->editName->setFocus();

    // Force the user to Save/Discard new groups
    showApplyButton(!create);

    setModified(false);
}

void EditGroupWidget::save()
{
    apply();
    clear();
    emit editFinished(true);
}

void EditGroupWidget::apply()
{
    m_temporaryGroup->setName(m_mainUi->editName->text());
    m_temporaryGroup->setNotes(m_mainUi->editNotes->toPlainText());
    m_temporaryGroup->setExpires(m_mainUi->expireCheck->isChecked());
    m_temporaryGroup->setExpiryTime(m_mainUi->expireDatePicker->dateTime().toUTC());

    m_temporaryGroup->setSearchingEnabled(triStateFromIndex(m_mainUi->searchComboBox->currentIndex()));
    m_temporaryGroup->setAutoTypeEnabled(triStateFromIndex(m_mainUi->autotypeComboBox->currentIndex()));

    if (m_mainUi->autoTypeSequenceInherit->isChecked()) {
        m_temporaryGroup->setDefaultAutoTypeSequence(QString());
    } else {
        m_temporaryGroup->setDefaultAutoTypeSequence(m_mainUi->autoTypeSequenceCustomEdit->text());
    }

    IconStruct iconStruct = m_editGroupWidgetIcons->state();

    if (iconStruct.number < 0) {
        m_temporaryGroup->setIcon(Group::DefaultIconNumber);
    } else if (iconStruct.uuid.isNull()) {
        m_temporaryGroup->setIcon(iconStruct.number);
    } else {
        m_temporaryGroup->setIcon(iconStruct.uuid);
    }

    for (const ExtraPage& page : asConst(m_extraPages)) {
        page.assign();
    }

#ifdef KPXC_FEATURE_BROWSER
    if (config()->get(Config::Browser_Enabled).toBool()) {
        if (!m_browserSettingsChanged) {
            return;
        }

        m_temporaryGroup->setCustomDataTriState(
            BrowserService::OPTION_HIDE_ENTRY,
            triStateFromIndex(m_browserUi->browserIntegrationHideEntriesComboBox->currentIndex()));
        m_temporaryGroup->setCustomDataTriState(
            BrowserService::OPTION_SKIP_AUTO_SUBMIT,
            triStateFromIndex(m_browserUi->browserIntegrationSkipAutoSubmitComboBox->currentIndex()));
        m_temporaryGroup->setCustomDataTriState(
            BrowserService::OPTION_ONLY_HTTP_AUTH,
            triStateFromIndex(m_browserUi->browserIntegrationOnlyHttpAuthComboBox->currentIndex()));
        m_temporaryGroup->setCustomDataTriState(
            BrowserService::OPTION_NOT_HTTP_AUTH,
            triStateFromIndex(m_browserUi->browserIntegrationNotHttpAuthComboBox->currentIndex()));
        m_temporaryGroup->setCustomDataTriState(
            BrowserService::OPTION_OMIT_WWW,
            triStateFromIndex(m_browserUi->browserIntegrationOmitWwwCombobox->currentIndex()));
        setRestrictKeyCustomData(m_temporaryGroup->customData());
    }
#endif

    // Icons add/remove are applied globally outside the transaction!
    m_group->copyDataFrom(m_temporaryGroup.data());

    // Assign the icon to children if selected
    if (iconStruct.applyTo == ApplyIconToOptions::CHILD_GROUPS
        || iconStruct.applyTo == ApplyIconToOptions::ALL_CHILDREN) {
        m_group->applyGroupIconToChildGroups();
    }

    if (iconStruct.applyTo == ApplyIconToOptions::CHILD_ENTRIES
        || iconStruct.applyTo == ApplyIconToOptions::ALL_CHILDREN) {
        m_group->applyGroupIconToChildEntries();
    }

    setModified(false);
}

void EditGroupWidget::cancel()
{
    if (!m_group->iconUuid().isNull() && !m_db->metadata()->hasCustomIcon(m_group->iconUuid())) {
        m_group->setIcon(Entry::DefaultIconNumber);
    }

    if (isModified()) {
        auto result = MessageBox::question(this,
                                           QString(),
                                           tr("Group has unsaved changes"),
                                           MessageBox::Cancel | MessageBox::Save | MessageBox::Discard,
                                           MessageBox::Cancel);
        if (result == MessageBox::Cancel) {
            return;
        }
        if (result == MessageBox::Save) {
            save();
            return;
        }
    }

    clear();
    emit editFinished(false);
}

#ifdef KPXC_FEATURE_BROWSER
void EditGroupWidget::initializeBrowserPage()
{
    addPage(tr("Browser Integration"), icons()->icon("internet-web-browser"), m_browserWidget);
    m_browserUi->setupUi(m_browserWidget);
}

void EditGroupWidget::setupBrowserModifiedTracking()
{
    // Browser integration tab
    connect(m_browserUi->browserIntegrationHideEntriesComboBox, SIGNAL(currentIndexChanged(int)), SLOT(setModified()));
    connect(
        m_browserUi->browserIntegrationSkipAutoSubmitComboBox, SIGNAL(currentIndexChanged(int)), SLOT(setModified()));
    connect(m_browserUi->browserIntegrationOnlyHttpAuthComboBox, SIGNAL(currentIndexChanged(int)), SLOT(setModified()));
    connect(m_browserUi->browserIntegrationNotHttpAuthComboBox, SIGNAL(currentIndexChanged(int)), SLOT(setModified()));
    connect(m_browserUi->browserIntegrationHideEntriesComboBox,
            SIGNAL(currentIndexChanged(int)),
            SLOT(updateBrowserModified()));
    connect(m_browserUi->browserIntegrationSkipAutoSubmitComboBox,
            SIGNAL(currentIndexChanged(int)),
            SLOT(updateBrowserModified()));
    connect(m_browserUi->browserIntegrationOnlyHttpAuthComboBox,
            SIGNAL(currentIndexChanged(int)),
            SLOT(updateBrowserModified()));
    connect(m_browserUi->browserIntegrationNotHttpAuthComboBox,
            SIGNAL(currentIndexChanged(int)),
            SLOT(updateBrowserModified()));
}

void EditGroupWidget::updateBrowserModified()
{
    m_browserSettingsChanged = true;
}
#endif

void EditGroupWidget::clear()
{
    m_group = nullptr;
    m_db.reset();
    m_temporaryGroup.reset(nullptr);
    m_editGroupWidgetIcons->reset();
}

void EditGroupWidget::addEditPage(IEditGroupPage* page)
{
    QWidget* widget = page->createWidget();
    widget->setParent(this);

    m_extraPages.append(ExtraPage(page, widget));
    addPage(page->name(), page->icon(), widget);
}

void EditGroupWidget::addTriStateItems(QComboBox* comboBox, bool inheritDefault)
{
    QString inheritDefaultString;
    if (inheritDefault) {
        inheritDefaultString = tr("Enable");
    } else {
        inheritDefaultString = tr("Disable");
    }

    comboBox->clear();
    comboBox->addItem(tr("Inherit from parent group (%1)").arg(inheritDefaultString));
    comboBox->addItem(tr("Enable"));
    comboBox->addItem(tr("Disable"));
}

int EditGroupWidget::indexFromTriState(Group::TriState triState)
{
    switch (triState) {
    case Group::Inherit:
        return 0;
    case Group::Enable:
        return 1;
    case Group::Disable:
        return 2;
    default:
        Q_ASSERT(false);
        return 0;
    }
}

Group::TriState EditGroupWidget::triStateFromIndex(int index)
{
    switch (index) {
    case 0:
        return Group::Inherit;
    case 1:
        return Group::Enable;
    case 2:
        return Group::Disable;
    default:
        Q_ASSERT(false);
        return Group::Inherit;
    }
}

#ifdef KPXC_FEATURE_BROWSER
void EditGroupWidget::addRestrictKeyComboBoxItems(QStringList const& keyList, QString inheritValue)
{
    auto comboBox = m_browserUi->browserIntegrationRestrictKeyCombobox;

    comboBox->clear();
    comboBox->addItem(
        tr("Inherit from parent group (%1)").arg(BrowserService::decodeCustomDataRestrictKey(inheritValue)));
    comboBox->addItem(tr("Disable"));

    comboBox->insertSeparator(2);

    // Add all the browser keys to the combobox
    for (const QString& key : keyList) {
        if (key.startsWith(CustomData::BrowserKeyPrefix)) {
            auto strippedKey = key;
            strippedKey.remove(CustomData::BrowserKeyPrefix);
            comboBox->addItem(strippedKey);
        }
    }
}

void EditGroupWidget::setRestrictKeyComboBoxIndex(const Group* group)
{
    auto comboBox = m_browserUi->browserIntegrationRestrictKeyCombobox;

    if (!group || !group->customData()->contains(BrowserService::OPTION_RESTRICT_KEY)) {
        comboBox->setCurrentIndex(0);
        return;
    }

    auto key = group->customData()->value(BrowserService::OPTION_RESTRICT_KEY);
    if (key.isEmpty()) {
        comboBox->setCurrentIndex(1);
    } else {
        comboBox->setCurrentText(key);
    }
}

// Set the customData regarding OPTION_RESTRICT_KEY
void EditGroupWidget::setRestrictKeyCustomData(CustomData* customData)
{
    auto comboBox = m_browserUi->browserIntegrationRestrictKeyCombobox;
    auto key = BrowserService::OPTION_RESTRICT_KEY;
    auto idx = comboBox->currentIndex();
    if (idx == 0) {
        customData->remove(key);
    } else if (idx == 1) {
        customData->set(key, QString());
    } else {
        customData->set(key, comboBox->currentText());
    }
}
#endif
