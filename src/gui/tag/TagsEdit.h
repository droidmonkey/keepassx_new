/*
  MIT License

  Copyright (c) 2019 Nicolai Trandafil

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

#pragma once

#include <QAbstractScrollArea>
#include <QScopedPointer>

class QCompleter;

class TagsEdit : public QAbstractScrollArea
{
    Q_OBJECT

public:
    explicit TagsEdit(QWidget* parent = nullptr);
    ~TagsEdit() override;

    bool hasHeightForWidth() const override;
    int heightForWidth(int w) const override;

    void setReadOnly(bool readOnly);
    void setCompletion(const QStringList& completions);
    void setTags(const QStringList& tags);

    QStringList tags() const;

signals:
    void tagsEdited();

protected:
    void paintEvent(QPaintEvent* event) override;
    void timerEvent(QTimerEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    QSize viewportSizeHint() const override;

private:
    bool isAcceptableInput(QKeyEvent const* event) const;
    void setupCompleter();
    void setCursorVisible(bool visible);
    bool cursorVisible() const;
    void updateCursorBlinking();

    struct Impl;
    QScopedPointer<Impl> impl;
    QScopedPointer<QCompleter> completer;

    bool m_readOnly = false;
    int blink_timer = 0;
    bool blink_status = true;
};
