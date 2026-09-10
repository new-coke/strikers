#include "helptext.h"

#include "inifile.h"
#include "schema.h"

#include <QCoreApplication>
#include <QStringList>

namespace {

// A context of its own rather than QObject's, so the four strings the popover frame is made of sit
// together in the .ts file.
class Text
{
    Q_DECLARE_TR_FUNCTIONS(HelpText)
};

// The example file's wording is hard-wrapped at 78 columns and mostly prose, with the occasional
// indented table in it.
bool isTable(const QStringList& lines)
{
    for (const QString& l : lines)
    {
        if (!l.startsWith(QLatin1String("  ")))
            return false;
    }
    return !lines.isEmpty();
}

QString paragraphs(const QString& text)
{
    QString html;
    const QStringList blocks = text.split(QLatin1String("\n\n"));
    for (const QString& block : blocks)
    {
        const QStringList lines = block.split(QLatin1Char('\n'));
        if (isTable(lines))
        {
            QString pre = block.toHtmlEscaped();
            pre.replace(QLatin1Char('\n'), QLatin1String("<br>"));
            pre.replace(QLatin1Char(' '), QLatin1String("&nbsp;"));
            html += QStringLiteral("<div style='margin:0 0 8px 0'>%1</div>").arg(pre);
        }
        else
        {
            QString flowed = block;
            flowed.replace(QLatin1Char('\n'), QLatin1Char(' '));
            html += QStringLiteral("<p style='margin:0 0 8px 0'>%1</p>")
                        .arg(flowed.simplified().toHtmlEscaped());
        }
    }
    return html;
}

} // namespace

QString HelpText::environmentName(const QString& key)
{
    return QStringLiteral("STRIKERS_") + IniFile::normalise(key);
}

const char* HelpText::technicalLink()
{
    return "strikers:technical";
}

QString HelpText::popover(const Setting& s, bool technical)
{
    QString html;

    if (!s.detail.isEmpty())
        html += QStringLiteral("<p style='margin:0 0 10px 0'>%1</p>").arg(s.detail.toHtmlEscaped());

    if (!s.help.isEmpty())
    {
        if (technical)
        {
            // A line naming what follows, and the reason it exists is the translation.
            if (s.helpIsVerbatim)
            {
                html += QStringLiteral("<p style='margin:0 0 8px 0'><i>%1</i></p>")
                            .arg(Text::tr("The setting's own documentation, as shipped "
                                          "with the game:")
                                     .toHtmlEscaped());
            }
            html += paragraphs(s.help);
        }
        else
        {
            html += QStringLiteral("<p style='margin:0'><a href='%1'>%2</a></p>")
                        .arg(QString::fromLatin1(technicalLink()),
                             Text::tr("Technical details"));
        }
    }

    if (!s.key.isEmpty())
    {
        // Named once, at the bottom, for the reader who is editing strikers.ini by hand or setting
        // a variable for one run.
        html += QStringLiteral("<p style='margin:14px 0 0 0'><i>%1</i></p>")
                    .arg(Text::tr("In strikers.ini: <b>%1</b> &nbsp;&nbsp; "
                                  "As a variable: <b>%2</b>")
                             .arg(s.key.toHtmlEscaped(), environmentName(s.key)));
    }

    return html;
}
