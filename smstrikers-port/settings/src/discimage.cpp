#include "discimage.h"

#include "port/disc.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QVector>

#include <cstring>

// qUncompress wants a four-byte big-endian size in front of the zlib stream.
extern "C" int port_disc_inflate(unsigned char* dst, unsigned long* dstLen,
                                 const unsigned char* src, unsigned long srcLen)
{
    QByteArray in;
    in.reserve(qsizetype(srcLen) + 4);
    const unsigned long want = *dstLen;
    in.append(char((want >> 24) & 0xFF));
    in.append(char((want >> 16) & 0xFF));
    in.append(char((want >> 8) & 0xFF));
    in.append(char(want & 0xFF));
    in.append(reinterpret_cast<const char*>(src), qsizetype(srcLen));
    const QByteArray out = qUncompress(in);
    if (out.isEmpty() || quint64(out.size()) > want)
        return -1;
    std::memcpy(dst, out.constData(), size_t(out.size()));
    *dstLen = (unsigned long)out.size();
    return 0;
}

namespace {

class Text
{
    Q_DECLARE_TR_FUNCTIONS(DiscImage)
};

struct Entry
{
    QByteArray path;
    unsigned offset;
    unsigned length;
    bool isDir;
};

struct Closer
{
    PortDisc* d;
    ~Closer() { port_disc_close(d); }
};

quint32 be32(const unsigned char* p)
{
    return (quint32(p[0]) << 24) | (quint32(p[1]) << 16) | (quint32(p[2]) << 8) | p[3];
}

QString reflow(const char* text)
{
    QString s = QString::fromUtf8(text);
    s.replace(QStringLiteral("\n\n"), QStringLiteral("\001"));
    s.replace(QLatin1Char('\n'), QLatin1Char(' '));
    s.replace(QLatin1Char('\001'), QLatin1Char('\n'));
    return s;
}

PortDisc* open(const QString& path, QString* error)
{
    char err[2048];
    PortDisc* d = port_disc_open(QFile::encodeName(path).constData(), err, sizeof err);
    if (d == nullptr && error != nullptr)
        *error = reflow(err);
    return d;
}

bool walk(PortDisc* d, QVector<Entry>* entries, QString* error)
{
    char err[2048];
    const auto visit = [](void* user, const char* path, unsigned offset, unsigned length,
                          int isDir) -> int {
        static_cast<QVector<Entry>*>(user)->push_back(
            { QByteArray(path), offset, length, isDir != 0 });
        return 0;
    };
    if (port_disc_walk(d, visit, entries, err, sizeof err) != 0)
    {
        if (error != nullptr)
            *error = reflow(err);
        return false;
    }
    return true;
}

// FST names become host paths, so nothing that climbs out, names a drive or is not printable ASCII.
bool safeRelative(const QByteArray& path)
{
    if (path.isEmpty())
        return false;
    for (const QByteArray& part : path.split('/'))
    {
        if (part.isEmpty() || part == "." || part == "..")
            return false;
        for (char c : part)
        {
            if (c < 0x20 || c > 0x7E || c == '\\' || c == ':')
                return false;
        }
    }
    return true;
}

} // namespace

