/*
  MIT License

  Copyright (c) 2021 Nicolai Trandafil

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#include "TagsEdit.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QCompleter>
#include <QDebug>
#include <QLinkedList>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QStyle>
#include <QStyleHints>
#include <QStyleOptionFrame>
#include <QTextLayout>

namespace
{
    constexpr int TAG_V_SPACING = 4;
    constexpr int TAG_H_SPACING = 4;

    constexpr QMargins TAG_INNER(5, 3, 4, 3);

    constexpr int TAG_CROSS_WIDTH = 5;
    constexpr float TAG_CROSS_RADIUS = TAG_CROSS_WIDTH / 2;
    constexpr int TAG_CROSS_PADDING = 5;

    class Tag
    {
    public:
        Tag() = default;
        Tag(const QString& text) : text(text.trimmed()), rect(), row() {}

        bool isEmpty() const noexcept
        {
            return text.isEmpty();
        }

        QString text;
    public:
        // Render state
        mutable QRect rect;
        mutable size_t row;
    };

} // namespace

class TagManager {
public:
    using iterator = QLinkedList<Tag>::iterator;
    using const_iterator = QLinkedList<Tag>::const_iterator;

    TagManager() : tags{Tag()}, editing_index(tags.begin()) {}
    template<typename InputIterator> TagManager(InputIterator begin, InputIterator end) {
        QSet<QString> unique_tags;
        for (auto it = begin; it != end; ++it) {
            Tag new_tag(*it);
            if (unique_tags.contains(new_tag.text)) {
                continue;
            }
            unique_tags.insert(new_tag.text);
            tags.push_back(new_tag);
        }

        if (tags.isEmpty()) {
            tags.push_back(Tag());
        }
        editing_index = tags.begin();
    }

    iterator begin() { return tags.begin(); }
    iterator end() { return tags.end(); }
    const_iterator begin() const { return tags.begin(); }
    const_iterator end() const { return tags.end(); }
    const_iterator cbegin() const { return tags.cbegin(); }
    const_iterator cend() const { return tags.cend(); }

    const Tag& back() const {
        return tags.back();
    }

    const Tag& front() const {
        return tags.front();
    }

    iterator editingIndex() { return editing_index; }

    const_iterator editingIndex() const { return editing_index; }

    bool isCurrentTextEmpty() const {
        return editing_index->isEmpty();
    }

    void setEditingIndex(const iterator& it) {
        if (editing_index == it) {
            return;
        }
        // Ensure Invariant-1. If the previously edited tag is empty, remove it.
        auto occurrencesOfCurrentText =
            std::count_if(tags.cbegin(), tags.cend(), [this](const auto& tag) { return tag.text == editing_index->text; });
        if (isCurrentTextEmpty() || occurrencesOfCurrentText > 1) {
            erase(editing_index);
        }
        editing_index = it;
    }

    iterator insert(const iterator& it, const Tag& tag) {
        return tags.insert(it, tag);
    }

    iterator erase(const iterator& it) {
        bool current_index_needs_update = it == editing_index;

        auto next = tags.erase(it);
        if (next == tags.end()) {
            next = std::prev(next);
        }

        if (current_index_needs_update) {
            editing_index = next;
        }

        return next;

    }

    bool isEmpty() const { return tags.isEmpty(); }
    int size() const { return tags.size(); }

private:
    QLinkedList<Tag> tags;
    // TODO Rename
    iterator editing_index;
};

// Invariant-1 ensures no empty tags apart from currently being edited.
// Default-state is one empty tag which is currently editing.
struct TagsEdit::Impl
{
    using iterator = QLinkedList<Tag>::iterator;
    using const_iterator = QLinkedList<Tag>::const_iterator;

    explicit Impl(TagsEdit* ifce)
        : ifce(ifce)
        , cursor(0)
        , select_start(0)
        , select_size(0)
        , cross_deleter(true)
    {
    }

    iterator begin()
    {
        return tags.begin();
    }

    iterator end()
    {
        return tags.end();
    }

    const_iterator begin() const
    {
        return tags.begin();
    }

    const_iterator end() const
    {
        return tags.end();
    }

    QRectF crossRect(const QRectF& r) const
    {
        QRectF cross(QPointF{0, 0}, QSizeF{TAG_CROSS_WIDTH + TAG_CROSS_PADDING * 2, r.top() - r.bottom()});
        cross.moveCenter(QPointF(r.right() - TAG_CROSS_RADIUS - TAG_CROSS_PADDING, r.center().y()));
        return cross;
    }

    bool isBeingEdited(const const_iterator& it) const
    {
        return it == tags.editingIndex();
    }

    bool inCrossArea(const const_iterator& it, const QPoint& point) const
    {
        return cross_deleter
                   ? crossRect(it->rect)
                             .adjusted(-TAG_CROSS_RADIUS, 0, 0, 0)
                             .translated(-ifce->horizontalScrollBar()->value(), -ifce->verticalScrollBar()->value())
                             .contains(point)
                         && (!cursorVisible() || !isBeingEdited(it))
                   : false;
    }

    void drawTag(QPainter& p, const Tag& tag) const
    {
        QRect const& i_r =
            tag.rect.translated(-ifce->horizontalScrollBar()->value(), -ifce->verticalScrollBar()->value());
        const auto text_pos =
            i_r.topLeft()
            + QPointF(TAG_INNER.left(),
                      ifce->fontMetrics().ascent() + ((i_r.height() - ifce->fontMetrics().height()) / 2));

        // draw tag rect
        auto palette = ifce->palette();
        QPainterPath path;
        auto cornerRadius = 4;
        path.addRoundedRect(i_r, cornerRadius, cornerRadius);
        p.fillPath(path, palette.brush(QPalette::ColorGroup::Inactive, QPalette::ColorRole::Highlight));

        // draw text
        p.drawText(text_pos, tag.text);

        if (cross_deleter) {
            // calc cross rect
            const auto i_cross_r = crossRect(i_r);

            QPainterPath crossRectBg1, crossRectBg2;
            crossRectBg1.addRoundedRect(i_cross_r, cornerRadius, cornerRadius);
            // cover left rounded corners
            crossRectBg2.addRect(
                i_cross_r.left(), i_cross_r.bottom(), TAG_CROSS_RADIUS, i_cross_r.top() - i_cross_r.bottom());
            p.fillPath(crossRectBg1, palette.highlight());
            p.fillPath(crossRectBg2, palette.highlight());

            QPen pen = p.pen();
            pen.setWidth(2);
            pen.setBrush(palette.highlightedText());

            p.save();
            p.setPen(pen);
            p.setRenderHint(QPainter::Antialiasing);
            p.drawLine(QLineF(i_cross_r.center() - QPointF(TAG_CROSS_RADIUS, TAG_CROSS_RADIUS),
                              i_cross_r.center() + QPointF(TAG_CROSS_RADIUS, TAG_CROSS_RADIUS)));
            p.drawLine(QLineF(i_cross_r.center() - QPointF(-TAG_CROSS_RADIUS, TAG_CROSS_RADIUS),
                              i_cross_r.center() + QPointF(-TAG_CROSS_RADIUS, TAG_CROSS_RADIUS)));
            p.restore();
        }
    }

    QRect contentsRect() const
    {
        return ifce->viewport()->contentsRect();
    }

    QRect updateTagRenderStates()
    {
        return updateTagRenderStates(contentsRect());
    }

    QRect updateTagRenderStates(QRect r)
    {
        size_t row = 0;
        auto lt = r.topLeft();
        QFontMetrics fm = ifce->fontMetrics();

        for(auto it = std::begin(tags); it != std::end(tags); ++it) {
            updateTagRenderState(lt, row, r, fm, *it, it == tags.editingIndex() && cursorVisible());
        }

        r.setBottom(lt.y() + fm.height() + fm.leading() + TAG_INNER.top() + TAG_INNER.bottom() - 1);
        return r;
    }

    void updateTagRenderState(QPoint& lt, size_t& row, QRect r, QFontMetrics const& fm, const Tag& tag, bool isBeingEdited) const
    {
        // calc text rect
        const auto text_w = fm.horizontalAdvance(tag.text);
        const auto text_h = fm.height() + fm.leading();
        const auto w = (cross_deleter && !isBeingEdited)
                       ? TAG_INNER.left() + TAG_INNER.right() + TAG_CROSS_PADDING * 2 + TAG_CROSS_WIDTH
                       : TAG_INNER.left() + TAG_INNER.right();
        const auto h = TAG_INNER.top() + TAG_INNER.bottom();
        QRect i_r(lt, QSize(text_w + w, text_h + h));

        // line wrapping
        // doesn't fit in current line && doesn't occupy entire line already
        if (r.right() < i_r.right() && i_r.left() != r.left()) {
            i_r.moveTo(r.left(), i_r.bottom() + TAG_V_SPACING);
            ++row;
            lt = i_r.topLeft();
        }

        tag.rect = i_r;
        tag.row = row;
        lt.setX(i_r.right() + TAG_H_SPACING);
    }

    bool cursorVisible() const
    {
        return ifce->cursorVisible();
    }

    void updateDisplayText()
    {
        text_layout.clearLayout();
        text_layout.setText(currentText());
        text_layout.beginLayout();
        text_layout.createLine();
        text_layout.endLayout();
    }

    bool isEmptyTag(const iterator& it) {
        return it->text.trimmed().isEmpty();
    }

    bool isCurrentTagEmpty() {
        return isEmptyTag(tags.editingIndex());
    }

    /// Makes the tag at `i` currently editing, and ensures Invariant-1`.
    void setEditingIndex(const iterator& it)
    {
        tags.setEditingIndex(it);
    }

    void insertText(const QString& text) {
        currentText().insert(cursor, text);
        moveCursor(cursor + text.size(), false);
    }

    void calcRectsAndUpdateScrollRanges()
    {
        const auto row = tags.back().row;
        const auto max_width = std::max_element(std::begin(tags), std::end(tags), [](const auto& x, const auto& y) {
                                   return x.rect.width() < y.rect.width();
                               })->rect.width();

        updateTagRenderStates();

        if (row != tags.back().row) {
            updateVScrollRange();
        }

        const auto new_max_width = std::max_element(std::begin(tags), std::end(tags), [](const auto& x, const auto& y) {
                                       return x.rect.width() < y.rect.width();
                                   })->rect.width();

        if (max_width != new_max_width) {
            updateHScrollRange(new_max_width);
        }
    }

    void setCurrentText(const QString& text)
    {
        Q_ASSERT(tags.editingIndex() != tags.end());
        currentText() = text;
        moveCursor(currentText().length(), false);
        updateDisplayText();
        calcRectsAndUpdateScrollRanges();
        ifce->viewport()->update();
    }

    QString currentText() const
    {
        Q_ASSERT(tags.editingIndex() != tags.end());
        return tags.editingIndex()->text;
    }

    QRect currentRect() const
    {
        Q_ASSERT(tags.editingIndex() != tags.end());
        return tags.editingIndex()->rect;
    }

    // Inserts a new tag at `i`, makes the tag currently editing,
    // and ensures Invariant-1.
    void editNewTag(const iterator& i)
    {
        currentText() = currentText().trimmed();
        auto inserted_at = tags.insert(i, Tag());
        setEditingIndex(inserted_at);
        moveCursor(0, false);
    }

    QVector<QTextLayout::FormatRange> formatting() const
    {
        if (select_size == 0) {
            return {};
        }

        QTextLayout::FormatRange selection;
        selection.start = select_start;
        selection.length = select_size;
        selection.format.setBackground(ifce->palette().brush(QPalette::Highlight));
        selection.format.setForeground(ifce->palette().brush(QPalette::HighlightedText));
        return {selection};
    }

    bool hasSelection() const noexcept
    {
        return select_size > 0;
    }

    void removeSelection()
    {
        cursor = select_start;
        currentText().remove(cursor, select_size);
        deselectAll();
    }

    void removeBackwardOne()
    {
        if (hasSelection()) {
            removeSelection();
        } else {
            currentText().remove(--cursor, 1);
        }
    }

    void selectAll()
    {
        select_start = 0;
        select_size = currentText().size();
    }

    void deselectAll()
    {
        select_start = 0;
        select_size = 0;
    }

    void moveCursor(int pos, bool mark)
    {
        if (mark) {
            auto e = select_start + select_size;
            int anchor = select_size > 0 && cursor == select_start ? e
                         : select_size > 0 && cursor == e          ? select_start
                                                                   : cursor;
            select_start = qMin(anchor, pos);
            select_size = qMax(anchor, pos) - select_start;
        } else {
            deselectAll();
        }

        cursor = pos;
    }

    bool finishTag() {
        // Make existing text into a tag
        if (!isCurrentTagEmpty()) {
            editNewTag(std::next(tags.editingIndex()));
            return true;
        }
        return false;
    }

    qreal cursorToX()
    {
        return text_layout.lineAt(0).cursorToX(cursor);
    }

    void editPreviousTag()
    {
        if (tags.editingIndex() != begin()) {
            setEditingIndex(std::prev(tags.editingIndex()));
            moveCursor(currentText().size(), false);
        }
    }

    template<typename InputIterator> void setTags(InputIterator begin, InputIterator end) {
        cursor = 0;
        select_start = 0;
        select_size = 0;

        tags = TagManager(begin, end);
    }

    void editNextTag(bool add_new = false)
    {
        if (tags.editingIndex() != std::prev(end())) {
            setEditingIndex(std::next(tags.editingIndex()));
            moveCursor(0, false);
        } else if (add_new) {
            editNewTag(std::next(tags.editingIndex()));
        }
    }

    void previousCursorPosition() {
        if (cursor == 0) {
            editPreviousTag();
        } else {
            moveCursor(text_layout.previousCursorPosition(cursor), false);
        }
    }

    void nextCursorPosition() {
        if (cursor == currentText().size()) {
            editNextTag();
        } else {
            moveCursor(text_layout.nextCursorPosition(cursor), false);
        }
    }

    void jumpToFront() {
        if (cursor == 0 && !isBeingEdited(tags.begin())) {
            editTag(tags.begin());
        } else {
            moveCursor(0, false);
        }
    }

    void jumpToBack() {
        if (cursor == currentText().size()) {
            editTag(std::prev(tags.end()));
        } else {
            moveCursor(currentText().size(), false);
        }
    }

    void selectNext() {
        moveCursor(text_layout.nextCursorPosition(cursor), true);
    }

    void selectPrev() {
        moveCursor(text_layout.previousCursorPosition(cursor), true);
    }

    void editTag(const iterator& i)
    {
        setEditingIndex(i);
        moveCursor(currentText().size(), false);
    }

    void removeTag(const iterator& i)
    {
        tags.erase(i);
    }

    void updateVScrollRange()
    {
        auto fm = ifce->fontMetrics();
        const auto row_h = fm.height() + fm.leading() + TAG_INNER.top() + TAG_INNER.bottom() + TAG_V_SPACING;
        ifce->verticalScrollBar()->setPageStep(row_h);
        const auto h = tags.back().rect.bottom() - tags.front().rect.top() + 1;
        const auto contents_rect = contentsRect();
        if (h > contents_rect.height()) {
            ifce->verticalScrollBar()->setRange(0, h - contents_rect.height());
        } else {
            ifce->verticalScrollBar()->setRange(0, 0);
        }
    }

    void updateHScrollRange()
    {
        const auto max_width = std::max_element(std::begin(tags), std::end(tags), [](const auto& x, const auto& y) {
                                   return x.rect.width() < y.rect.width();
                               })->rect.width();
        updateHScrollRange(max_width);
    }

    void updateHScrollRange(int width)
    {
        // TODO Transform to getHScrollRange. Handle in iface
        const auto contents_rect_width = contentsRect().width();
        if (width > contents_rect_width) {
            ifce->horizontalScrollBar()->setRange(0, width - contents_rect_width);
        } else {
            ifce->horizontalScrollBar()->setRange(0, 0);
        }
    }

    void ensureCursorIsVisibleV()
    {
        auto fm = ifce->fontMetrics();
        const auto row_h = fm.height() + fm.leading() + TAG_INNER.top() + TAG_INNER.bottom();
        const auto vscroll = ifce->verticalScrollBar()->value();
        const auto cursor_top = currentRect().topLeft() + QPoint(qRound(cursorToX()), 0);
        const auto cursor_bottom = cursor_top + QPoint(0, row_h - 1);
        const auto contents_rect = contentsRect().translated(0, vscroll);
        if (contents_rect.bottom() < cursor_bottom.y()) {
            ifce->verticalScrollBar()->setValue(cursor_bottom.y() - row_h);
        } else if (cursor_top.y() < contents_rect.top()) {
            ifce->verticalScrollBar()->setValue(cursor_top.y() - 1);
        }
    }

    void ensureCursorIsVisibleH()
    {
        const auto hscroll = ifce->horizontalScrollBar()->value();
        const auto contents_rect = contentsRect().translated(hscroll, 0);
        const auto cursor_x = (currentRect() - TAG_INNER).left() + qRound(cursorToX());
        if (contents_rect.right() < cursor_x) {
            ifce->horizontalScrollBar()->setValue(cursor_x - contents_rect.width());
        } else if (cursor_x < contents_rect.left()) {
            ifce->horizontalScrollBar()->setValue(cursor_x - 1);
        }
    }

private:
    TagsEdit* const ifce;
    TagManager tags;
    int cursor;
    int select_start;
    int select_size;
    bool cross_deleter;
    int hscroll{0};
    QTextLayout text_layout;

public:
    void setReadOnly(bool readOnly) {
        cross_deleter = !readOnly;
    }

    QTextLine lineAt(int i) const
    {
        return text_layout.lineAt(i);
    }

    void paint(QPainter& p, QPointF scrollOffsets, int fontHeight, bool drawCursor)
    { // clip
        const auto rect = contentsRect();
        p.setClipRect(rect);

        for (auto it = std::begin(tags); it != std::end(tags); ++it) {
            if (cursorVisible() && isBeingEdited(it)) {
                const auto r = currentRect();
                const auto txt_p = r.topLeft() + QPoint(TAG_INNER.left(), ((r.height() - fontHeight) / 2));

                // Nothing to draw. Don't draw anything to avoid adding text margins.
                if (!it->isEmpty()) {
                    // draw not terminated tag
                    text_layout.draw(&p, txt_p - scrollOffsets, formatting());
                }

                // draw cursor
                if (drawCursor) {
                    text_layout.drawCursor(&p, txt_p - scrollOffsets, cursor);
                }
            } else if(!it->isEmpty()) {
                drawTag(p, *it);
            }
        }
    }
};

TagsEdit::TagsEdit(QWidget* parent)
    : QAbstractScrollArea(parent)
    , impl(new Impl(this))
    , completer(new QCompleter)
{
    QSizePolicy size_policy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    size_policy.setHeightForWidth(true);
    setSizePolicy(size_policy);

    setFocusPolicy(Qt::StrongFocus);
    viewport()->setCursor(Qt::IBeamCursor);
    setAttribute(Qt::WA_InputMethodEnabled, true);
    setMouseTracking(true);

    setupCompleter();
    setCursorVisible(hasFocus());
    impl->updateDisplayText();

    viewport()->setContentsMargins(TAG_H_SPACING, TAG_V_SPACING, TAG_H_SPACING, TAG_V_SPACING);
}

TagsEdit::~TagsEdit() = default;

void TagsEdit::setReadOnly(bool readOnly)
{
    m_readOnly = readOnly;
    if (m_readOnly) {
        setFocusPolicy(Qt::NoFocus);
        setCursor(Qt::ArrowCursor);
        setAttribute(Qt::WA_InputMethodEnabled, false);
        setFrameShape(QFrame::NoFrame);
    } else {
        setFocusPolicy(Qt::StrongFocus);
        setCursor(Qt::IBeamCursor);
        setAttribute(Qt::WA_InputMethodEnabled, true);
    }
    impl->setReadOnly(m_readOnly);
}

void TagsEdit::resizeEvent(QResizeEvent*)
{
    impl->updateTagRenderStates();
    impl->updateVScrollRange();
    impl->updateHScrollRange();
}

void TagsEdit::focusInEvent(QFocusEvent*)
{
    setCursorVisible(true);
    impl->updateDisplayText();
    impl->updateTagRenderStates();
    completer->complete();
    viewport()->update();
}

void TagsEdit::focusOutEvent(QFocusEvent*)
{
    setCursorVisible(false);
    impl->updateDisplayText();
    impl->updateTagRenderStates();
    completer->popup()->hide();
    viewport()->update();
    // TODO This fixes a bug where an empty tag was shown
    impl->finishTag();
}

void TagsEdit::hideEvent(QHideEvent* event)
{
    Q_UNUSED(event)
    completer->popup()->hide();
}

void TagsEdit::setCursorVisible(bool visible)
{
    if (blink_timer) {
        killTimer(blink_timer);
        blink_timer = 0;
        blink_status = true;
    }

    if (visible) {
        int flashTime = QGuiApplication::styleHints()->cursorFlashTime();
        if (flashTime >= 2) {
            blink_timer = startTimer(flashTime / 2);
        }
    } else {
        blink_status = false;
    }
}

bool TagsEdit::cursorVisible() const
{
    return blink_timer != 0;
}

void TagsEdit::updateCursorBlinking()
{
    setCursorVisible(cursorVisible());
}

void TagsEdit::paintEvent(QPaintEvent*)
{
    QPainter p(viewport());
    QPointF scrollOffsets = QPointF(horizontalScrollBar()->value(), verticalScrollBar()->value());
    const auto fontHeight = fontMetrics().height();

    impl->paint(p, scrollOffsets, fontHeight, blink_status);
}

void TagsEdit::timerEvent(QTimerEvent* event)
{
    if (event->timerId() == blink_timer) {
        blink_status = !blink_status;
        viewport()->update();
    }
}

void TagsEdit::mousePressEvent(QMouseEvent* event)
{
    bool found = false;
    for (auto it = std::begin(*impl); it != std::end(*impl); ++it) {
        if (!it->rect.translated(-horizontalScrollBar()->value(), -verticalScrollBar()->value())
            .contains(event->pos())) {
            continue;
        }

        if (impl->inCrossArea(it, event->pos())) {
            impl->removeTag(it);
            emit tagsEdited();
            found = true;
            // TODO This fixes a bug where the scroll bars were not updated after removing a tag
            event->accept();
            break;
        }

        impl->editTag(it);
        impl->moveCursor(impl->lineAt(0).xToCursor(
                             (event->pos()
                              - impl->currentRect()
                                  .translated(-horizontalScrollBar()->value(), -verticalScrollBar()->value())
                                  .topLeft())
                                 .x()),
                         false);

        found = true;
        break;
    }

    if (!found) {
        for (auto it = std::begin(*impl); it != std::end(*impl); ++it) {
            // Click of a row.
            if (it->rect.translated(-horizontalScrollBar()->value(), -verticalScrollBar()->value()).bottom()
                < event->pos().y()) {
                continue;
            }

            // Last tag of the row.
            const auto row = it->row;
            while (it != std::end(*impl) && it->row == row) {
                ++it;
            }

            impl->editNewTag(it);
            break;
        }

        event->accept();
    }

    if (event->isAccepted()) {
        impl->updateDisplayText();
        impl->calcRectsAndUpdateScrollRanges();
        impl->ensureCursorIsVisibleV();
        impl->ensureCursorIsVisibleH();
        updateCursorBlinking();
        viewport()->update();
    }
}

QSize TagsEdit::sizeHint() const
{
    return minimumSizeHint();
}

QSize TagsEdit::minimumSizeHint() const
{
    ensurePolished();
    QFontMetrics fm = fontMetrics();
    QRect rect(0, 0, fm.maxWidth() + TAG_CROSS_PADDING + TAG_CROSS_WIDTH, fm.height() + fm.leading());
    rect += TAG_INNER + contentsMargins() + viewport()->contentsMargins() + viewportMargins();
    return rect.size();
}

int TagsEdit::heightForWidth(int w) const
{
    const auto content_width = w;
    QRect contents_rect(0, 0, content_width, 100);
    contents_rect -= contentsMargins() + viewport()->contentsMargins() + viewportMargins();
    contents_rect = impl->updateTagRenderStates(contents_rect);
    contents_rect += contentsMargins() + viewport()->contentsMargins() + viewportMargins();
    return contents_rect.height();
}

void TagsEdit::keyPressEvent(QKeyEvent* event)
{
    event->setAccepted(false);
    bool unknown = false;

    if (event == QKeySequence::SelectAll) {
        impl->selectAll();
        event->accept();
    } else if (event == QKeySequence::SelectPreviousChar) {
        impl->selectPrev();
        event->accept();
    } else if (event == QKeySequence::SelectNextChar) {
        impl->selectNext();
        event->accept();
    } else if (event == QKeySequence::Paste) {
        auto clipboard = QApplication::clipboard();
        if (clipboard) {
            for (auto tagtext : clipboard->text().split(",")) {
                impl->insertText(tagtext);
                impl->editNextTag(true);
            }
        }
        event->accept();
    } else {
        switch (event->key()) {
        case Qt::Key_Left:
            impl->previousCursorPosition();
            event->accept();
            break;
        case Qt::Key_Right:
            impl->nextCursorPosition();
            event->accept();
            break;
        case Qt::Key_Home:
            impl->jumpToFront();
            event->accept();
            break;
        case Qt::Key_End:
            impl->jumpToBack();
            event->accept();
            break;
        case Qt::Key_Backspace:
            if (!impl->isCurrentTagEmpty()) {
                impl->removeBackwardOne();
            } else {
                impl->editPreviousTag();
            }
            event->accept();
            break;
        case Qt::Key_Return:
        case Qt::Key_Enter:
        case Qt::Key_Comma:
        case Qt::Key_Semicolon:
            // If completer is visible, accept the selection or hide if no selection
            if (completer->popup()->isVisible() && completer->popup()->selectionModel()->hasSelection()) {
                break;
            }
            // TODO This finishes the tag, but does not split it if the cursor is in the middle of the tag.
            if (impl->finishTag()) {
                // TODO Accept event? Original code did not if tag was empty
                event->accept();
            }
            break;
        default:
            unknown = true;
        }
    }

    if (unknown && isAcceptableInput(event)) {
        if (impl->hasSelection()) {
            impl->removeSelection();
        }
        impl->insertText(event->text());
        event->accept();
    }

    if (event->isAccepted()) {
        // update content
        impl->updateDisplayText();
        impl->calcRectsAndUpdateScrollRanges();
        impl->ensureCursorIsVisibleV();
        impl->ensureCursorIsVisibleH();
        updateCursorBlinking();

        // complete
        completer->setCompletionPrefix(impl->currentText());
        completer->complete();

        viewport()->update();

        emit tagsEdited();
    }
}

void TagsEdit::setCompletion(const QStringList& completions)
{
    completer.reset(new QCompleter(completions));
    setupCompleter();
}

void TagsEdit::setTags(const QStringList& tags)
{
    impl->setTags(tags.begin(), tags.end());

    impl->updateDisplayText();
    impl->calcRectsAndUpdateScrollRanges();
    viewport()->update();
    updateGeometry();
}

QStringList TagsEdit::tags() const
{
    QStringList ret;
    for (const auto& tag : *impl) {
        if (!tag.isEmpty()) {
            ret.push_back(tag.text);
        }
    }
    return ret;
}

void TagsEdit::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_readOnly) {
        for (auto i = std::begin(*impl); i != std::end(*impl); ++i) {
            if (impl->inCrossArea(i, event->pos())) {
                viewport()->setCursor(Qt::ArrowCursor);
                return;
            }
        }
        if (impl->contentsRect().contains(event->pos())) {
            viewport()->setCursor(Qt::IBeamCursor);
        } else {
            QAbstractScrollArea::mouseMoveEvent(event);
        }
    }
}

bool TagsEdit::isAcceptableInput(const QKeyEvent* event) const
{
    const QString text = event->text();
    if (text.isEmpty())
        return false;

    const QChar c = text.at(0);

    // Formatting characters such as ZWNJ, ZWJ, RLM, etc. This needs to go before the
    // next test, since CTRL+SHIFT is sometimes used to input it on Windows.
    if (c.category() == QChar::Other_Format)
        return true;

    // QTBUG-35734: ignore Ctrl/Ctrl+Shift; accept only AltGr (Alt+Ctrl) on German keyboards
    if (event->modifiers() == Qt::ControlModifier || event->modifiers() == (Qt::ShiftModifier | Qt::ControlModifier)) {
        return false;
    }

    if (c.isPrint())
        return true;

    if (c.category() == QChar::Other_PrivateUse)
        return true;

    return false;
}

void TagsEdit::setupCompleter()
{
    completer->setWidget(this);
    connect(completer.get(),
            static_cast<void (QCompleter::*)(QString const&)>(&QCompleter::activated),
            [this](QString const& text) { impl->setCurrentText(text); });
}
