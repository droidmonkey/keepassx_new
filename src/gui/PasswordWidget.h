/*
 *  Copyright (C) 2014 Felix Geyer <debfx@fobos.de>
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

#ifndef KEEPASSX_PASSWORDWIDGET_H
#define KEEPASSX_PASSWORDWIDGET_H

#include <QAction>
#include <QLineEdit>
#include <QPointer>
#include <QWidget>

namespace Ui
{
    class PasswordWidget;
}

class PasswordWidget : public QWidget
{
    Q_OBJECT

    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged USER true)
public:
    explicit PasswordWidget(QWidget* parent = nullptr);
    ~PasswordWidget() override;
    void enablePasswordGenerator();
    void setRepeatPartner(PasswordWidget* repeatPartner);
    void setQualityVisible(bool state);

    bool isPasswordVisible() const;
    QString text();

    bool eventFilter(QObject* watched, QEvent* event) override;

signals:
    void textChanged(QString text);

public slots:
    void setText(const QString& text);
    void setShowPassword(bool show);

    void clear();
    void selectAll();
    void setReadOnly(bool state);
    void setEchoMode(QLineEdit::EchoMode mode);
    void setClearButtonEnabled(bool enabled);

private slots:
    void popupPasswordGenerator();
    void updateRepeatStatus();
    void updatePasswordStrength(const QString& password);

private:
    void checkCapslockState();
    void setParentPasswordEdit(PasswordWidget* parent);

    const QScopedPointer<Ui::PasswordWidget> m_ui;

    QPointer<QAction> m_errorAction;
    QPointer<QAction> m_correctAction;
    QPointer<QAction> m_toggleVisibleAction;
    QPointer<QAction> m_passwordGeneratorAction;
    QPointer<QAction> m_capslockAction;
    QPointer<PasswordWidget> m_repeatPasswordWidget;
    QPointer<PasswordWidget> m_parentPasswordWidget;

    bool m_capslockState = false;
};

#endif // KEEPASSX_PASSWORDWIDGET_H
