#include "inifile.h"

#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

namespace {

bool isKeyChar(QChar c)
{
    return c.isLetterOrNumber() || c == QLatin1Char('_') || c == QLatin1Char('.') ||
           c == QLatin1Char('-');
}

// A key starts with a letter or an underscore, or prose containing '=' reads as a setting.
bool looksLikeKey(const QString& key)
{
    if (key.isEmpty())
        return false;
    const QChar first = key.at(0);
    if (!first.isLetter() && first != QLatin1Char('_'))
        return false;
    for (const QChar c : key)
    {
        if (!isKeyChar(c))
            return false;
    }
    return true;
}

QString stripQuotes(const QString& in, bool* wasQuoted)
{
    *wasQuoted = false;
    if (in.size() >= 2)
    {
        const QChar a = in.at(0);
        const QChar b = in.at(in.size() - 1);
        if ((a == QLatin1Char('"') && b == QLatin1Char('"')) ||
            (a == QLatin1Char('\'') && b == QLatin1Char('\'')))
        {
            *wasQuoted = true;
            return in.mid(1, in.size() - 2);
        }
    }
    return in;
}

} // namespace

QString IniFile::normalise(const QString& key)
{
    QString k = key.trimmed().toUpper();
    if (k.startsWith(QLatin1String("STRIKERS_")))
        k = k.mid(9);
    return k;
}

void IniFile::clear() { m_lines.clear(); }

bool IniFile::load(const QString& path, QString* error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
    {
        if (error)
            *error = QStringLiteral("%1: %2").arg(QFileInfo(path).fileName(), f.errorString());
        return false;
    }
    parse(f.readAll());
    return true;
}

void IniFile::parse(const QByteArray& bytes)
{
    m_lines.clear();

    // UTF-8, so a comment the user typed survives the round trip.
    const QString all = QString::fromUtf8(bytes);
    QString section;

    int i = 0;
    while (i < all.size())
    {
        int nl = all.indexOf(QLatin1Char('\n'), i);
        QString raw;
        QString ending;
        if (nl < 0)
        {
            raw = all.mid(i);
            i = all.size();
        }
        else
        {
            raw = all.mid(i, nl - i);
            ending = QStringLiteral("\n");
            if (raw.endsWith(QLatin1Char('\r')))
            {
                raw.chop(1);
                ending = QStringLiteral("\r\n");
            }
            i = nl + 1;
        }

        Line line;
        line.orig = raw;
        line.text = raw;
        line.ending = ending;

        QString body = raw;
        bool commented = false;

        const QString trimmedFull = raw.trimmed();
        if (trimmedFull.startsWith(QLatin1Char('[')))
        {
            const int close = trimmedFull.indexOf(QLatin1Char(']'));
            if (close > 0)
                section = trimmedFull.mid(1, close - 1).trimmed().toLower();
            line.section = section;
            m_lines.push_back(line);
            continue;
        }

        if (trimmedFull.startsWith(QLatin1Char('#')) || trimmedFull.startsWith(QLatin1Char(';')))
        {
            // A switched-off setting is one comment mark, at most one space, inside a section.
            const int mark = raw.indexOf(trimmedFull.at(0));
            QString rest = raw.mid(mark + 1);
            int spaces = 0;
            while (spaces < rest.size() && rest.at(spaces) == QLatin1Char(' '))
                spaces++;
            if (spaces > 1 || section.isEmpty())
            {
                line.section = section;
                m_lines.push_back(line);
                continue;
            }
            commented = true;
            // rebuild() puts "# " back, so drop the space here.
            body = rest.mid(spaces);
        }

        const int eq = body.indexOf(QLatin1Char('='));
        if (eq < 0)
        {
            line.section = section;
            m_lines.push_back(line);
            continue;
        }

        const QString lhs = body.left(eq);
        const QString rhs = body.mid(eq + 1);
        const QString key = lhs.trimmed();
        if (!looksLikeKey(key))
        {
            line.section = section;
            m_lines.push_back(line);
            continue;
        }

        int lead = 0;
        while (lead < lhs.size() && (lhs.at(lead) == QLatin1Char(' ') || lhs.at(lead) == QLatin1Char('\t')))
            lead++;

        line.entry = true;
        line.commented = commented;
        line.origCommented = commented;
        line.section = section;
        line.key = normalise(key);
        line.rawKey = key;
        line.indent = lhs.left(lead);
        line.preEq = lhs.mid(lead + key.size());
        int post = 0;
        while (post < rhs.size() && (rhs.at(post) == QLatin1Char(' ') || rhs.at(post) == QLatin1Char('\t')))
            post++;
        line.postEq = rhs.left(post);
        bool quoted = false;
        line.value = stripQuotes(rhs.trimmed(), &quoted);
        line.quoted = quoted;
        line.origValue = line.value;
        line.origQuoted = quoted;

        m_lines.push_back(line);
    }
}

