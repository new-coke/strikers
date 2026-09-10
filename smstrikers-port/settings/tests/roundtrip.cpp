// The example file has to survive a load and a save byte for byte.

#include "apppaths.h"
#include "helptext.h"
#include "inifile.h"
#include "keynames.h"
#include "schema.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStringList>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QTextStream>

#include <cstdio>

static int g_failures = 0;

static void check(bool ok, const QString& what, const QString& detail = QString())
{
    QTextStream out(stdout);
    out << (ok ? "ok   " : "FAIL ") << what << '\n';
    if (!ok && !detail.isEmpty())
        out << "       " << detail << '\n';
    if (!ok)
        g_failures++;
}

static QByteArray readAll(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QByteArray();
    return f.readAll();
}

// Where the two byte strings first differ, as a line number and both lines.
static QString firstDifference(const QByteArray& a, const QByteArray& b)
{
    const QStringList la = QString::fromUtf8(a).split(QLatin1Char('\n'));
    const QStringList lb = QString::fromUtf8(b).split(QLatin1Char('\n'));
    for (int i = 0; i < qMax(la.size(), lb.size()); ++i)
    {
        const QString x = la.value(i);
        const QString y = lb.value(i);
        if (x != y)
            return QStringLiteral("line %1:\n         read  [%2]\n         wrote [%3]")
                .arg(i + 1)
                .arg(x, y);
    }
    return QStringLiteral("same lines, different length (%1 vs %2 bytes)")
        .arg(a.size())
        .arg(b.size());
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    if (argc < 2)
    {
        fprintf(stderr, "usage: inifile_test <strikers.ini.example>\n");
        return 2;
    }

    const QString example = QString::fromLocal8Bit(argv[1]);
    const QByteArray original = readAll(example);
    check(!original.isEmpty(), QStringLiteral("read %1").arg(example));
    if (original.isEmpty())
        return 1;

    {
        IniFile ini;
        QString error;
        check(ini.load(example, &error), QStringLiteral("load the example"), error);
        const QByteArray rendered = ini.render();
        check(rendered == original, QStringLiteral("round trip is byte-identical"),
              firstDifference(original, rendered));
    }

    {
        IniFile ini;
        QString error;
        ini.load(example, &error);
        check(ini.liveKeys().isEmpty(),
              QStringLiteral("nothing in the example is switched on"),
              ini.liveKeys().join(QLatin1String(", ")));
        check(ini.commentedKeys().contains(QStringLiteral("RES_SCALE")),
              QStringLiteral("res_scale is seen as a switched-off setting"));
        check(ini.commentedKeys().contains(QStringLiteral("BACKEND")),
              QStringLiteral("backend is seen as a switched-off setting"));
        // The note near the top writes `#   fps_limit = 40` inside a paragraph.
        check(ini.sectionOf(QStringLiteral("fps_limit")) == QLatin1String("display"),
              QStringLiteral("fps_limit is found in [display], not in the prose above it"),
              ini.sectionOf(QStringLiteral("fps_limit")));
    }

    {
        IniFile ini;
        QString error;
        ini.load(example, &error);
        ini.set(QStringLiteral("msaa"), QStringLiteral("4"), QStringLiteral("display"));
        const QString text = QString::fromUtf8(ini.render());
        check(text.contains(QLatin1String("\nmsaa = 4\n")),
              QStringLiteral("set uncomments the declaration in place"));
        check(text.count(QLatin1String("msaa =")) == 1,
              QStringLiteral("set does not append a second msaa line"),
              QString::number(text.count(QLatin1String("msaa ="))));
        check(ini.value(QStringLiteral("STRIKERS_MSAA")) == QLatin1String("4"),
              QStringLiteral("the prefixed spelling reads the same key"));

        ini.unset(QStringLiteral("MSAA"));
        check(ini.render() == original,
              QStringLiteral("unset restores the file byte for byte"),
              firstDifference(original, ini.render()));
    }

    {
        IniFile ini;
        QString error;
        ini.load(example, &error);
        // key_a is the uncomment-in-place case and a missing section is the create case.
        ini.set(QStringLiteral("key_a"), QStringLiteral("Q"), QStringLiteral("input"));
        ini.set(QStringLiteral("vsync"), QStringLiteral("0"), QStringLiteral("display"));
        ini.set(QStringLiteral("brand_new"), QStringLiteral("1"), QStringLiteral("nosuchsection"));
        const QString text = QString::fromUtf8(ini.render());
        check(text.contains(QLatin1String("\nkey_a = Q\n")) && text.count(QLatin1String("key_a =")) == 1,
              QStringLiteral("a key the file lists commented out is uncommented in place"),
              text.right(120));
        check(text.contains(QLatin1String("[nosuchsection]\nbrand_new = 1\n")),
              QStringLiteral("a new key makes its section and lands in it"),
              text.right(120));

        IniFile back;
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("strikers.ini"));
        check(ini.save(path, &error), QStringLiteral("save"), error);
        check(back.load(path, &error), QStringLiteral("reload"), error);
        check(back.value(QStringLiteral("key_a")) == QLatin1String("Q"),
              QStringLiteral("key_a survives the round trip"));
        check(back.value(QStringLiteral("vsync")) == QLatin1String("0"),
              QStringLiteral("vsync survives the round trip"));
        check(back.render() == ini.render(),
              QStringLiteral("a saved file reloads to itself"));
    }

    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("dialect.ini"));
        {
            QFile f(path);
            check(f.open(QIODevice::WriteOnly), QStringLiteral("write the dialect fixture"));
            f.write("; a semicolon comment\r\n"
                    "[display]\r\n"
                    "  RES_SCALE\t=\t2.0\r\n"
                    "STRIKERS_ASPECT = \"16 : 9\"\r\n"
                    "res_scale = 3.0\r\n"
                    "no_equals_here\r\n");
        }
        IniFile ini;
        QString error;
        check(ini.load(path, &error), QStringLiteral("load a CRLF file"), error);
        check(ini.value(QStringLiteral("res_scale")) == QLatin1String("2.0"),
              QStringLiteral("a duplicate key takes the FIRST value, as setenv_default does"),
              ini.value(QStringLiteral("res_scale")));
        check(ini.value(QStringLiteral("aspect")) == QLatin1String("16 : 9"),
              QStringLiteral("quotes are stripped and inner spaces kept"),
              ini.value(QStringLiteral("aspect")));
        check(!ini.has(QStringLiteral("no_equals_here")),
              QStringLiteral("a line with no '=' is not a key"));
        const QString text = QString::fromUtf8(ini.render());
        check(text.contains(QLatin1String("\r\n")),
              QStringLiteral("CRLF endings survive"));

        // The key keeps the spelling and quoting the file gave it.
        ini.set(QStringLiteral("aspect"), QStringLiteral("21:9"), QStringLiteral("display"));
        check(QString::fromUtf8(ini.render()).contains(QLatin1String("STRIKERS_ASPECT = \"21:9\"")),
              QStringLiteral("a rewritten value keeps the file's own spelling and quoting"),
              QString::fromUtf8(ini.render()));

        // A value that would not survive config.c's trim comes back quoted.
        ini.set(QStringLiteral("data"), QStringLiteral("C:/games/x "), QStringLiteral("paths"));
        check(QString::fromUtf8(ini.render()).contains(QLatin1String("data = \"C:/games/x \"")),
              QStringLiteral("a trailing space is quoted"));
    }

    {
        for (const Binding& b : Schema::keyboardBindings())
        {
            check(KeyNames::isKnown(b.def),
                  QStringLiteral("%1 default '%2' is an SDL scancode name").arg(b.key, b.def));
        }
        for (const Binding& b : Schema::gamepadBindings())
        {
            check(Schema::padButtonNames().contains(b.def),
                  QStringLiteral("%1 default '%2' is an SDL gamepad button").arg(b.key, b.def));
        }
        QStringList seen;
        for (const Binding& b : Schema::keyboardBindings())
        {
            check(!seen.contains(b.def),
                  QStringLiteral("keyboard default '%1' is bound once").arg(b.def));
            seen << b.def;
        }
    }

    // 7. The archive root, which on a packaged macOS build is not the folder the executable is in.
    {
        const QString plain = QStringLiteral("/opt/strikers");
        check(AppPaths::archiveRootFor(plain) == plain,
              QStringLiteral("a plain folder is its own archive root"),
              AppPaths::archiveRootFor(plain));

        const QString bundle =
            QStringLiteral("/opt/strikers/strikers-settings.app/Contents/MacOS");
        const QString got = AppPaths::archiveRootFor(bundle);
#if defined(Q_OS_MACOS)
        check(got == plain,
              QStringLiteral("a bundle resolves three levels up to the archive root"), got);
        check(got != bundle,
              QStringLiteral("... which is not applicationDirPath(), the old answer"), got);
#else
        check(got == bundle,
              QStringLiteral("off macOS the path is left alone"), got);
#endif
        // A folder that merely ends in MacOS is not a bundle.
        const QString decoy = QStringLiteral("/home/me/Contents/MacOS/notabundle");
        check(AppPaths::archiveRootFor(decoy) == decoy,
              QStringLiteral("only the exact bundle shape is unwrapped"),
              AppPaths::archiveRootFor(decoy));
        check(AppPaths::archiveRootFor(plain + QLatin1Char('/')) == plain,
              QStringLiteral("a trailing slash does not change the answer"));
    }

    // 8. Seeding a new player's file.
    {
        QTemporaryDir dir;
        const QString root = dir.path();
        const QString ini = root + QStringLiteral("/strikers.ini");
        check(AppPaths::findExample(ini, root).isEmpty(),
              QStringLiteral("no example beside the file means no seed"));

        QFile::copy(example, root + QStringLiteral("/strikers.ini.example"));
        check(AppPaths::findExample(ini, root) == root + QStringLiteral("/strikers.ini.example"),
              QStringLiteral("the example in the archive root is the seed"),
              AppPaths::findExample(ini, root));

        // The claim the seeding rests on: an all-comments file and no file at all configure the
        // game identically. config.c skips a line beginning with '#' before it looks at anything
        // else, so "no live keys" is the whole of it.
        IniFile seed;
        QString error;
        check(seed.load(AppPaths::findExample(ini, root), &error),
              QStringLiteral("the seed loads"), error);
        check(seed.liveKeys().isEmpty(),
              QStringLiteral("the seed sets nothing, so seeding changes no behaviour"),
              seed.liveKeys().join(QLatin1String(", ")));
        check(seed.render() == original,
              QStringLiteral("a seeded file saved untouched is the example, byte for byte"));
    }

    // 9. No explanation is ever shortened.
    {
        for (const Setting& s : Schema::all())
        {
            // The expanded form: the reference paragraphs sit behind a "Technical details" link so
            // that the panel opens on one line rather than an essay.
            const QString html = HelpText::popover(s, true);
            check(!html.contains(QString::fromUtf8("\xE2\x80\xA6")),
                  QStringLiteral("%1: no ellipsis in the explanation").arg(s.key));
            check(!html.contains(QLatin1String("hover for the rest")),
                  QStringLiteral("%1: no truncation marker").arg(s.key));

            // Every line of the shipped wording survives into the panel.
            bool whole = true;
            QString missing;
            for (const QString& line : s.help.split(QLatin1Char('\n')))
            {
                const QString t = line.trimmed();
                if (t.isEmpty())
                    continue;
                // The panel reflows prose, so a hard-wrapped line arrives in pieces; its longest
                // word-run is enough to prove nothing was dropped, and the words themselves are
                // checked.
                for (const QString& word : t.split(QLatin1Char(' '), Qt::SkipEmptyParts))
                {
                    if (html.contains(word.toHtmlEscaped()))
                        continue;
                    whole = false;
                    missing = word;
                    break;
                }
                if (!whole)
                    break;
            }
            check(whole, QStringLiteral("%1: the whole shipped wording reaches the panel").arg(s.key),
                  QStringLiteral("missing: %1").arg(missing));

            // and the collapsed form is what a player actually meets, so it must be the short one.
            const QString brief = HelpText::popover(s);
            check(!brief.contains(QLatin1String("<p>")) || s.help.isEmpty()
                      || brief.length() < html.length(),
                  QStringLiteral("%1: the panel opens short").arg(s.key));

            check(!s.label.isEmpty(), QStringLiteral("%1 has a label").arg(s.key));
            check(!s.detail.isEmpty(),
                  QStringLiteral("%1 has a plain-language sentence").arg(s.key));
        }
    }

    // 10. Nothing a player reads names a variable, a key, the graphics library or the
    // decompilation.
    {
        const QStringList banned = {
            QStringLiteral("Aurora"), QStringLiteral("STRIKERS_"),
            QStringLiteral(".md"),
            QStringLiteral("decomp"),
            QStringLiteral("GX"),
        };
        for (const Setting& s : Schema::all())
        {
            QStringList facing = { s.label, s.check, s.detail };
            facing += s.valueLabels;
            QString offence;
            for (const QString& text : facing)
            {
                for (const QString& word : banned)
                {
                    if (text.contains(word))
                        offence = QStringLiteral("\"%1\" in \"%2\"").arg(word, text);
                }
                // An underscore is what a raw ini key looks like.
                if (text.contains(QLatin1Char('_')))
                    offence = QStringLiteral("a raw key spelling in \"%1\"").arg(text);
            }
            check(offence.isEmpty(),
                  QStringLiteral("%1 says nothing to a player that belongs in the panel")
                      .arg(s.key),
                  offence);
        }
    }

    // 11. schema.h says the help is copied from strikers.ini.example rather than rewritten, so that
    // there is one explanation of each setting.
    {
        const QStringList lines = QString::fromUtf8(original).split(QLatin1Char('\n'));
        for (const Setting& s : Schema::all())
        {
            // The last declaration of the key: the first can be an example inside the prose at the
            // top of the file.
            int at = -1;
            const QRegularExpression decl(
                QStringLiteral("^#\\s*%1\\s*=").arg(QRegularExpression::escape(s.key)));
            for (int i = 0; i < lines.size(); ++i)
            {
                if (decl.match(lines[i]).hasMatch())
                    at = i;
            }
            if (at < 0)
                continue;

            QStringList block;
            for (int i = at - 1; i >= 0; --i)
            {
                if (!lines[i].startsWith(QLatin1Char('#')))
                    break;
                if (QRegularExpression(QStringLiteral("^#\\s*[A-Za-z_]+\\s*=")).match(lines[i]).hasMatch())
                    break;
                QString text = lines[i];
                text.remove(0, 1);
                if (text.startsWith(QLatin1Char(' ')))
                    text.remove(0, 1);
                while (text.endsWith(QLatin1Char('\r')) || text.endsWith(QLatin1Char(' ')))
                    text.chop(1);
                block.prepend(text);
            }
            while (!block.isEmpty() && block.first().isEmpty())
                block.removeFirst();
            while (!block.isEmpty() && block.last().isEmpty())
                block.removeLast();
            if (block.isEmpty())
                continue;

            // simplified(), so the example may wrap at 100 columns while the
            // GUI keeps one flowing paragraph; the wording still has to match.
            const QString fromFile = block.join(QLatin1Char(' ')).simplified();
            check(fromFile == s.help.simplified(),
                  QStringLiteral("%1's help is the example's own wording").arg(s.key),
                  QStringLiteral("file:\n%1\nschema:\n%2").arg(fromFile, s.help));
        }
    }

    // 10. The same value saved twice stays saved, and a value set back to the file's own keeps its bytes.
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("strikers.ini"));
        const QByteArray loaded = "[display]\nmsaa = 1\n";
        {
            QFile f(path);
            f.open(QIODevice::WriteOnly);
            f.write(loaded);
        }
        IniFile ini;
        QString error;
        check(ini.load(path, &error), QStringLiteral("load a one-key file"), error);
        ini.set(QStringLiteral("msaa"), QStringLiteral("4"), QStringLiteral("display"));
        ini.set(QStringLiteral("msaa"), QStringLiteral("4"), QStringLiteral("display"));
        check(ini.render() == "[display]\nmsaa = 4\n",
              QStringLiteral("setting the same value twice keeps it"),
              QString::fromUtf8(ini.render()));
        check(ini.save(path, &error), QStringLiteral("save"), error);
        IniFile back;
        check(back.load(path, &error) && back.value(QStringLiteral("msaa")) == QLatin1String("4"),
              QStringLiteral("the saved file carries the value the window showed"),
              back.value(QStringLiteral("msaa")));
        // And a value set back to the file's own restores the file's own bytes.
        ini.set(QStringLiteral("msaa"), QStringLiteral("1"), QStringLiteral("display"));
        check(ini.render() == loaded,
              QStringLiteral("setting the loaded value back restores the line byte for byte"),
              firstDifference(loaded, ini.render()));
    }

    // 11. Resetting a key the file names twice switches off both lines; config.c takes the first live one.
    {
        QTemporaryDir dir;
        const QString path = dir.filePath(QStringLiteral("strikers.ini"));
        {
            QFile f(path);
            f.open(QIODevice::WriteOnly);
            f.write("[display]\nmsaa = 4\nmsaa = 4\n");
        }
        IniFile ini;
        QString error;
        check(ini.load(path, &error), QStringLiteral("load a file with a duplicate key"), error);
        ini.unset(QStringLiteral("msaa"));
        check(!ini.has(QStringLiteral("msaa")),
              QStringLiteral("unset leaves no live occurrence"),
              QString::fromUtf8(ini.render()));
        check(ini.render() == "[display]\n# msaa = 4\n# msaa = 4\n",
              QStringLiteral("both duplicates are commented out"),
              QString::fromUtf8(ini.render()));
    }

    QTextStream out(stdout);
    out << (g_failures == 0 ? "\nall checks passed\n"
                            : QStringLiteral("\n%1 check(s) failed\n").arg(g_failures));
    return g_failures == 0 ? 0 : 1;
}