namespace DiscImage {

Info inspect(const QString& path)
{
    Info info;
    const QFileInfo fi(path);
    if (!fi.exists())
    {
        info.error = Text::tr("File not found.");
        return info;
    }
    if (!fi.isFile())
    {
        info.error = Text::tr("That is a folder, not a disc image.");
        return info;
    }

    PortDisc* d = open(path, &info.error);
    if (d == nullptr)
        return info;
    Closer closer{ d };

    unsigned char head[0x220];
    if (port_disc_read(d, head, sizeof head, 0) != long(sizeof head))
    {
        info.error = Text::tr("Could not read the image past its header.");
        return info;
    }
    info.format = QString::fromLatin1(port_disc_format(d));
    info.gameId = QString::fromLatin1(reinterpret_cast<const char*>(head), 6);
    info.title = QString::fromLatin1(reinterpret_cast<const char*>(head) + 0x20,
                                     int(qstrnlen(reinterpret_cast<const char*>(head) + 0x20, 0x40)))
                     .trimmed();
    info.nkit = std::memcmp(head + 0x200, "NKIT", 4) == 0;

    QVector<Entry> entries;
    if (!walk(d, &entries, &info.error))
        return info;
    for (const Entry& e : entries)
    {
        if (e.isDir)
            continue;
        info.files++;
        info.bytes += e.length;
    }

    bool sentinel = false;
    for (const Entry& e : entries)
        sentinel = sentinel || (!e.isDir && e.path.compare("common.ini", Qt::CaseInsensitive) == 0);
    if (!info.gameId.startsWith(QLatin1String("G4Q")) || !sentinel)
    {
        info.notThisGame = true;
        info.error = Text::tr("This disc image is for another game (%1).")
                         .arg(info.title.isEmpty() ? info.gameId : info.title);
        return info;
    }

    info.ok = true;
    return info;
}

QStringList nameFilters()
{
    return { QStringLiteral("*.iso"), QStringLiteral("*.gcm"), QStringLiteral("*.ciso"),
             QStringLiteral("*.gcz") };
}

QString defaultExtractDir(const QString& gameId, const QString& base)
{
    if (!base.isEmpty())
        return QDir::cleanPath(base + QStringLiteral("/Game Data/") + gameId);
    QString data = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    if (data.isEmpty())
        data = QDir::homePath();
    return QDir::cleanPath(data + QStringLiteral("/Super Mario Strikers/Game Data/") + gameId);
}

QString extractedGameId(const QString& files)
{
    QFile boot(QDir(files).filePath(QStringLiteral("../sys/boot.bin")));
    if (!boot.open(QIODevice::ReadOnly))
        return QString();
    const QByteArray id = boot.read(6);
    if (id.size() != 6 || !id.startsWith("G4Q"))
        return QString();
    return QString::fromLatin1(id);
}

Result extract(const QString& image, const QString& outDir, const Progress& progress,
               const std::atomic<bool>& cancel, QString* error)
{
    QString dummy;
    QString& why = error != nullptr ? *error : dummy;

    const Info info = inspect(image);
    if (!info.ok)
    {
        why = info.error;
        return Result::Failed;
    }

    const QString target = QDir::cleanPath(QFileInfo(outDir).absoluteFilePath());
    if (QFileInfo::exists(target))
    {
        why = Text::tr("%1 already exists. Extraction requires a new folder.")
                  .arg(QDir::toNativeSeparators(target));
        return Result::Failed;
    }

    PortDisc* d = open(image, &why);
    if (d == nullptr)
        return Result::Failed;
    Closer closer{ d };

    unsigned char head[0x2440];
    unsigned char appHead[0x20];
    unsigned char dolHead[0x100];
    if (port_disc_read(d, head, sizeof head, 0) != long(sizeof head)
        || port_disc_read(d, appHead, sizeof appHead, 0x2440) != long(sizeof appHead))
    {
        why = Text::tr("Could not read the image past its header.");
        return Result::Failed;
    }
    const quint32 dolOff = be32(head + 0x420);
    const quint32 fstOff = be32(head + 0x424);
    const quint32 fstSize = be32(head + 0x428);
    const quint64 appSize = 0x20ull + be32(appHead + 0x14) + be32(appHead + 0x18);
    if (port_disc_read(d, dolHead, sizeof dolHead, dolOff) != long(sizeof dolHead))
    {
        why = Text::tr("Could not read the image past its header.");
        return Result::Failed;
    }
    quint64 dolSize = 0x100;
    for (int i = 0; i < 18; ++i)
    {
        const quint64 off = be32(dolHead + i * 4);
        const quint64 size = be32(dolHead + 0x90 + i * 4);
        if (size != 0)
            dolSize = qMax(dolSize, off + size);
    }
    if (appSize > (16u << 20) || dolSize > (64u << 20))
    {
        why = Text::tr("The image is damaged. Its header describes a program too large for "
                       "this game.");
        return Result::Failed;
    }

    QVector<Entry> entries;
    if (!walk(d, &entries, &why))
        return Result::Failed;
    for (const Entry& e : entries)
    {
        if (!safeRelative(e.path))
        {
            why = Text::tr("The image contains an unsafe file name, \"%1\". The image is "
                           "damaged or has been modified.")
                      .arg(QString::fromLatin1(e.path));
            return Result::Failed;
        }
    }

    struct Piece
    {
        QString rel;
        quint64 offset;
        quint64 length;
    };
    QVector<Piece> sys = {
        { QStringLiteral("sys/boot.bin"), 0, 0x440 },
        { QStringLiteral("sys/bi2.bin"), 0x440, 0x2000 },
        { QStringLiteral("sys/apploader.img"), 0x2440, appSize },
        { QStringLiteral("sys/main.dol"), dolOff, dolSize },
        { QStringLiteral("sys/fst.bin"), fstOff, fstSize },
    };

    qint64 total = 0;
    for (const Piece& p : sys)
        total += qint64(p.length);
    total += info.bytes;

    const QString parent = QFileInfo(target).absolutePath();
    if (!QDir().mkpath(parent))
    {
        why = Text::tr("Could not create %1.").arg(QDir::toNativeSeparators(parent));
        return Result::Failed;
    }
    const QStorageInfo volume(parent);
    if (volume.isValid() && volume.bytesAvailable() >= 0 && volume.bytesAvailable() < total)
    {
        why = Text::tr("Not enough free space. The game files need %1 MB; the drive has %2 MB "
                       "free.")
                  .arg((total + (1 << 20) - 1) >> 20)
                  .arg(volume.bytesAvailable() >> 20);
        return Result::Failed;
    }

    const QString work = parent + QStringLiteral("/.") + QFileInfo(target).fileName()
                         + QStringLiteral(".extracting");
    QDir(work).removeRecursively();
    if (!QDir().mkpath(work + QStringLiteral("/sys")) || !QDir().mkpath(work + QStringLiteral("/files")))
    {
        why = Text::tr("Could not create %1.").arg(QDir::toNativeSeparators(work));
        return Result::Failed;
    }

    struct Cleanup
    {
        QString dir;
        bool keep = false;
        ~Cleanup()
        {
            if (!keep)
                QDir(dir).removeRecursively();
        }
    } cleanup{ work };

    QByteArray buffer(4 << 20, Qt::Uninitialized);
    qint64 done = 0;

    const auto copy = [&](const QString& rel, quint64 offset, quint64 length) -> Result {
        QFile out(work + QLatin1Char('/') + rel);
        if (!out.open(QIODevice::WriteOnly | QIODevice::NewOnly))
        {
            why = Text::tr("Could not write %1: %2")
                      .arg(QDir::toNativeSeparators(out.fileName()), out.errorString());
            return Result::Failed;
        }
        quint64 left = length;
        while (left > 0)
        {
            if (cancel.load())
                return Result::Cancelled;
            const size_t want = size_t(qMin<quint64>(left, quint64(buffer.size())));
            const long got = port_disc_read(d, buffer.data(), want, offset + (length - left));
            if (got <= 0)
            {
                why = Text::tr("The image ends before the end of %1. The image is incomplete "
                               "or damaged.")
                          .arg(rel);
                return Result::Failed;
            }
            if (out.write(buffer.constData(), got) != got)
            {
                why = Text::tr("Could not write %1: %2")
                          .arg(QDir::toNativeSeparators(out.fileName()), out.errorString());
                return Result::Failed;
            }
            left -= quint64(got);
            done += got;
            if (progress)
                progress(done, total);
        }
        if (!out.flush())
        {
            why = Text::tr("Could not write %1: %2")
                      .arg(QDir::toNativeSeparators(out.fileName()), out.errorString());
            return Result::Failed;
        }
        return Result::Ok;
    };

    for (const Piece& p : sys)
    {
        const Result r = copy(p.rel, p.offset, p.length);
        if (r != Result::Ok)
            return r;
    }
    for (const Entry& e : entries)
    {
        const QString rel = QStringLiteral("files/") + QString::fromLatin1(e.path);
        if (e.isDir)
        {
            if (!QDir().mkpath(work + QLatin1Char('/') + rel))
            {
                why = Text::tr("Could not create %1.")
                          .arg(QDir::toNativeSeparators(work + QLatin1Char('/') + rel));
                return Result::Failed;
            }
            continue;
        }
        const Result r = copy(rel, e.offset, e.length);
        if (r != Result::Ok)
            return r;
    }

    if (!QDir().rename(work, target))
    {
        why = Text::tr("Could not move the extracted files into %1.")
                  .arg(QDir::toNativeSeparators(target));
        return Result::Failed;
    }
    cleanup.keep = true;
    return Result::Ok;
}

} // namespace DiscImage
