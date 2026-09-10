// A line-preserving reader and writer for the strikers.ini dialect src/platform/config.c parses.

// config.c applies each key with port_setenv_default, so the FIRST occurrence of a key wins.

#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVector>

class IniFile
{
public:
    // A missing file is a failure here; the caller decides whether that matters.
    bool load(const QString& path, QString* error);

    void clear();

    // Through a temporary file and a rename, so an interrupted save leaves the original.
    bool save(const QString& path, QString* error) const;

    QByteArray render() const;

    // Upper case with `STRIKERS_` stripped, so res_scale and STRIKERS_res_scale are one key.
    static QString normalise(const QString& key);

    // True if the key has a live (uncommented) entry.
    bool has(const QString& key) const;

    QString value(const QString& key, const QString& fallback = QString()) const;

    // Updates the first live entry, else uncomments one, else appends under [section].
    void set(const QString& key, const QString& value, const QString& section);

    // Comments the entry out again, restoring a loaded line to its original text.
    void unset(const QString& key);

    void remove(const QString& key);

    // Normalised keys with a live entry, in file order.
    QStringList liveKeys() const;

    // Normalised keys that appear only as a commented-out declaration, in file order.
    QStringList commentedKeys() const;

    // The section a key's line sits in, live or commented, or an empty string.
    QString sectionOf(const QString& key) const;

    QString commentedValue(const QString& key) const;

private:
    struct Line
    {
        QString orig;      // exactly as read, without its ending
        QString text;      // current text, without its ending
        QString ending;    // "\n", "\r\n", or "" on a final line with none
        QString section;   // section in force on this line, lower case

        bool entry = false;      // parses as key = value
        bool commented = false;  // ... but behind a # or ;
        bool origCommented = false;

        QString key;      // normalised
        QString indent;   // leading whitespace, after any comment mark
        QString rawKey;   // as spelled in the file
        QString preEq;    // whitespace before '='
        QString postEq;   // whitespace after '='
        QString value;    // with quotes stripped
        bool quoted = false;
        QString origValue;   // as loaded, so a value set back to it can keep the file's bytes
        bool origQuoted = false;
    };

    void parse(const QByteArray& bytes);
    int findLive(const QString& normKey) const;
    int findCommented(const QString& normKey) const;
    void rebuild(Line& line) const;
    int insertionPointFor(const QString& section);

    QVector<Line> m_lines;
};
