// Qt key events to SDL scancode names and back; the game stores the name SDL prints.

// SDL scancodes are physical and Qt keys are layout-mapped, so they differ on AZERTY.

#pragma once

#include <QString>
#include <QStringList>

namespace KeyNames {

// The SDL name for a Qt key, or an empty string. `keypad` is Qt::KeypadModifier.
QString fromQt(int qtKey, bool keypad);

// True if this is a name the table can produce, so a hand-edited binding can be flagged.
bool isKnown(const QString& sdlName);

const QStringList& all();

} // namespace KeyNames