int IniFile::findLive(const QString& normKey) const
{
    for (int i = 0; i < m_lines.size(); ++i)
    {
        if (m_lines[i].entry && !m_lines[i].commented && m_lines[i].key == normKey)
            return i;
    }
    return -1;
}

int IniFile::findCommented(const QString& normKey) const
{
    for (int i = 0; i < m_lines.size(); ++i)
    {
        if (m_lines[i].entry && m_lines[i].commented && m_lines[i].key == normKey)
            return i;
    }
    return -1;
}

bool IniFile::has(const QString& key) const { return findLive(normalise(key)) >= 0; }

QString IniFile::value(const QString& key, const QString& fallback) const
{
    const int i = findLive(normalise(key));
    return i < 0 ? fallback : m_lines[i].value;
}

QString IniFile::commentedValue(const QString& key) const
{
    const int i = findCommented(normalise(key));
    return i < 0 ? QString() : m_lines[i].value;
}

QString IniFile::sectionOf(const QString& key) const
{
    const QString n = normalise(key);
    int i = findLive(n);
    if (i < 0)
        i = findCommented(n);
    return i < 0 ? QString() : m_lines[i].section;
}

QStringList IniFile::liveKeys() const
{
    QStringList out;
    for (const Line& l : m_lines)
    {
        if (l.entry && !l.commented && !out.contains(l.key))
            out << l.key;
    }
    return out;
}

QStringList IniFile::commentedKeys() const
{
    QStringList out;
    const QStringList live = liveKeys();
    for (const Line& l : m_lines)
    {
        if (l.entry && l.commented && !live.contains(l.key) && !out.contains(l.key))
            out << l.key;
    }
    return out;
}

void IniFile::rebuild(Line& line) const
{
    // config.c strips one layer of matching quotes, so a value with edge spaces is quoted.
    QString v = line.value;
    const bool needsQuotes =
        line.quoted || v != v.trimmed() ||
        (!v.isEmpty() && (v.startsWith(QLatin1Char('"')) || v.startsWith(QLatin1Char('\''))));
    if (needsQuotes)
        v = QLatin1Char('"') + v + QLatin1Char('"');

    // Spacing around the '=' is the file's; only a line invented here gets the house style.
    const bool invented = line.orig.isEmpty();
    const QString preEq = invented && line.preEq.isEmpty() ? QStringLiteral(" ") : line.preEq;
    const QString postEq = invented && line.postEq.isEmpty() ? QStringLiteral(" ") : line.postEq;
    QString body = line.indent + line.rawKey + preEq + QLatin1Char('=') + postEq + v;
    line.text = line.commented ? (QStringLiteral("# ") + body) : body;
}

