#include "keycapturebutton.h"

#include "keynames.h"

#include <QEvent>
#include <QKeyEvent>
#include <QToolTip>

KeyCaptureButton::KeyCaptureButton(QWidget* parent)
    : QPushButton(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMinimumWidth(120);
    connect(this, &QPushButton::clicked, this, [this] {
        if (!m_capturing)
            beginCapture();
    });
    refresh();
}

void KeyCaptureButton::setBinding(const QString& sdlName)
{
    if (m_binding == sdlName)
        return;
    m_binding = sdlName;
    refresh();
}

void KeyCaptureButton::setConflict(bool conflict)
{
    if (m_conflict == conflict)
        return;
    m_conflict = conflict;
    refresh();
}

void KeyCaptureButton::refresh()
{
    if (m_capturing)
    {
        setText(tr("press a key…"));
        setStyleSheet(QStringLiteral("font-style: italic;"));
        return;
    }

    setText(m_binding.isEmpty() ? tr("(unbound)") : m_binding);

    // One red for both themes: Qt has no palette role for "wrong".
    if (m_conflict)
        setStyleSheet(QStringLiteral("color: #d64545; font-weight: bold;"));
    else if (!m_binding.isEmpty() && !KeyNames::isKnown(m_binding))
        setStyleSheet(QStringLiteral("color: #c08a2e;")); // hand-edited, unrecognised
    else
        setStyleSheet(QString());
}

void KeyCaptureButton::beginCapture()
{
    m_capturing = true;
    grabKeyboard();
    refresh();
}

void KeyCaptureButton::endCapture()
{
    if (!m_capturing)
        return;
    m_capturing = false;
    releaseKeyboard();
    refresh();
}

void KeyCaptureButton::focusOutEvent(QFocusEvent* e)
{
    endCapture();
    QPushButton::focusOutEvent(e);
}

bool KeyCaptureButton::event(QEvent* e)
{
    if (!m_capturing)
        return QPushButton::event(e);

    // ShortcutOverride is the only chance to stop a key going to a menu accelerator.
    if (e->type() == QEvent::ShortcutOverride)
    {
        e->accept();
        return true;
    }

    if (e->type() == QEvent::KeyPress)
    {
        auto* ke = static_cast<QKeyEvent*>(e);
        const int key = ke->key();

        if (key == Qt::Key_Escape)
        {
            endCapture();
            return true;
        }

        // A bare modifier is not a binding, so keep waiting.
        if (key == Qt::Key_Shift || key == Qt::Key_Control || key == Qt::Key_Alt ||
            key == Qt::Key_Meta || key == Qt::Key_AltGr || key == Qt::Key_Super_L ||
            key == Qt::Key_Super_R || key == Qt::Key_unknown)
            return true;

        const bool keypad = ke->modifiers().testFlag(Qt::KeypadModifier);
        const QString name = KeyNames::fromQt(key, keypad);
        if (name.isEmpty())
        {
            QToolTip::showText(mapToGlobal(rect().bottomLeft()),
                               tr("No SDL scancode for that key. Try another."),
                               this);
            return true;
        }

        endCapture();
        if (m_binding != name)
        {
            m_binding = name;
            refresh();
            emit bindingChanged(m_binding);
        }
        return true;
    }

    if (e->type() == QEvent::KeyRelease)
        return true;

    return QPushButton::event(e);
}
