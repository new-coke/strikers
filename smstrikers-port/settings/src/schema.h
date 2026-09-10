// Every key the settings window knows about, with its default and the wording that explains it.

#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

struct Setting
{
    QString key;      // canonical spelling, as written into the file
    QString section;  // ini section to append under, if the key is new
    QString label;    // the left column, in a player's words
    QString def;      // "" == leave unset
    QString check;    // a switch's own text, e.g. "Play with the keyboard"
    QString detail;   // one plain sentence, the top of the info popover
    QString help;     // verbatim from strikers.ini.example
    QStringList values;       // a choice's ini values, in the order offered
    QStringList valueLabels;  // parallel, in a player's words

    // True when `help` is the example file's own English wording rather than something this
    // application wrote.
    bool helpIsVerbatim = false;
};

struct Binding
{
    QString key;
    QString section;
    QString label;  // an identifier, not display text; see bindingLabel()
    QString def;
    QString group;  // which box on the controller layout it belongs in
};

namespace Schema {

// Lookup by any spelling of the key (case-insensitive, prefix optional).
const Setting& get(const QString& key);

const QVector<Setting>& display();
const QVector<Setting>& audio();
const QVector<Setting>& game();

// keyboard=1 and the pad's scalar settings are Settings; the per-button bindings are Bindings,
// because there are 36 of them and they are uniform.
const QVector<Setting>& inputSwitches();
const QVector<Binding>& keyboardBindings();
const QVector<Binding>& gamepadBindings();

// Every Setting the window shows, in one list, for a test that has to hold all of them to the same
// rule.
QVector<Setting> all();

// SDL gamepad button names, in the order SDL lists them.
const QStringList& padButtonNames();

// True if a tab owns this key, i.e. the advanced table should not show it.
bool isOwned(const QString& key);

// The title of the box a Binding::group names.
QString groupLabel(const QString& group);

// The row label for a Binding. `A`, `Start` and `L` are what is printed on a GameCube pad and come
// back unchanged in every language; `Up` and its three neighbours are directions and do not.
QString bindingLabel(const QString& label);

// Build the tables again in the language that is installed now.
void retranslate();

} // namespace Schema