int IniFile::insertionPointFor(const QString& section)
{
    const QString want = section.toLower();

    int last = -1;
    for (int i = 0; i < m_lines.size(); ++i)
    {
        if (m_lines[i].section == want)
            last = i;
    }

    if (last >= 0)
    {
        // Step back over trailing blank lines so the key joins the section's block.
        while (last >= 0 && m_lines[last].text.trimmed().isEmpty())
            last--;
        return last + 1;
    }

    // No such section, so add one at the end.
    if (!m_lines.isEmpty())
    {
        Line& tail = m_lines.back();
        if (tail.ending.isEmpty())
            tail.ending = QStringLiteral("\n");
        if (!tail.text.trimmed().isEmpty())
        {
            Line blank;
            blank.ending = QStringLiteral("\n");
            blank.section = m_lines.back().section;
            m_lines.push_back(blank);
        }
    }
    Line header;
    header.text = QLatin1Char('[') + want + QLatin1Char(']');
    header.orig = header.text;
    header.ending = QStringLiteral("\n");
    header.section = want;
    m_lines.push_back(header);
    return m_lines.size();
}

void IniFile::set(const QString& key, const QString& value, const QString& section)
{
    const QString n = normalise(key);

    int i = findLive(n);
    if (i >= 0)
    {
        Line& l = m_lines[i];
        if (l.value == value)
        {
            // Nothing to change; the text stays as it is, whatever an earlier set() made it.
            return;
        }
        if (!l.origCommented && value == l.origValue)
        {
            // Back to what the file said: keep the file's own bytes for it.
            l.value = l.origValue;
            l.quoted = l.origQuoted;
            l.text = l.orig;
            return;
        }
        l.value = value;
        rebuild(l);
        return;
    }

    i = findCommented(n);
    if (i >= 0)
    {
        Line& l = m_lines[i];
        l.commented = false;
        l.value = value;
        rebuild(l);
        return;
    }

    const QString want = section.isEmpty() ? QStringLiteral("misc") : section.toLower();
    const int at = insertionPointFor(want);
    Line l;
    l.entry = true;
    l.section = want;
    l.key = n;
    l.rawKey = key.trimmed().toLower();
    if (l.rawKey.startsWith(QLatin1String("strikers_")))
        l.rawKey = l.rawKey.mid(9);
    l.preEq = QStringLiteral(" ");
    l.postEq = QStringLiteral(" ");
    l.value = value;
    l.ending = QStringLiteral("\n");
    rebuild(l);
    m_lines.insert(at, l);

    // A file that ended without a newline needs one before the appended key.
    if (at > 0 && m_lines[at - 1].ending.isEmpty())
        m_lines[at - 1].ending = QStringLiteral("\n");
}

void IniFile::unset(const QString& key)
{
    const QString n = normalise(key);
    // Every live occurrence: config.c takes the first live line for a key, so one left live takes over.
    for (int i = 0; i < m_lines.size(); ++i)
    {
        Line& l = m_lines[i];
        if (!l.entry || l.commented || l.key != n)
            continue;
        if (l.origCommented)
        {
            // A commented declaration from the file goes back exactly as it was.
            l.value = l.origValue;
            l.quoted = l.origQuoted;
            l.commented = true;
            l.text = l.orig;
            continue;
        }
        l.commented = true;
        rebuild(l);
    }
}

void IniFile::remove(const QString& key)
{
    const QString n = normalise(key);
    for (int i = m_lines.size() - 1; i >= 0; --i)
    {
        if (m_lines[i].entry && m_lines[i].key == n)
            m_lines.remove(i);
    }
}

QByteArray IniFile::render() const
{
    QString out;
    for (const Line& l : m_lines)
    {
        out += l.text;
        out += l.ending;
    }
    return out.toUtf8();
}

bool IniFile::save(const QString& path, QString* error) const
{
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        if (error)
            *error = QStringLiteral("%1: %2").arg(path, f.errorString());
        return false;
    }
    const QByteArray bytes = render();
    if (f.write(bytes) != bytes.size() || !f.commit())
    {
        if (error)
            *error = QStringLiteral("%1: %2").arg(path, f.errorString());
        return false;
    }
    return true;
}
