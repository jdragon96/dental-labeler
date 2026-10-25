#include "UI/Common/LabelKeyInput.h"

#include <QAbstractSpinBox>
#include <QApplication>
#include <QComboBox>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTextEdit>

namespace
{
constexpr int kSecondDigitTimeoutMs = 800;

bool IsTextInputFocused()
{
    QWidget* pFocus = QApplication::focusWidget();
    if (pFocus == nullptr)
    {
        return false;
    }
    if (qobject_cast<QLineEdit*>(pFocus) != nullptr || qobject_cast<QTextEdit*>(pFocus) != nullptr
        || qobject_cast<QPlainTextEdit*>(pFocus) != nullptr || qobject_cast<QAbstractSpinBox*>(pFocus) != nullptr)
    {
        return true;
    }
    QComboBox* pCombo = qobject_cast<QComboBox*>(pFocus);
    return pCombo != nullptr && pCombo->isEditable();
}
}

LabelKeyInput::LabelKeyInput(QObject* parent)
    : QObject(parent)
    , m_strPending()
    , m_timerTimeout()
{
    m_timerTimeout.setSingleShot(true);
    m_timerTimeout.setInterval(kSecondDigitTimeoutMs);
    connect(&m_timerTimeout, &QTimer::timeout, this, &LabelKeyInput::Clear);
}

bool LabelKeyInput::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() != QEvent::KeyPress || !watched->isWidgetType())
    {
        return QObject::eventFilter(watched, event);
    }

    QKeyEvent* pKeyEvent = static_cast<QKeyEvent*>(event);
    const Qt::KeyboardModifiers modifiers = pKeyEvent->modifiers() & ~Qt::KeypadModifier;
    const int iKey = pKeyEvent->key();
    if (iKey < Qt::Key_0 || iKey > Qt::Key_9 || modifiers != Qt::NoModifier || pKeyEvent->isAutoRepeat()
        || QApplication::activeModalWidget() != nullptr || IsTextInputFocused())
    {
        return QObject::eventFilter(watched, event);
    }

    const int iDigit = iKey - Qt::Key_0;
    if (m_strPending.isEmpty())
    {
        if (iDigit == 0)
        {
            emit LabelEntered(0);
            return true;
        }
        m_strPending = QString::number(iDigit);
        m_timerTimeout.start();
        emit PendingChanged(m_strPending);
        return true;
    }

    const int iLabel = m_strPending.toInt() * 10 + iDigit;
    Clear();
    emit LabelEntered(iLabel);
    return true;
}

void LabelKeyInput::Clear()
{
    m_timerTimeout.stop();
    if (m_strPending.isEmpty())
    {
        return;
    }
    m_strPending.clear();
    emit PendingChanged(QString());
}
