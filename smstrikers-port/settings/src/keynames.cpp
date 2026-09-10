#include "keynames.h"

#include <QHash>
#include <Qt>

namespace {

struct Entry
{
    int qt;
    const char* sdl;
};

// Spelled as SDL's scancode_names table does: "PageUp", "Left Shift", "Keypad 0".
const Entry kTable[] = {
    { Qt::Key_A, "A" }, { Qt::Key_B, "B" }, { Qt::Key_C, "C" }, { Qt::Key_D, "D" },
    { Qt::Key_E, "E" }, { Qt::Key_F, "F" }, { Qt::Key_G, "G" }, { Qt::Key_H, "H" },
    { Qt::Key_I, "I" }, { Qt::Key_J, "J" }, { Qt::Key_K, "K" }, { Qt::Key_L, "L" },
    { Qt::Key_M, "M" }, { Qt::Key_N, "N" }, { Qt::Key_O, "O" }, { Qt::Key_P, "P" },
    { Qt::Key_Q, "Q" }, { Qt::Key_R, "R" }, { Qt::Key_S, "S" }, { Qt::Key_T, "T" },
    { Qt::Key_U, "U" }, { Qt::Key_V, "V" }, { Qt::Key_W, "W" }, { Qt::Key_X, "X" },
    { Qt::Key_Y, "Y" }, { Qt::Key_Z, "Z" },

    { Qt::Key_0, "0" }, { Qt::Key_1, "1" }, { Qt::Key_2, "2" }, { Qt::Key_3, "3" },
    { Qt::Key_4, "4" }, { Qt::Key_5, "5" }, { Qt::Key_6, "6" }, { Qt::Key_7, "7" },
    { Qt::Key_8, "8" }, { Qt::Key_9, "9" },

    { Qt::Key_Return, "Return" },
    { Qt::Key_Enter, "Return" },   // main-block Enter only; the keypad one is
                                   // handled by the keypad branch below
    { Qt::Key_Escape, "Escape" },
    { Qt::Key_Backspace, "Backspace" },
    { Qt::Key_Tab, "Tab" },
    { Qt::Key_Space, "Space" },

    { Qt::Key_Left, "Left" },
    { Qt::Key_Right, "Right" },
    { Qt::Key_Up, "Up" },
    { Qt::Key_Down, "Down" },

    { Qt::Key_Insert, "Insert" },
    { Qt::Key_Delete, "Delete" },
    { Qt::Key_Home, "Home" },
    { Qt::Key_End, "End" },
    { Qt::Key_PageUp, "PageUp" },
    { Qt::Key_PageDown, "PageDown" },
    { Qt::Key_CapsLock, "CapsLock" },
    { Qt::Key_Print, "PrintScreen" },
    { Qt::Key_ScrollLock, "ScrollLock" },
    { Qt::Key_Pause, "Pause" },

    { Qt::Key_F1, "F1" },   { Qt::Key_F2, "F2" },   { Qt::Key_F3, "F3" },
    { Qt::Key_F4, "F4" },   { Qt::Key_F5, "F5" },   { Qt::Key_F6, "F6" },
    { Qt::Key_F7, "F7" },   { Qt::Key_F8, "F8" },   { Qt::Key_F9, "F9" },
    { Qt::Key_F10, "F10" }, { Qt::Key_F11, "F11" }, { Qt::Key_F12, "F12" },

    { Qt::Key_Minus, "-" },
    { Qt::Key_Equal, "=" },
    { Qt::Key_BracketLeft, "[" },
    { Qt::Key_BracketRight, "]" },
    { Qt::Key_Backslash, "\\" },
    { Qt::Key_Semicolon, ";" },
    { Qt::Key_Apostrophe, "'" },
    { Qt::Key_Comma, "," },
    { Qt::Key_Period, "." },
    { Qt::Key_Slash, "/" },
    { Qt::Key_QuoteLeft, "`" },
};

// Qt reports both shifts as Qt::Key_Shift with no side, so a capture cannot tell them apart.
const char* const kUncapturable[] = {
    "Left Shift", "Right Shift", "Left Ctrl", "Right Ctrl", "Left Alt", "Right Alt",
};

const char* const kKeypad[] = {
    "Keypad 0", "Keypad 1", "Keypad 2", "Keypad 3", "Keypad 4",
    "Keypad 5", "Keypad 6", "Keypad 7", "Keypad 8", "Keypad 9",
    "Keypad Enter", "Keypad .", "Keypad +", "Keypad -", "Keypad *", "Keypad /",
};

} // namespace

QString KeyNames::fromQt(int qtKey, bool keypad)
{
    if (keypad)
    {
        if (qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9)
            return QStringLiteral("Keypad %1").arg(qtKey - Qt::Key_0);
        switch (qtKey)
        {
        case Qt::Key_Enter:
        case Qt::Key_Return: return QStringLiteral("Keypad Enter");
        case Qt::Key_Period: return QStringLiteral("Keypad .");
        case Qt::Key_Plus:   return QStringLiteral("Keypad +");
        case Qt::Key_Minus:  return QStringLiteral("Keypad -");
        case Qt::Key_Asterisk: return QStringLiteral("Keypad *");
        case Qt::Key_Slash:  return QStringLiteral("Keypad /");
        default: break;
        }
        // With numlock off the keypad reports Home/End/arrows with the flag still set.
        return QString();
    }

    for (const Entry& e : kTable)
    {
        if (e.qt == qtKey)
            return QString::fromLatin1(e.sdl);
    }
    return QString();
}

const QStringList& KeyNames::all()
{
    static QStringList names = [] {
        QStringList out;
        for (const Entry& e : kTable)
        {
            const QString n = QString::fromLatin1(e.sdl);
            if (!out.contains(n))
                out << n;
        }
        for (const char* n : kUncapturable)
            out << QString::fromLatin1(n);
        for (const char* n : kKeypad)
            out << QString::fromLatin1(n);
        return out;
    }();
    return names;
}

bool KeyNames::isKnown(const QString& sdlName)
{
    return all().contains(sdlName, Qt::CaseSensitive);
}
