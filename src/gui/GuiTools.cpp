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

#include "GuiTools.h"

#include "core/Config.h"
#include "core/Group.h"
#include "gui/MessageBox.h"

namespace GuiTools
{
    bool confirmDeleteEntries(QWidget* parent, const QList<Entry*>& entries, bool permanent)
    {
        if (!parent || entries.isEmpty()) {
            return false;
        }

        if (permanent) {
            QString prompt;
            if (entries.size() == 1) {
                prompt = QObject::tr("Do you really want to delete the entry \"%1\" for good?")
                             .arg(entries.first()->title().toHtmlEscaped());
            } else {
                prompt = QObject::tr("Do you really want to delete %n entry(s) for good?", "", entries.size());
            }

            auto answer = MessageBox::question(parent,
                                               QObject::tr("Delete entry(s)?", "", entries.size()),
                                               prompt,
                                               MessageBox::Delete | MessageBox::Cancel,
                                               MessageBox::Cancel);

            return answer == MessageBox::Delete;
        } else if (config()->get(Config::Security_NoConfirmMoveEntryToRecycleBin).toBool()) {
            return true;
        } else {
            QString prompt;
            if (entries.size() == 1) {
                prompt = QObject::tr("Do you really want to move entry \"%1\" to the recycle bin?")
                             .arg(entries.first()->title().toHtmlEscaped());
            } else {
                prompt = QObject::tr("Do you really want to move %n entry(s) to the recycle bin?", "", entries.size());
            }

            auto answer = MessageBox::question(parent,
                                               QObject::tr("Move entry(s) to recycle bin?", "", entries.size()),
                                               prompt,
                                               MessageBox::Move | MessageBox::Cancel,
                                               MessageBox::Cancel);

            return answer == MessageBox::Move;
        }
    }

    bool confirmDeletePluginData(QWidget* parent, const QList<Entry*>& entries)
    {
        if (!parent || entries.isEmpty()) {
            return false;
        }

        auto answer = MessageBox::question(parent,
                                           QObject::tr("Delete plugin data?"),
                                           QObject::tr("Delete plugin data from Entry(s)?", "", entries.size()),
                                           MessageBox::Delete | MessageBox::Cancel,
                                           MessageBox::Cancel);

        return answer == MessageBox::Delete;
    }

    size_t deleteEntriesResolveReferences(QWidget* parent, const QList<Entry*>& entries, bool permanent)
    {
        if (!parent || entries.isEmpty()) {
            return 0;
        }

        QList<Entry*> selectedEntries;
        // Find references to entries and prompt for direction if necessary
        for (auto entry : entries) {
            if (permanent) {
                auto references = entry->database()->rootGroup()->referencesRecursive(entry);
                if (!references.isEmpty()) {
                    // Ignore references that are part of this cohort
                    for (auto e : entries) {
                        references.removeAll(e);
                    }
                    // Prompt the user on what to do with the reference (Overwrite, Delete, Skip)
                    auto result = MessageBox::question(
                        parent,
                        QObject::tr("Replace references to entry?"),
                        QObject::tr(
                            "Entry \"%1\" has %2 reference(s). "
                            "Do you want to overwrite references with values, skip this entry, or delete anyway?",
                            "",
                            references.size())
                            .arg(entry->resolvePlaceholder(entry->title()).toHtmlEscaped())
                            .arg(references.size()),
                        MessageBox::Overwrite | MessageBox::Skip | MessageBox::Delete,
                        MessageBox::Overwrite);

                    if (result == MessageBox::Overwrite) {
                        for (auto ref : references) {
                            ref->replaceReferencesWithValues(entry);
                        }
                    } else if (result == MessageBox::Skip) {
                        continue;
                    }
                }
            }
            // Marked for deletion
            selectedEntries << entry;
        }

        for (auto entry : asConst(selectedEntries)) {
            if (permanent) {
                delete entry;
            } else {
                entry->database()->recycleEntry(entry);
            }
        }
        return selectedEntries.size();
    }
} // namespace GuiTools
