// The text behind a setting's info button, and the only place it is built.

#pragma once

#include <QString>

struct Setting;

namespace HelpText {

// Rich text for the popover and for the hover tooltip.
QString popover(const Setting& s, bool technical = false);

// The href the "Technical details" link carries, so the panel can recognise it.
const char* technicalLink();

// The environment spelling of a key, as config.c would look it up.
QString environmentName(const QString& key);

} // namespace HelpText
