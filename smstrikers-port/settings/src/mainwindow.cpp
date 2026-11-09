#include "mainwindow.h"

#include "apppaths.h"
#include "discimage.h"
#include "helptext.h"
#include "infobutton.h"
#include "keycapturebutton.h"
#include "keynames.h"
#include "settingspage.h"
#include "texturepacks.h"
#include "windowlanguage.h"

#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QDoubleSpinBox>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFontMetrics>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMap>
#include <QMessageBox>
#include <QPair>
#include <QPalette>
#include <QProcess>
#include <QProgressDialog>
#include <QPushButton>
#include <QResizeEvent>
#include <QScreen>
#include <QScrollArea>
#include <QSettings>
#include <QSlider>
#include <QStandardItemModel>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QThread>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>
#include <QVariant>

#include "port/aspect.h"
#include "port/steamdeck.h"

namespace {

QString trimNumber(double v)
{
    QString s = QString::number(v, 'f', 2);
    while (s.contains(QLatin1Char('.')) && (s.endsWith(QLatin1Char('0'))))
        s.chop(1);
    if (s.endsWith(QLatin1Char('.')))
        s.chop(1);
    return s;
}

// One spelling: a \x escape takes every hex digit after it, so "\x01custom" is not "\001custom".
const QLatin1String kCustomChoice("\001custom");

QString trimScale(double scale)
{
    QString s = QString::number(scale, 'f', 3);
    while (s.endsWith(QLatin1Char('0')))
        s.chop(1);
    if (s.endsWith(QLatin1Char('.')))
        s.chop(1);
    return s;
}

bool truthy(const QString& v)
{
    const QString s = v.trimmed().toLower();
    return s == QLatin1String("1") || s == QLatin1String("true") ||
           s == QLatin1String("yes") || s == QLatin1String("on");
}

// A switch and its info button, then a button; `lead` is the first two, for lining up several rows.
QWidget* switchRow(QCheckBox* box, QWidget* info, QPushButton* button, QWidget** lead)
{
    *lead = new QWidget;
    auto* l = new QHBoxLayout(*lead);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(6);
    l->addWidget(box);
    l->addSpacing(SettingsPage::trailingGap(box));
    l->addWidget(info, 0, Qt::AlignVCenter);
    l->addStretch(1);

    auto* row = new QWidget;
    auto* h = new QHBoxLayout(row);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(12);
    h->addWidget(*lead);
    h->addWidget(button);
    h->addStretch(1);
    return row;
}

bool falsy(const QString& v)
{
    const QString s = v.trimmed().toLower();
    return s == QLatin1String("0") || s == QLatin1String("false") ||
           s == QLatin1String("no") || s == QLatin1String("off");
}

const Setting& byKey(const QVector<Setting>& group, const char* k)
{
    for (const Setting& x : group)
    {
        if (x.key == QLatin1String(k))
            return x;
    }
    return group.first();
}

bool isGlBackend(const QString& backend)
{
    return backend == QLatin1String("opengl") || backend == QLatin1String("opengles");
}

bool backendSupportedHere(const QString& backend)
{
    if (backend.isEmpty())
        return true; // automatic
#if defined(Q_OS_WIN)
    return backend == QLatin1String("d3d12") || backend == QLatin1String("vulkan");
#elif defined(Q_OS_MACOS)
    return backend == QLatin1String("metal");
#else
    return backend == QLatin1String("vulkan") || isGlBackend(backend);
#endif
}

// A row that is a control plus something that is not a setting, the aspect ratio's free-text box,
// the Browse button.
QWidget* pair(QWidget* first, QWidget* second, int stretchFirst = 0)
{
    auto* row = new QWidget;
    auto* h = new QHBoxLayout(row);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(6);
    h->addWidget(first, stretchFirst);
    h->addSpacing(SettingsPage::trailingGap(first));
    h->addWidget(second);
    return row;
}

QString releaseName(const QString& gameId)
{
    if (gameId.startsWith(QLatin1String("G4QP")))
        return MainWindow::tr("Mario Smash Football (Europe)");
    if (gameId.startsWith(QLatin1String("G4QJ")))
        return MainWindow::tr("Super Mario Strikers (Japan)");
    if (gameId.startsWith(QLatin1String("G4QE")))
        return MainWindow::tr("Super Mario Strikers (USA)");
    return gameId;
}

void setGood(QLabel* label, const QString& text)
{
    label->setText(text);
    label->setStyleSheet(QStringLiteral("color: #3f9142;"));
}

void setBad(QLabel* label, const QString& text)
{
    label->setText(text);
    label->setStyleSheet(QStringLiteral("color: #d64545;"));
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("Super Mario Strikers Settings"));

    auto* central = new QWidget;
    auto* outer = new QVBoxLayout(central);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    m_tabs = new SettingsTabs;
    m_tabs->addTab(buildDisplayTab(), tr("Display"));
    m_tabs->addTab(buildAudioTab(), tr("Audio"));
    m_tabs->addTab(buildInputTab(), tr("Controls"));
    m_gameTab = m_tabs->addTab(buildGameTab(), tr("Game"));
    outer->addWidget(m_tabs, 1);

    auto* buttons = new QHBoxLayout;
    buttons->setContentsMargins(20, 12, 20, 16);
    buttons->setSpacing(10);
    m_playButton = new QPushButton(tr("Play"));
    m_playButton->setDefault(false);
    auto* reset = new QPushButton(tr("Reset to Defaults"));
    auto* revert = new QPushButton(tr("Revert"));
    m_saveButton = new QPushButton(tr("Save"));
    m_saveButton->setDefault(true);

    buttons->addWidget(m_playButton);
    buttons->addStretch(1);
    buttons->addWidget(reset);
    buttons->addWidget(revert);
    buttons->addWidget(m_saveButton);
    outer->addLayout(buttons);

    connect(m_saveButton, &QPushButton::clicked, this, &MainWindow::onSave);
    connect(revert, &QPushButton::clicked, this, &MainWindow::onRevert);
    connect(reset, &QPushButton::clicked, this, &MainWindow::onResetAll);
    connect(m_playButton, &QPushButton::clicked, this, &MainWindow::onPlay);

    setCentralWidget(central);

    m_pathLabel = new QLabel;
    // Ignored, not Preferred: showPath() elides to the current width from resizeEvent, and a label
    // whose size hint follows the elided text raises the window's minimum width, which resizes,
    // which elides again.
    m_pathLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    m_pathLabel->setMinimumWidth(0);
    statusBar()->addWidget(m_pathLabel, 1);
    statusBar()->setSizeGripEnabled(true);

    updatePlayButton();

    // The width comes from the widest page and then stays put; the height follows whichever page is
    // showing.
    adjustSize();
    // A settings window narrower than this is technically legible and reads as cramped: the control
    // column ends up against the right edge and every note wraps.
    resize(qMax(width(), 620), height());

    // No refit on a tab change: the window is already the height of the tallest page, so every tab
    // fits without moving anything.
    fitToCurrentTab();
}

void MainWindow::rememberGeometry()
{
    m_rememberGeometry = true;
    QSettings settings;
    const QByteArray geometry = settings.value(QStringLiteral("window/geometry")).toByteArray();
    if (!geometry.isEmpty())
        restoreGeometry(geometry);
}

void MainWindow::changeEvent(QEvent* event)
{
    // Packs dropped in from the file manager show when the window is back in front.
    if (event->type() == QEvent::ActivationChange && isActiveWindow())
        updateTexturePacks();
    QMainWindow::changeEvent(event);
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    // A QThread destroyed while running takes the process with it.
    if (m_extractThread != nullptr)
    {
        m_extractCancel = true;
        m_extractThread->wait();
    }

    if (m_rememberGeometry)
    {
        QSettings settings;
        settings.setValue(QStringLiteral("window/geometry"), saveGeometry());
    }
    QMainWindow::closeEvent(event);
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    ++m_resizes;
    QMainWindow::resizeEvent(event);
    showPath(m_pathText);
}

// The height follows the selected tab; SettingsTabs is what makes the window's own sizeHint say so.
void MainWindow::fitToCurrentTab()
{
    // Re-entrancy guard. resize() below delivers a resizeEvent synchronously on some platforms, and
    // anything that reaches back here from inside one turns a single fit into an unbounded chain.
    if (m_fitting)
        return;
    m_fitting = true;
    struct Clear { bool* f; ~Clear() { *f = false; } } clear{ &m_fitting };

    m_tabs->updateGeometry();
    if (m_tabs->layout() != nullptr)
        m_tabs->layout()->invalidate();
    if (centralWidget() != nullptr && centralWidget()->layout() != nullptr)
        centralWidget()->layout()->invalidate();
    layout()->invalidate();
    layout()->activate();

    const QRect screen = QGuiApplication::primaryScreen() != nullptr
                             ? QGuiApplication::primaryScreen()->availableGeometry()
                             : QRect(0, 0, 1920, 1080);
    const int want = qBound(minimumSizeHint().height(), sizeHint().height(),
                            screen.height());
    // A one-pixel disagreement between what the layout wants and what the window manager grants
    // would otherwise be a resize on every request.
    if (qAbs(want - height()) > 2)
        resize(width(), want);
}

void MainWindow::registerControl(const Setting& s, std::function<QString()> get,
                                 std::function<void(const QString&)> set)
{
    Control c;
    c.key = s.key;
    c.section = s.section;
    c.def = s.def;
    c.get = std::move(get);
    c.set = std::move(set);
    m_controlByKey.insert(IniFile::normalise(s.key), int(m_controls.size()));
    m_controls.push_back(std::move(c));
}

InfoButton* MainWindow::infoFor(const Setting& s)
{
    auto* info = new InfoButton(HelpText::popover(s), HelpText::popover(s, true));
    m_infoByKey.insert(IniFile::normalise(s.key), info);
    return info;
}

void MainWindow::addSwitch(SettingsPage* page, const Setting& s)
{
    auto* box = new QCheckBox(s.check);
    page->addSetting(s.label, box, infoFor(s));
    connect(box, &QCheckBox::toggled, this, &MainWindow::markDirty);
    registerControl(
        s, [box] { return QString(box->isChecked() ? "1" : "0"); },
        [box](const QString& v) { box->setChecked(truthy(v)); });
}

QComboBox* MainWindow::addChoice(SettingsPage* page, const Setting& s)
{
    auto* combo = new QComboBox;
    for (int i = 0; i < s.values.size(); ++i)
        combo->addItem(s.valueLabels.value(i, s.values[i]), s.values[i]);

    page->addSetting(s.label, combo, infoFor(s));
    connect(combo, &QComboBox::currentIndexChanged, this, &MainWindow::markDirty);
    registerControl(
        s, [combo] { return combo->currentData().toString(); },
        [combo](const QString& v) {
            int i = combo->findData(v);
            if (i < 0)
            {
                // A value the file has and the combo does not: keep it rather than silently
                // rewriting the user's file on the next save.
                i = combo->count();
                while (i > 0 && isGlBackend(combo->itemData(i - 1).toString()))
                    --i;
                combo->insertItem(i, MainWindow::tr("%1 (from file)").arg(v), v);
            }
            combo->setCurrentIndex(i);
        });
    return combo;
}

QWidget* MainWindow::buildDisplayTab()
{
    auto* page = new SettingsPage;
    const auto& group = Schema::display();

    page->beginSection(tr("Resolution and quality"));

    {
        const Setting& s = byKey(group, "res_scale");
        auto* combo = new QComboBox;
        // The game renders the panel's rows on a Deck when this is left unset.
        combo->addItem(PortIsSteamDeck() ? tr("Automatic (Steam Deck, 1280×800)")
                                         : tr("Automatic (follows the window)"),
                       QString());
        // One entry per height, since the height is all the value holds; the width names the display it is known from.
        static const struct { int width; int rows; const char* name; } kResolutions[] = {
            { 640, 448, "GameCube" },
            { 854, 480, "480p" },
            { 960, 540, "540p" },
            { 1024, 576, "576p" },
            { 1280, 720, "720p" },
            { 1366, 768, nullptr },
            { PORT_STEAM_DECK_WIDTH, PORT_STEAM_DECK_ROWS, "Steam Deck" },
            { 1600, 900, "900p" },
            { 1920, 1080, "1080p" },
            { 1920, 1200, nullptr },
            { 2560, 1440, "1440p" },
            { 2560, 1600, nullptr },
            { 3200, 1800, nullptr },
            { 3840, 2160, "4K" },
            { 5120, 2880, "5K" },
        };
        for (const auto& r : kResolutions)
        {
            const QString size = QStringLiteral("%1×%2").arg(r.width).arg(r.rows);
            combo->addItem(r.name != nullptr ? QStringLiteral("%1 (%2)").arg(size, QLatin1String(r.name))
                                             : size,
                           trimScale(double(r.rows) / 448.0));
        }
        combo->addItem(tr("Custom…"), kCustomChoice);
        auto* spin = new QDoubleSpinBox;
        spin->setRange(0.5, 7.0);
        spin->setSingleStep(0.05);
        spin->setDecimals(3);
        spin->setValue(2.0);
        spin->setSuffix(tr("×"));
        spin->setEnabled(false);
        auto* rowsNote = SettingsPage::note(QString());

        auto isCustom = [combo] {
            return combo->currentData().toString() == kCustomChoice;
        };
        auto showRows = [combo, spin, rowsNote, isCustom] {
            const bool automatic = combo->currentIndex() <= 0;
            rowsNote->setText(isCustom() ? tr("Renders %1 rows.").arg(qRound(spin->value() * 448.0))
                                         : tr("Height is exact; width follows the aspect ratio."));
            rowsNote->setVisible(!automatic);
        };
        connect(combo, &QComboBox::currentIndexChanged, this, [this, spin, isCustom, showRows] {
            spin->setEnabled(isCustom());
            showRows();
            markDirty();
        });
        connect(spin, &QDoubleSpinBox::valueChanged, this, [this, showRows] {
            showRows();
            markDirty();
        });
        page->addSetting(s.label, pair(combo, spin), infoFor(s));
        page->addFieldNote(rowsNote);
        rowsNote->setVisible(false);
        registerControl(
            s,
            [combo, spin, isCustom] {
                return isCustom() ? trimScale(spin->value()) : combo->currentData().toString();
            },
            [combo, spin, showRows](const QString& v) {
                const QString t = v.trimmed();
                bool ok = false;
                const double d = t.toDouble(&ok);
                int index = t.isEmpty() ? 0 : -1;
                for (int i = 1; ok && index < 0 && i < combo->count() - 1; ++i)
                {
                    if (qRound(combo->itemData(i).toDouble() * 1000.0) == qRound(d * 1000.0))
                        index = i;
                }
                if (index < 0)
                    index = combo->count() - 1;
                if (ok)
                    spin->setValue(d);
                combo->setCurrentIndex(index);
                spin->setEnabled(index == combo->count() - 1);
                showRows();
            });
    }

    addChoice(page, byKey(group, "msaa"));
    addChoice(page, byKey(group, "aniso"));

    page->beginSection(tr("Frame pacing"));

    // fps_limit: three states in one control, follow the display, unlimited, or an exact number. 0
    // is the game's own spelling of unlimited, so it is a value here and not a third checkbox.
    {
        const Setting& s = byKey(group, "fps_limit");
        auto* monitorBox = new QCheckBox(tr("Match display"));
        auto* spin = new QDoubleSpinBox;
        spin->setRange(0.0, 1000.0);
        spin->setDecimals(2);
        spin->setSingleStep(1.0);
        // The game opens on the primary display.
        const QScreen* primary = QGuiApplication::primaryScreen();
        spin->setValue(primary != nullptr && primary->refreshRate() >= 20.0 ? primary->refreshRate()
                                                                            : 60.0);
        spin->setSuffix(tr(" Hz"));
        spin->setSpecialValueText(tr("No limit"));
        connect(monitorBox, &QCheckBox::toggled, spin, &QWidget::setDisabled);
        connect(monitorBox, &QCheckBox::toggled, this, &MainWindow::markDirty);
        connect(spin, &QDoubleSpinBox::valueChanged, this, &MainWindow::markDirty);
        page->addSetting(s.label, pair(monitorBox, spin), infoFor(s));
        registerControl(
            s,
            [monitorBox, spin] {
                return monitorBox->isChecked() ? QString() : trimNumber(spin->value());
            },
            [monitorBox, spin](const QString& v) {
                bool ok = false;
                const double d = v.toDouble(&ok);
                monitorBox->setChecked(!ok || v.trimmed().isEmpty());
                spin->setDisabled(monitorBox->isChecked());
                if (ok)
                    spin->setValue(d);
            });
    }

    addChoice(page, byKey(group, "vsync"));

    page->beginSection(tr("Window"));

    // aspect takes free text (any W:H or a decimal), so the combo has to be able to hand over to a
    // line edit rather than close the set of values.
    {
        const Setting& s = byKey(group, "aspect");
        auto* combo = new QComboBox;
        combo->addItem(tr("Match the window"), QString());
        for (const char* a : { "16:9", "4:3", "21:9", "32:9", "48:9" })
            combo->addItem(QString::fromLatin1(a), QString::fromLatin1(a));
        combo->addItem(tr("Custom…"), kCustomChoice);
        auto* custom = new QLineEdit;
        custom->setPlaceholderText(tr("width : height"));
        custom->setEnabled(false);
        custom->setMaximumWidth(140);

        auto* aspectNote = SettingsPage::note(QString());

        auto isCustom = [combo] {
            return combo->currentData().toString() == kCustomChoice;
        };
        auto showAspect = [custom, aspectNote, isCustom] {
            const QString text = custom->text().trimmed();
            aspectNote->setVisible(isCustom() && !text.isEmpty());
            const float asked = PortParseAspect(text.toUtf8().constData());
            const float used = PortClampAspect(asked);
            const QString ratio =
                trimScale(qRound(double(used) * 100.0) / 100.0) + QStringLiteral(":1");
            aspectNote->setStyleSheet(QString());
            if (text.compare(QLatin1String("auto"), Qt::CaseInsensitive) == 0)
                aspectNote->setText(tr("Same as Match the window."));
            else if (asked <= 0.0f)
                setBad(aspectNote, tr("✗ Invalid aspect ratio. Use width:height (such as 21:9) "
                                      "or a decimal."));
            else if (used != asked)
                setBad(aspectNote,
                       tr("✗ Must be between 1:1 and 12:1. %1 will be used.").arg(ratio));
            else
                aspectNote->setText(tr("Reads as %1.").arg(ratio));
        };
        connect(combo, &QComboBox::currentIndexChanged, this, [this, custom, isCustom, showAspect] {
            custom->setEnabled(isCustom());
            showAspect();
            markDirty();
        });
        connect(custom, &QLineEdit::textChanged, this, [this, showAspect] {
            showAspect();
            markDirty();
        });
        page->addSetting(s.label, pair(combo, custom), infoFor(s));
        page->addFieldNote(aspectNote);
        aspectNote->setVisible(false);
        registerControl(
            s,
            [combo, custom, isCustom] {
                return isCustom() ? custom->text().trimmed() : combo->currentData().toString();
            },
            [combo, custom, showAspect](const QString& v) {
                const int i = v.isEmpty() ? 0 : combo->findData(v);
                if (i >= 0)
                {
                    combo->setCurrentIndex(i);
                    custom->clear();
                    custom->setEnabled(false);
                }
                else
                {
                    combo->setCurrentIndex(combo->count() - 1);
                    custom->setEnabled(true);
                    custom->setText(v);
                }
                showAspect();
            });
    }

    // backend: every value is offered so the file can say what it likes, but the ones this platform
    // cannot start are greyed rather than hidden, "why is Vulkan missing" is a worse question than
    // "why is it grey".
    {
        QComboBox* combo = addChoice(page, byKey(group, "backend"));
        for (int i = 0; i < combo->count(); ++i)
        {
            const QString v = combo->itemData(i).toString();
            if (backendSupportedHere(v))
            {
                if (isGlBackend(v))
                    combo->setItemText(i, tr("%1 (experimental)").arg(combo->itemText(i)));
                continue;
            }
            combo->setItemData(i, QVariant(0), Qt::UserRole - 1); // disable the item
            combo->setItemText(i, tr("%1 (not available)").arg(combo->itemText(i)));
        }
    }

    addChoice(page, byKey(group, "fullscreen"));
    addSwitch(page, byKey(group, "pause_on_focus_lost"));

    page->finish();
    return page->scrollable();
}

QWidget* MainWindow::buildAudioTab()
{
    auto* page = new SettingsPage;

    // No heading over the first group: a two-setting page does not need one, and "Sound" over a row
    // labelled "Sound" is a heading saying nothing.
    page->beginSection(QString());
    addSwitch(page, Schema::get(QStringLiteral("audio")));
    // Under the control it is about, not at the left margin: this is a note about the switch above
    // it.
    page->addFieldNote(SettingsPage::note(
        tr("Music, effects and commentary volume are set in the game's Options screen.")));

    page->beginSection(tr("Troubleshooting"));
    addSwitch(page, Schema::get(QStringLiteral("log_audio")));

    page->finish();
    return page->scrollable();
}

QWidget* MainWindow::buildInputTab()
{
    auto* inner = new SettingsTabs;
    inner->addTab(buildKeyboardPage(), tr("Keyboard"));
    inner->addTab(buildGamepadPage(), tr("Controller"));
    // The inner tabs change the outer page's height as much as the outer ones do, and nothing else
    // would notice.
    return inner;
}

// The two binding pages lay the controls out the way the pad is: the face buttons and the shoulders
// on one side, the sticks and the d-pad on the other.
namespace {

// `placement` is keyed by Binding::group, which is an untranslated identifier; the hash the
// bindings are filed under has to stay the same in every language or every binding lands in no box.
QGridLayout* bindingGrid(QHash<QString, QFormLayout*>& groups,
                         const QVector<QPair<QString, QPair<int, int>>>& placement)
{
    auto* grid = new QGridLayout;
    grid->setContentsMargins(0, 4, 0, 0);
    grid->setHorizontalSpacing(20);
    grid->setVerticalSpacing(12);
    for (const auto& p : placement)
    {
        auto* box = new QGroupBox(Schema::groupLabel(p.first));
        auto* f = new QFormLayout(box);
        f->setHorizontalSpacing(8);
        f->setVerticalSpacing(6);
        f->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
        groups.insert(p.first, f);
        grid->addWidget(box, p.second.first, p.second.second);
    }
    return grid;
}

} // namespace

QWidget* MainWindow::buildKeyboardPage()
{
    auto* page = new SettingsPage;

    page->beginSection(QString());
    addSwitch(page, Schema::get(QStringLiteral("keyboard")));

    {
        // One explanation for thirty-two buttons, and the part about physical key positions; which
        // matters only on a layout that is not US or UK, behind the info button rather than in
        // front of everyone.
        Setting s;
        s.key = QStringLiteral("key_a");
        s.detail = tr("Click a binding, then press a key. Escape cancels.");
        s.help =
            tr("Bindings are stored as SDL scancodes (physical key positions). On US and UK\n"
               "layouts, the key pressed is the key stored. On AZERTY or Dvorak, the letter\nis "
               "stored instead.\n\nUnnamed keys, Command, Menu, media keys and keys that need a "
               "modifier\nare not accepted.");
        auto* row = new QWidget;
        auto* h = new QHBoxLayout(row);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(6);
        auto* text = SettingsPage::note(s.detail);
        // One line, not wrapped: inside a row that also holds the info button a wrapped label takes
        // its own width hint, which is a word.
        text->setWordWrap(false);
        h->addWidget(text);
        h->addWidget(new InfoButton(HelpText::popover(s), HelpText::popover(s, true)), 0, Qt::AlignVCenter);
        h->addStretch(1);
        page->addFieldNote(row);
    }

    QHash<QString, QFormLayout*> groups;
    page->addBlock([&] {
        auto* holder = new QWidget;
        auto* grid = bindingGrid(groups, {
            { QStringLiteral("Face buttons"), { 0, 0 } },
            { QStringLiteral("Shoulders and Start"), { 1, 0 } },
            { QStringLiteral("D-pad"), { 2, 0 } },
            { QStringLiteral("Control stick"), { 0, 1 } },
            { QStringLiteral("C-stick"), { 1, 1 } },
        });
        holder->setLayout(grid);
        return holder;
    }());

    for (const Binding& b : Schema::keyboardBindings())
    {
        auto* button = new KeyCaptureButton;
        button->setMinimumWidth(96);
        button->setMaximumWidth(150);
        button->setBinding(b.def);
        button->setToolTip(tr("Click, then press the key for %1.")
                               .arg(Schema::bindingLabel(b.label)));
        // Three boxes on this page have a row called "Up".
        button->setAccessibleName(tr("%1, %2", "controller group, then the button in it")
                                      .arg(Schema::groupLabel(b.group),
                                           Schema::bindingLabel(b.label)));
        groups.value(b.group)->addRow(Schema::bindingLabel(b.label), button);
        m_keyButtons.push_back(button);

        Setting s;
        s.key = b.key;
        s.section = b.section;
        s.label = b.label;
        s.def = b.def;
        registerControl(
            s, [button] { return button->binding(); },
            [button](const QString& val) { button->setBinding(val); });
        connect(button, &KeyCaptureButton::bindingChanged, this, [this] {
            markDirty();
            updateConflicts();
        });
    }

    m_conflictLabel = new QLabel;
    m_conflictLabel->setWordWrap(true);
    m_conflictLabel->setStyleSheet(QStringLiteral("color: #d64545;"));
    page->addBlock(m_conflictLabel);

    auto* resetKeys = new QPushButton(tr("Reset Keyboard"));
    connect(resetKeys, &QPushButton::clicked, this, [this] {
        for (const Binding& b : Schema::keyboardBindings())
            setValueByKey(b.key, b.def);
        setValueByKey(QStringLiteral("keyboard"),
                      Schema::get(QStringLiteral("keyboard")).def);
        updateConflicts();
        markDirty();
    });
    auto* row = new QWidget;
    auto* h = new QHBoxLayout(row);
    h->setContentsMargins(0, 4, 0, 0);
    h->addStretch(1);
    h->addWidget(resetKeys);
    page->addBlock(row);

    page->finish();
    return page->scrollable();
}

QWidget* MainWindow::buildGamepadPage()
{
    auto* page = new SettingsPage;

    page->beginSection(tr("Buttons"));

    {
        Setting s;
        s.key = QStringLiteral("pad_a");
        s.detail = tr("Select the controller button for each GameCube button.");
        s.help =
            tr("Names are SDL controller positions: `a` is the bottom face button on every\n"
               "controller, whatever its label.\n\nButtons cannot be captured here. To see the "
               "bindings in effect, run the\ngame with the pad probe on.");
        auto* row = new QWidget;
        auto* h = new QHBoxLayout(row);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(6);
        auto* text = SettingsPage::note(s.detail);
        text->setWordWrap(false);
        h->addWidget(text);
        h->addWidget(new InfoButton(HelpText::popover(s), HelpText::popover(s, true)), 0, Qt::AlignVCenter);
        h->addStretch(1);
        page->addNote(row);
    }

    QHash<QString, QFormLayout*> groups;
    page->addBlock([&] {
        auto* holder = new QWidget;
        holder->setLayout(bindingGrid(groups, {
            { QStringLiteral("Face buttons"), { 0, 0 } },
            { QStringLiteral("Shoulders and Start"), { 1, 0 } },
            { QStringLiteral("D-pad"), { 0, 1 } },
        }));
        return holder;
    }());

    for (const Binding& b : Schema::gamepadBindings())
    {
        auto* combo = new QComboBox;
        combo->setMinimumWidth(140);
        combo->setMaximumWidth(180);
        for (const QString& n : Schema::padButtonNames())
            combo->addItem(n, n);
        combo->setCurrentIndex(combo->findData(b.def));
        combo->setAccessibleName(tr("%1, %2", "controller group, then the button in it")
                                     .arg(Schema::groupLabel(b.group),
                                          Schema::bindingLabel(b.label)));
        groups.value(b.group)->addRow(Schema::bindingLabel(b.label), combo);

        Setting s;
        s.key = b.key;
        s.section = b.section;
        s.label = b.label;
        s.def = b.def;
        connect(combo, &QComboBox::currentIndexChanged, this, &MainWindow::markDirty);
        registerControl(
            s, [combo] { return combo->currentData().toString(); },
            [combo](const QString& val) {
                int i = combo->findData(val);
                if (i < 0)
                {
                    combo->addItem(tr("%1 (from file)").arg(val), val);
                    i = combo->count() - 1;
                }
                combo->setCurrentIndex(i);
            });
    }

    page->beginSection(tr("Sticks and triggers"));

    auto addSlider = [this, page](const Setting& s, double lo, double hi, int scale,
                                  const QString& suffix) {
        auto* slider = new QSlider(Qt::Horizontal);
        slider->setRange(int(lo * scale + 0.5), int(hi * scale + 0.5));
        slider->setMinimumWidth(180);
        slider->setMaximumWidth(240);
        auto* readout = new QLabel;

        const bool percent = scale == 1;
        auto text = [percent, scale, suffix](int raw) {
            return percent ? QStringLiteral("%1%2").arg(raw).arg(suffix)
                           : trimNumber(double(raw) / scale) + suffix;
        };
        // As wide as the widest value any slider shows, so the info buttons line up just past it.
        const QFontMetrics fm = readout->fontMetrics();
        readout->setFixedWidth(qMax(fm.horizontalAdvance(QStringLiteral("100%")),
                                    qMax(fm.horizontalAdvance(text(slider->minimum())),
                                         fm.horizontalAdvance(text(slider->maximum())))));
        QObject::connect(slider, &QSlider::valueChanged, readout,
                         [readout, text](int val) { readout->setText(text(val)); });
        readout->setText(text(slider->value()));
        connect(slider, &QSlider::valueChanged, this, &MainWindow::markDirty);
        page->addSetting(s.label, pair(slider, readout), infoFor(s));
        registerControl(
            s,
            [slider, percent, scale] {
                return percent ? QString::number(slider->value())
                               : trimNumber(double(slider->value()) / scale);
            },
            [slider, scale](const QString& val) {
                bool ok = false;
                const double d = val.toDouble(&ok);
                if (ok)
                    slider->setValue(int(d * scale + 0.5));
            });
    };

    addSwitch(page, Schema::get(QStringLiteral("pad_swap_sticks")));
    addSlider(Schema::get(QStringLiteral("pad_deadzone")), 0.0, 0.9, 100, QString());
    // The top is the game's default, 0.95; a lower one would clamp it on load and save a change.
    addSlider(Schema::get(QStringLiteral("pad_trigger_threshold")), 0.1, 0.95, 100, QString());
    addSwitch(page, Schema::get(QStringLiteral("pad_rumble")));
    addSlider(Schema::get(QStringLiteral("pad_rumble_strength")), 0, 100, 1,
              QStringLiteral("%"));

    auto* resetPad = new QPushButton(tr("Reset Controller"));
    connect(resetPad, &QPushButton::clicked, this, [this] {
        for (const Binding& b : Schema::gamepadBindings())
            setValueByKey(b.key, b.def);
        for (const Setting& s : Schema::inputSwitches())
        {
            if (s.key.startsWith(QLatin1String("pad_")))
                setValueByKey(s.key, s.def);
        }
        markDirty();
    });
    auto* row = new QWidget;
    auto* h = new QHBoxLayout(row);
    h->setContentsMargins(0, 4, 0, 0);
    h->addStretch(1);
    h->addWidget(resetPad);
    page->addBlock(row);

    page->finish();
    return page->scrollable();
}

QWidget* MainWindow::buildGameTab()
{
    auto* page = new SettingsPage;

    page->beginSection(tr("Game files"));

    {
        const Setting& s = Schema::get(QStringLiteral("data"));
        m_dataEdit = new QLineEdit;
        m_dataEdit->setPlaceholderText(tr("Found automatically"));
        m_dataEdit->setMinimumWidth(200);
        auto* chooseImage = new QPushButton(tr("Disc Image…"));
        chooseImage->setToolTip(tr("Play directly from a disc image."));
        auto* chooseFolder = new QPushButton(tr("Folder…"));
        chooseFolder->setToolTip(tr("Use files already extracted from the disc."));
        m_dataState = SettingsPage::note(QString());

        connect(chooseImage, &QPushButton::clicked, this, &MainWindow::onChooseImage);
        connect(chooseFolder, &QPushButton::clicked, this, &MainWindow::onChooseFolder);
        connect(m_dataEdit, &QLineEdit::textChanged, this, [this] {
            updateDataState();
            markDirty();
        });

        auto* row = new QWidget;
        auto* h = new QHBoxLayout(row);
        h->setContentsMargins(0, 0, 0, 0);
        h->setSpacing(6);
        h->addWidget(m_dataEdit, 1);
        h->addWidget(chooseImage);
        h->addWidget(chooseFolder);

        page->addSetting(s.label, row, infoFor(s));
        page->addFieldNote(m_dataState);

        m_extractRow = new QWidget;
        auto* er = new QHBoxLayout(m_extractRow);
        er->setContentsMargins(0, 2, 0, 0);
        er->setSpacing(8);
        m_extractButton = new QPushButton(tr("Extract Files…"));
        m_extractButton->setObjectName(QStringLiteral("extractFiles"));
        connect(m_extractButton, &QPushButton::clicked, this, &MainWindow::onExtract);
        er->addWidget(m_extractButton, 0, Qt::AlignTop);
        er->addWidget(SettingsPage::note(tr("Optional. Copies the image's files to a folder so the "
                                            "image is no longer needed.")),
                      1);
        m_extractRow->setVisible(false);
        page->addFieldNote(m_extractRow);

        registerControl(
            s, [this] { return m_dataEdit->text().trimmed(); },
            [this](const QString& val) { m_dataEdit->setText(val); });
    }

    page->beginSection(tr("Texture packs"));

    QWidget* packsLead = nullptr;
    QWidget* dumpLead = nullptr;
    {
        const Setting& s = Schema::get(QStringLiteral("textures"));
        m_texturesBox = new QCheckBox(s.check);
        auto* open = new QPushButton(tr("Open Folder"));
        connect(open, &QPushButton::clicked, this, [this] {
            openFolder(AppPaths::modsFolder(userFolder(), QStringLiteral("textures")));
        });
        page->addSetting(s.label, switchRow(m_texturesBox, infoFor(s), open, &packsLead));
        connect(m_texturesBox, &QCheckBox::toggled, this, [this] {
            updateTexturePacks();
            markDirty();
        });
        registerControl(
            s, [this] { return m_texturesBox->isChecked() ? m_texturesFolder : QStringLiteral("0"); },
            [this](const QString& v) {
                const bool off = falsy(v);
                m_texturesFolder = off || truthy(v) ? QString() : v.trimmed();
                m_texturesBox->setChecked(!off);
            });

        m_packList = SettingsPage::note(QString());
        m_packList->setObjectName(QStringLiteral("texturePacks"));
        m_packList->setTextFormat(Qt::PlainText);

        // With several packs the list becomes a choice of one of them.
        const Setting& p = Schema::get(QStringLiteral("texture_pack"));
        m_packChoice = new QComboBox;
        m_packChoice->setSizeAdjustPolicy(QComboBox::AdjustToContents);
        m_packChoiceRow = new QWidget;
        auto* ph = new QHBoxLayout(m_packChoiceRow);
        ph->setContentsMargins(0, 0, 0, 0);
        ph->setSpacing(6);
        ph->addWidget(m_packChoice);
        ph->addWidget(infoFor(p), 0, Qt::AlignVCenter);
        ph->addStretch(1);
        connect(m_packChoice, &QComboBox::currentIndexChanged, this, [this] {
            if (m_fillingPacks)
                return;
            m_packWanted = m_packChoice->currentData().toString();
            markDirty();
        });
        registerControl(
            p, [this] { return m_packWanted; },
            [this](const QString& v) {
                m_packWanted = v.trimmed();
                updateTexturePacks();
            });

        auto* under = new QWidget;
        auto* uv = new QVBoxLayout(under);
        uv->setContentsMargins(0, 0, 0, 0);
        uv->setSpacing(0);
        uv->addWidget(m_packList);
        uv->addWidget(m_packChoiceRow);
        page->addFieldNote(under);
    }

    {
        const Setting& s = Schema::get(QStringLiteral("texture_dump"));
        m_dumpBox = new QCheckBox(s.check);
        auto* open = new QPushButton(tr("Open Folder"));
        connect(open, &QPushButton::clicked, this, [this] {
            openFolder(m_dumpFolder.isEmpty() ? userFolder() + QStringLiteral("/texture_dumps")
                                              : gamePath(m_dumpFolder));
        });
        page->addSetting(s.label, switchRow(m_dumpBox, infoFor(s), open, &dumpLead));
        connect(m_dumpBox, &QCheckBox::toggled, this, &MainWindow::markDirty);
        registerControl(
            s,
            [this] {
                if (!m_dumpBox->isChecked())
                    return QStringLiteral("0");
                return m_dumpFolder.isEmpty() ? QStringLiteral("1") : m_dumpFolder;
            },
            [this](const QString& v) {
                const bool on = !v.trimmed().isEmpty() && !falsy(v);
                m_dumpFolder = on && !truthy(v) ? v.trimmed() : QString();
                m_dumpBox->setChecked(on);
            });
    }

    {
        const int w = qMax(packsLead->sizeHint().width(), dumpLead->sizeHint().width());
        packsLead->setMinimumWidth(w);
        dumpLead->setMinimumWidth(w);
    }

    page->beginSection(tr("Storage"));

    {
        const Setting& s = Schema::get(QStringLiteral("portable"));
        m_portableBox = new QCheckBox(s.check);
        page->addSetting(s.label, m_portableBox, infoFor(s));
        m_portableState = SettingsPage::note(QString());
        m_portableState->setTextFormat(Qt::PlainText);
        page->addFieldNote(m_portableState);
        connect(m_portableBox, &QCheckBox::toggled, this, [this] {
            updatePortableState();
            updateTexturePacks();
            markDirty();
        });
        registerControl(
            s, [this] { return QString(m_portableBox->isChecked() ? "1" : "0"); },
            [this](const QString& v) { m_portableBox->setChecked(truthy(v)); });
    }

    page->beginSection(tr("Options"));

    // The combo's first entry writes nothing, which leaves each disc its own language.
    m_language = addChoice(page, Schema::get(QStringLiteral("language")));
    m_languageState = SettingsPage::note(QString());
    page->addFieldNote(m_languageState);

    // Not in strikers.ini, which is the game's.
    {
        Setting s;
        s.label = tr("Window language");
        s.detail = tr("Language of this window. The game language is set under Language above.");
        auto* combo = new QComboBox;
        combo->addItem(
            tr("Automatic (%1)").arg(WindowLanguage::name(WindowLanguage::resolve(QString()))),
            QString());
        combo->addItem(WindowLanguage::name(QStringLiteral("en")), QStringLiteral("en"));
        for (const QString& tag : WindowLanguage::available())
            combo->addItem(WindowLanguage::name(tag), tag);
        combo->setCurrentIndex(qMax(0, combo->findData(WindowLanguage::chosen())));
        page->addSetting(s.label, combo,
                         new InfoButton(HelpText::popover(s), HelpText::popover(s, true)));
        connect(combo, &QComboBox::activated, this, [this, combo] {
            const QString tag = combo->currentData().toString();
            if (tag == WindowLanguage::chosen())
                return;
            WindowLanguage::choose(tag);
            emit windowLanguageChosen();
        });
    }

    addSwitch(page, Schema::get(QStringLiteral("unlock_all")));
    addSwitch(page, Schema::get(QStringLiteral("discord")));

    // A switch whose value is not "1": the menu opens on `menu` and the compact overlay on `1`, and
    // turning this on is meant to show the menu.
    {
        const Setting& s = Schema::get(QStringLiteral("overlay"));
        auto* box = new QCheckBox(s.check);
        page->addSetting(s.label, box, infoFor(s));
        connect(box, &QCheckBox::toggled, this, &MainWindow::markDirty);
        registerControl(
            s, [box] { return QString(box->isChecked() ? "menu" : "0"); },
            [box](const QString& v) {
                const QString t = v.trimmed();
                box->setChecked(!t.isEmpty() && t != QLatin1String("0"));
            });
        // Under the control, not behind the info button: a menu that opens by itself and cannot be
        // dismissed is a bug report, and the key that dismisses it is not one anybody guesses.
        page->addFieldNote(
            SettingsPage::note(tr("The menu opens when the game starts. F1 opens and closes it.")));
    }

    page->addBlock(buildAdvancedBox());

    page->finish();
    return page->scrollable();
}

// Every key in the file the window has no control for.
QWidget* MainWindow::buildAdvancedBox()
{
    auto* holder = new QWidget;
    auto* v = new QVBoxLayout(holder);
    v->setContentsMargins(0, 16, 0, 0);
    v->setSpacing(6);

    auto* rule = new QFrame;
    rule->setFrameShape(QFrame::HLine);
    rule->setFrameShadow(QFrame::Plain);
    rule->setFixedHeight(1);
    QPalette rp = rule->palette();
    rp.setColor(QPalette::WindowText, rp.color(QPalette::Mid));
    rule->setPalette(rp);
    v->addWidget(rule);

    auto* toggle = new QToolButton;
    toggle->setText(tr("Advanced"));
    toggle->setCheckable(true);
    toggle->setChecked(false);
    toggle->setArrowType(Qt::RightArrow);
    toggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toggle->setAutoRaise(true);
    // autoRaise is not enough on every style: a QToolButton with text still draws its own frame,
    // and a framed button here reads as an action rather than as a section that opens.
    toggle->setStyleSheet(QStringLiteral("QToolButton { border: none; padding: 2px 0; }"));
    QFont bold = toggle->font();
    bold.setBold(true);
    toggle->setFont(bold);
    v->addWidget(toggle, 0, Qt::AlignLeft);

    auto* body = new QWidget;
    auto* bv = new QVBoxLayout(body);
    bv->setContentsMargins(0, 4, 0, 0);
    bv->setSpacing(6);
    bv->addWidget(SettingsPage::note(
        tr("Other settings in the file, kept as written. An empty row is off in the file; enter a "
           "value and save to turn it on, or clear it to turn it off.")));

    m_advanced = new QTableWidget(0, 3);
    m_advanced->setHorizontalHeaderLabels({ tr("Setting"), tr("Value"), tr("State") });
    m_advanced->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_advanced->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    // Not ResizeToContents: a commented-out declaration in the example carries its own trailing `;
    // ` note, which config.c reads as part of the value and this column would then be three hundred
    // pixels of it.
    m_advanced->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Interactive);
    m_advanced->horizontalHeader()->resizeSection(2, 150);
    m_advanced->verticalHeader()->setVisible(false);
    m_advanced->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_advanced->setMinimumHeight(140);
    m_advanced->setMaximumHeight(220);
    connect(m_advanced, &QTableWidget::cellChanged, this, [this](int, int) {
        if (!m_loading)
            markDirty();
    });
    bv->addWidget(m_advanced);

    auto* buttons = new QHBoxLayout;
    auto* add = new QPushButton(tr("Add"));
    auto* del = new QPushButton(tr("Remove"));
    connect(add, &QPushButton::clicked, this, [this] {
        const int r = m_advanced->rowCount();
        m_advanced->insertRow(r);
        m_advanced->setItem(r, 0, new QTableWidgetItem(QString()));
        m_advanced->setItem(r, 1, new QTableWidgetItem(QString()));
        auto* state = new QTableWidgetItem(tr("new"));
        state->setFlags(state->flags() & ~Qt::ItemIsEditable);
        m_advanced->setItem(r, 2, state);
        m_advanced->editItem(m_advanced->item(r, 0));
        markDirty();
    });
    connect(del, &QPushButton::clicked, this, [this] {
        const int r = m_advanced->currentRow();
        if (r >= 0)
        {
            m_advanced->removeRow(r);
            markDirty();
        }
    });
    buttons->addStretch(1);
    buttons->addWidget(add);
    buttons->addWidget(del);
    bv->addLayout(buttons);

    body->setVisible(false);
    v->addWidget(body);

    connect(toggle, &QToolButton::toggled, this, [toggle, body](bool on) {
        toggle->setArrowType(on ? Qt::DownArrow : Qt::RightArrow);
        body->setVisible(on);
        // Deliberately no fitToCurrentTab() here: that is what a tab change does, and on a
        // disclosure it threw the window hundreds of pixels taller and snapped back, under the
        // control being clicked.
    });

    return holder;
}

// Load, save, revert.

void MainWindow::openFile(const QString& path)
{
    m_path = path;
    m_ini.clear();
    QString error;
    const bool exists = QFileInfo::exists(path);
    if (exists && !m_ini.load(path, &error))
        QMessageBox::warning(this, tr("Super Mario Strikers Settings"), error);

    QString seeded;
    if (!exists)
    {
        // A first run has no strikers.ini: the archive ships the example, and the player is
        // expected to rename it.
        const QString example = AppPaths::findExample(path, AppPaths::archiveRoot());
        if (!example.isEmpty() && m_ini.load(example, &error))
            seeded = example;
    }

    loadIntoUi();
    setDirty(false);

    if (exists)
        showPath(QDir::toNativeSeparators(path));
    else if (!seeded.isEmpty())
        showPath(tr("%1 (new; created from %2 on save)")
                     .arg(QDir::toNativeSeparators(path), QFileInfo(seeded).fileName()));
    else
        showPath(tr("%1 (new; created on save)").arg(QDir::toNativeSeparators(path)));
}

MainWindow::Snapshot MainWindow::snapshot()
{
    Snapshot s;
    s.path = m_path;
    s.dirty = m_dirty;
    if (m_dirty)
    {
        collectFromUi();
        s.ini = m_ini;
    }
    s.tab = m_tabs->currentIndex();
    s.geometry = saveGeometry();
    s.rememberGeometry = m_rememberGeometry;
    return s;
}

void MainWindow::restore(const Snapshot& s)
{
    openFile(s.path);
    if (s.dirty)
    {
        m_ini = s.ini;
        loadIntoUi();
        setDirty(true);
    }
    m_tabs->setCurrentIndex(s.tab);

    // No smaller than this language needs: German is wider than English.
    const QSize fitted = size();
    m_rememberGeometry = s.rememberGeometry;
    restoreGeometry(s.geometry);
    resize(qMax(width(), fitted.width()), qMax(height(), fitted.height()));
}

void MainWindow::showPath(const QString& text)
{
    m_pathText = text;
    if (m_pathLabel == nullptr)
        return;
    // Elided from the middle: the interesting halves of a path are its start and its file name, and
    // a status bar that widens the window to fit one is a status bar deciding how big the window
    // is.
    const int room = qMax(120, width() - 32);
    m_pathLabel->setText(m_pathLabel->fontMetrics().elidedText(text, Qt::ElideMiddle, room));
    m_pathLabel->setToolTip(text);
}

void MainWindow::loadIntoUi()
{
    m_loading = true;
    for (const Control& c : m_controls)
    {
        const QString norm = IniFile::normalise(c.key);
        c.set(m_ini.has(norm) ? m_ini.value(norm) : c.def);
    }
    refreshAdvancedTable();
    updateConflicts();
    updateDataState();
    updateTexturePacks();
    updatePortableState();
    m_loading = false;
}

void MainWindow::collectFromUi()
{
    for (const Control& c : m_controls)
    {
        const QString v = c.get();
        if (v == c.def)
            m_ini.unset(c.key);
        else
            m_ini.set(c.key, v, c.section);
    }
    applyAdvancedTable();
}

void MainWindow::refreshAdvancedTable()
{
    m_loading = true;
    m_advanced->setRowCount(0);

    auto addRowFor = [this](const QString& key, const QString& value, const QString& state) {
        const int r = m_advanced->rowCount();
        m_advanced->insertRow(r);
        auto* k = new QTableWidgetItem(key.toLower());
        k->setFlags(k->flags() & ~Qt::ItemIsEditable);
        m_advanced->setItem(r, 0, k);
        m_advanced->setItem(r, 1, new QTableWidgetItem(value));
        auto* s = new QTableWidgetItem(state);
        s->setFlags(s->flags() & ~Qt::ItemIsEditable);
        s->setToolTip(state);
        m_advanced->setItem(r, 2, s);
    };

    for (const QString& key : m_ini.liveKeys())
    {
        if (!Schema::isOwned(key))
            addRowFor(key, m_ini.value(key), tr("on"));
    }
    for (const QString& key : m_ini.commentedKeys())
    {
        if (Schema::isOwned(key))
            continue;
        const QString suggested = m_ini.commentedValue(key);
        addRowFor(key, QString(),
                  suggested.isEmpty() ? tr("off") : tr("off (suggests %1)").arg(suggested));
    }
    m_loading = false;
}

void MainWindow::applyAdvancedTable()
{
    QStringList present;
    for (int r = 0; r < m_advanced->rowCount(); ++r)
    {
        const QTableWidgetItem* k = m_advanced->item(r, 0);
        const QTableWidgetItem* val = m_advanced->item(r, 1);
        if (k == nullptr)
            continue;
        const QString key = k->text().trimmed();
        if (key.isEmpty() || Schema::isOwned(key))
            continue;
        present << IniFile::normalise(key);
        const QString value = val == nullptr ? QString() : val->text().trimmed();
        if (value.isEmpty())
            m_ini.unset(key);
        else
            m_ini.set(key, value, m_ini.sectionOf(key).isEmpty()
                                      ? QStringLiteral("advanced")
                                      : m_ini.sectionOf(key));
    }

    // A key that was in the file and is no longer in the table was deleted.
    for (const QString& key : m_ini.liveKeys())
    {
        if (!Schema::isOwned(key) && !present.contains(key))
            m_ini.remove(key);
    }
}

bool MainWindow::saveTo(const QString& path, QString* error)
{
    const bool wasPortable = AppPaths::iniIsPortable(path);
    collectFromUi();
    const QString aspect = m_ini.value(QStringLiteral("aspect")).trimmed();
    if (!aspect.isEmpty() && aspect.compare(QLatin1String("auto"), Qt::CaseInsensitive) != 0 &&
        PortParseAspect(aspect.toUtf8().constData()) <= 0.0f)
    {
        *error = tr("invalid aspect ratio \"%1\".").arg(aspect);
        return false;
    }
    if (!m_ini.save(path, error))
        return false;
    const bool portable = m_portableBox != nullptr && m_portableBox->isChecked();
    if (portable && !wasPortable && offerPortableMove())
    {
        collectFromUi();
        if (!m_ini.save(path, error))
            return false;
    }
    AppPaths::keepAppSettings(portable);
    return true;
}

bool MainWindow::offerPortableMove()
{
    const QString from = AppPaths::userFolder();
    const QString to = AppPaths::portableUserFolder();
    if (m_ini.has(QStringLiteral("USER_DIR")) || QFileInfo::exists(to) ||
        QDir(from).isEmpty(QDir::AllEntries | QDir::NoDotAndDotDot) ||
        AppPaths::movedPath(AppPaths::archiveRoot(), from) != AppPaths::archiveRoot())
        return false;

    const auto answer =
        QMessageBox::question(this, tr("Portable"),
                              tr("Move saves, controls and shader cache to the game folder?\n\n%1")
                                  .arg(QDir::toNativeSeparators(from)));
    if (answer != QMessageBox::Yes)
        return false;

    // Fallbacks off, or macOS lists the global domain's keys too.
    QSettings system;
    system.setFallbacksEnabled(false);
    const QStringList keys = system.allKeys();
    QHash<QString, QVariant> own;
    for (const QString& k : keys)
        own.insert(k, system.value(k));

    QString failed;
    if (!AppPaths::moveToPortable(from, AppPaths::archiveRoot(), &failed))
    {
        QMessageBox::warning(this, tr("Portable"),
                             tr("Could not move %1. Items already moved remain in the game "
                                "folder.")
                                 .arg(failed));
    }

    // Paths into the moved folder follow it, relative so the game folder stays movable.
    for (const char* key : { "data", "textures", "texture_dump" })
    {
        const QString k = QString::fromLatin1(key);
        const QString was = valueByKey(k);
        const QString now = AppPaths::movedPath(gamePath(was), from);
        if (!was.isEmpty() && now != gamePath(was) && QFileInfo::exists(gamePath(now)))
            setValueByKey(k, now);
    }
    AppPaths::keepAppSettings(true);
    QSettings portable;
    for (auto it = own.constBegin(); it != own.constEnd(); ++it)
        portable.setValue(it.key(), it.value());
    portable.sync();
    AppPaths::keepAppSettings(false);
    QSettings().clear();
    return true;
}

void MainWindow::onSave()
{
    QString error;
    if (!saveTo(m_path, &error))
    {
        QMessageBox::critical(this, tr("Super Mario Strikers Settings"),
                              tr("Could not save: %1").arg(error));
        return;
    }
    setDirty(false);
    showPath(tr("Saved to %1").arg(QDir::toNativeSeparators(m_path)));
    refreshAdvancedTable();
}

void MainWindow::onRevert()
{
    openFile(m_path);
}

void MainWindow::onResetAll()
{
    const auto answer = QMessageBox::question(
        this, tr("Reset to Defaults"),
        tr("Reset all settings to their defaults?\n\nSettings under Advanced are not changed. "
           "Nothing is saved until you click Save."));
    if (answer != QMessageBox::Yes)
        return;

    for (const Control& c : m_controls)
        c.set(c.def);
    updateConflicts();
    updateDataState();
    setDirty(true);
}

void MainWindow::updatePlayButton()
{
    QString reason;
    const QString exe = AppPaths::findGame(AppPaths::archiveRoot(), &reason);
    m_playButton->setEnabled(!exe.isEmpty());
    m_playButton->setToolTip(exe.isEmpty() ? reason
                                           : tr("Save and start %1.")
                                                 .arg(QDir::toNativeSeparators(exe)));
}

void MainWindow::onPlay()
{
    QString reason;
    const QString exe = AppPaths::findGame(AppPaths::archiveRoot(), &reason);
    if (exe.isEmpty())
    {
        QMessageBox::warning(this, tr("Play"), reason);
        return;
    }

    QString error;
    if (!saveTo(m_path, &error))
    {
        QMessageBox::critical(this, tr("Play"), tr("Could not save settings: %1").arg(error));
        return;
    }
    setDirty(false);

    if (!QProcess::startDetached(exe, {}, QFileInfo(exe).absolutePath()))
    {
        QMessageBox::critical(this, tr("Play"),
                              tr("Could not start %1.").arg(QDir::toNativeSeparators(exe)));
        return;
    }
    close();
}

void MainWindow::onChooseImage()
{
    const QString current = m_dataEdit->text().trimmed();
    const QString start = current.isEmpty() ? AppPaths::archiveRoot()
                                            : QFileInfo(current).absolutePath();

    const QStringList filters = {
        tr("GameCube disc images (%1)").arg(DiscImage::nameFilters().join(QLatin1Char(' '))),
        tr("ISO or GCM images, including NKit (*.iso *.gcm)"),
        tr("Compressed ISO (*.ciso)"),
        tr("Dolphin GCZ (*.gcz)"),
        tr("All files (*)"),
    };
    const QString file =
        QFileDialog::getOpenFileName(this, tr("Select the Super Mario Strikers disc image"), start,
                                     filters.join(QStringLiteral(";;")));
    if (!file.isEmpty())
        m_dataEdit->setText(file);
}

void MainWindow::onChooseFolder()
{
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Select the game files folder"),
                                                          m_dataEdit->text().trimmed());
    if (dir.isEmpty())
        return;
    const QDir chosen(dir);
    if (!chosen.exists(QStringLiteral("common.ini"))
        && chosen.exists(QStringLiteral("files/common.ini")))
    {
        m_dataEdit->setText(chosen.filePath(QStringLiteral("files")));
        return;
    }
    m_dataEdit->setText(dir);
}

QString MainWindow::extractTarget(const QString& gameId, qint64 bytes)
{
    QString target = DiscImage::defaultExtractDir(
        gameId, m_portableBox != nullptr && m_portableBox->isChecked() ? userFolder() : QString());
    for (;;)
    {
        const QString files = QDir(target).filePath(QStringLiteral("files"));
        QMessageBox box(this);
        box.setWindowTitle(tr("Extract Files"));
        QAbstractButton* go = nullptr;
        QAbstractButton* use = nullptr;

        if (QFileInfo::exists(QDir(files).filePath(QStringLiteral("common.ini")))
            && DiscImage::extractedGameId(files) == gameId)
        {
            box.setIcon(QMessageBox::Question);
            box.setText(tr("This disc is already extracted to:\n\n%1\n\nUse this folder?")
                            .arg(QDir::toNativeSeparators(target)));
            use = box.addButton(tr("Use It"), QMessageBox::AcceptRole);
            box.setDefaultButton(static_cast<QPushButton*>(use));
        }
        else if (QFileInfo::exists(target))
        {
            box.setIcon(QMessageBox::Warning);
            box.setText(tr("%1 already exists and is not from this disc. Select a new folder.")
                            .arg(QDir::toNativeSeparators(target)));
        }
        else
        {
            box.setIcon(QMessageBox::Question);
            box.setText(tr("Extract the game files to this folder?\n\n%1\n\nRequires %2 MB. "
                           "The disc image is not changed.")
                            .arg(QDir::toNativeSeparators(target))
                            .arg((bytes + (1 << 20) - 1) >> 20));
            go = box.addButton(tr("Extract"), QMessageBox::AcceptRole);
            box.setDefaultButton(static_cast<QPushButton*>(go));
        }
        const QAbstractButton* other =
            box.addButton(tr("Choose Another Folder…"), QMessageBox::ActionRole);
        box.addButton(QMessageBox::Cancel);
        box.exec();

        const QAbstractButton* clicked = box.clickedButton();
        if (clicked != nullptr && clicked == use)
        {
            m_dataEdit->setText(files);
            return QString();
        }
        if (clicked != nullptr && clicked == go)
            return target;
        if (clicked != other)
            return QString();

        const QString dir = QFileDialog::getExistingDirectory(
            this, tr("Select a destination for the game files"), QFileInfo(target).absolutePath());
        if (dir.isEmpty())
            return QString();
        target = QDir(dir).filePath(gameId);
    }
}

void MainWindow::onExtract()
{
    if (m_extractThread != nullptr)
        return;

    const QString image = m_dataEdit->text().trimmed();
    const DiscImage::Info info = DiscImage::inspect(image);
    if (!info.ok)
    {
        QMessageBox::warning(this, tr("Extract Files"), info.error);
        return;
    }
    const QString target = extractTarget(info.gameId, info.bytes);
    if (target.isEmpty())
        return;

    std::atomic<qint64> done{ 0 };
    std::atomic<qint64> total{ 0 };
    QString error;
    DiscImage::Result result = DiscImage::Result::Failed;
    m_extractCancel = false;
    m_extractThread = QThread::create([&] {
        result = DiscImage::extract(
            image, target,
            [&](qint64 d, qint64 t) {
                done = d;
                total = t;
            },
            m_extractCancel, &error);
    });

    QProgressDialog progress(tr("Extracting game files…"), tr("Cancel"), 0, 1000, this);
    progress.setWindowTitle(tr("Extract Files"));
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    progress.setAutoClose(false);
    progress.setAutoReset(false);
    // Escape rejects the dialog without emitting canceled.
    connect(&progress, &QProgressDialog::canceled, this, [this] { m_extractCancel = true; });
    connect(&progress, &QDialog::rejected, this, [this] { m_extractCancel = true; });

    QTimer tick;
    tick.setInterval(50);
    connect(&tick, &QTimer::timeout, &progress, [&] {
        const qint64 t = total;
        const qint64 d = done;
        if (t <= 0 || m_extractCancel)
            return;
        progress.setValue(int(d * 1000 / t));
        progress.setLabelText(tr("Extracting game files… %1 of %2 MB").arg(d >> 20).arg(t >> 20));
    });

    QEventLoop loop;
    connect(m_extractThread, &QThread::finished, &loop, &QEventLoop::quit);
    m_extractButton->setEnabled(false);
    m_extractThread->start();
    tick.start();
    progress.show();
    loop.exec();

    tick.stop();
    progress.close();
    m_extractThread->wait();
    delete m_extractThread;
    m_extractThread = nullptr;
    m_extractButton->setEnabled(true);

    if (result == DiscImage::Result::Cancelled)
        return;
    if (result == DiscImage::Result::Failed)
    {
        QMessageBox::critical(this, tr("Extract Files"),
                              tr("Could not extract the game files. No files were left "
                                 "behind.\n\n%1")
                                  .arg(error));
        return;
    }

    m_dataEdit->setText(QDir(target).filePath(QStringLiteral("files")));
    QMessageBox::information(
        this, tr("Extract Files"),
        tr("The game files are in:\n\n%1\n\nThe game now uses this folder. The disc image can "
           "be moved or deleted. Click Save to keep this change.")
            .arg(QDir::toNativeSeparators(target)));
}

void MainWindow::updateConflicts()
{
    const QVector<Binding>& bindings = Schema::keyboardBindings();
    QHash<QString, int> counts;
    for (int i = 0; i < bindings.size() && i < m_keyButtons.size(); ++i)
        counts[m_keyButtons[i]->binding()]++;

    QStringList clashes;
    for (int i = 0; i < bindings.size() && i < m_keyButtons.size(); ++i)
    {
        const QString name = m_keyButtons[i]->binding();
        const bool clash = counts.value(name) > 1 && !name.isEmpty();
        m_keyButtons[i]->setConflict(clash);
        if (clash && !clashes.contains(name))
            clashes << name;
    }

    if (m_conflictLabel != nullptr)
    {
        m_conflictLabel->setText(
            clashes.isEmpty()
                ? QString()
                : tr("%1 is assigned to more than one control. Only the first assignment works.")
                      .arg(clashes.join(QStringLiteral(", "))));
    }
}

void MainWindow::updateDataState()
{
    if (m_dataEdit == nullptr || m_dataState == nullptr)
        return;
    if (m_extractRow != nullptr)
        m_extractRow->setVisible(false);

    const QString path = m_dataEdit->text().trimmed();
    if (path.isEmpty())
    {
        const QString found = AppPaths::findDataBesideGame(AppPaths::archiveRoot());
        if (found.isEmpty())
        {
            setBad(m_dataState, tr("✗ No game disc found beside the game. Select a disc image."));
            updateLanguageState(QString());
            return;
        }
        m_dataState->setText(
            tr("Using %1, found beside the game.").arg(QFileInfo(found).fileName()));
        m_dataState->setStyleSheet(QString());
        const QFileInfo fi(found);
        updateLanguageState(fi.isFile() ? DiscImage::inspect(found).gameId
                                        : DiscImage::extractedGameId(found));
        return;
    }

    const QFileInfo fi(path);
    if (fi.isFile())
    {
        const DiscImage::Info info = DiscImage::inspect(path);
        if (!info.ok)
        {
            setBad(m_dataState, QStringLiteral("✗ ") + info.error);
            updateLanguageState(QString());
            return;
        }
        const QString format =
            (info.format == QLatin1String("raw") ? QStringLiteral("ISO") : info.format)
            + (info.nkit ? QStringLiteral(", NKit") : QString());
        setGood(m_dataState, tr("✓ %1, from a disc image (%2).")
                                 .arg(releaseName(info.gameId), format));
        if (m_extractRow != nullptr)
            m_extractRow->setVisible(true);
        updateLanguageState(info.gameId);
        return;
    }

    const QDir dir(path);
    const bool ok = dir.exists() &&
                    (dir.exists(QStringLiteral("common.ini")) ||
                     dir.exists(QStringLiteral("COMMON.INI")));
    if (ok)
    {
        const QString gameId = DiscImage::extractedGameId(path);
        setGood(m_dataState, gameId.isEmpty()
                                 ? tr("✓ This is the right folder.")
                                 : tr("✓ %1, from an extracted disc.").arg(releaseName(gameId)));
        updateLanguageState(gameId);
    }
    else
    {
        updateLanguageState(QString());
        if (!dir.exists())
            setBad(m_dataState, tr("✗ File or folder not found."));
        else if (dir.exists(QStringLiteral("files/common.ini")))
            setBad(m_dataState, tr("✗ Folder is one level too high. Select the files folder "
                                   "inside it."));
        else
            setBad(m_dataState, tr("✗ No game files found. Select the folder that contains "
                                   "common.ini."));
    }
}

// Greyed out under the American disc, which has one language; Japanese needs the Japanese disc's own menus.
QString MainWindow::gamePath(const QString& path)
{
    const QString p = QDir::fromNativeSeparators(path.trimmed());
    return p.isEmpty() ? p : QDir::cleanPath(QDir(AppPaths::archiveRoot()).absoluteFilePath(p));
}

QString MainWindow::userFolder() const
{
    if (m_ini.has(QStringLiteral("USER_DIR")))
        return AppPaths::userFolder(gamePath(m_ini.value(QStringLiteral("USER_DIR"))));
    if (m_portableBox != nullptr && m_portableBox->isChecked())
        return AppPaths::portableUserFolder();
    return AppPaths::userFolder();
}

void MainWindow::updatePortableState()
{
    if (m_portableState == nullptr)
        return;
    m_portableState->setText(tr("Saves go to %1").arg(QDir::toNativeSeparators(userFolder())));
}

void MainWindow::openFolder(const QString& path)
{
    if (!QDir().mkpath(path))
    {
        QMessageBox::warning(this, tr("Texture packs"),
                             tr("Could not create %1.").arg(QDir::toNativeSeparators(path)));
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void MainWindow::updateTexturePacks()
{
    if (m_packList == nullptr)
        return;

    QStringList roots = { AppPaths::modsFolder(userFolder(), QStringLiteral("textures")),
                          AppPaths::modsFolder(AppPaths::archiveRoot(), QStringLiteral("textures")) };
    if (!m_texturesFolder.isEmpty())
        roots << gamePath(m_texturesFolder);

    const QLocale locale;
    const auto describe = [&](const QString& name, int textures) {
        return textures == 1 ? tr("%1: 1 texture").arg(name)
                             : tr("%1: %2 textures").arg(name, locale.toString(textures));
    };
    QStringList lines;
    QMap<QString, int> folders;  // pack folder -> textures, summed over the roots that have it
    for (const QString& root : roots)
    {
        for (const TexturePack& p : TexturePacks::scan(root))
        {
            lines << describe(p.name, p.textures);
            if (p.path != root)
                folders[p.name] += p.textures;
        }
    }
    m_packList->setText(lines.isEmpty()
                            ? tr("No packs found. Put each pack in its own folder inside the "
                                 "textures folder.")
                            : lines.join(QLatin1Char('\n')));

    const bool choose = folders.size() > 1 || !m_packWanted.isEmpty();
    m_packList->setVisible(!choose);
    m_packChoiceRow->setVisible(choose);
    if (choose)
    {
        m_fillingPacks = true;
        m_packChoice->clear();
        m_packChoice->addItem(tr("All packs"), QString());
        for (auto it = folders.cbegin(); it != folders.cend(); ++it)
            m_packChoice->addItem(describe(it.key(), it.value()), it.key());
        if (!m_packWanted.isEmpty() && !folders.contains(m_packWanted))
            m_packChoice->addItem(tr("%1 (not found)").arg(m_packWanted), m_packWanted);
        m_packChoice->setCurrentIndex(qMax(0, m_packChoice->findData(m_packWanted)));
        m_fillingPacks = false;
    }
    m_packList->setEnabled(m_texturesBox->isChecked());
    m_packChoiceRow->setEnabled(m_texturesBox->isChecked());
}

void MainWindow::updateLanguageState(const QString& gameId)
{
    if (m_language == nullptr || m_languageState == nullptr)
        return;

    const bool european = gameId.startsWith(QStringLiteral("G4QP"));
    const bool japanese = gameId.startsWith(QStringLiteral("G4QJ"));
    const bool known = !gameId.isEmpty();
    m_language->setEnabled(!known || european || japanese);

    if (auto* model = qobject_cast<QStandardItemModel*>(m_language->model()))
    {
        const int i = m_language->findData(QStringLiteral("japanese"));
        if (QStandardItem* item = i >= 0 ? model->item(i) : nullptr)
            item->setEnabled(!known || japanese);
    }

    if (!known)
        m_languageState->setText(tr("Applies to the European and Japanese releases only."));
    else if (european)
        m_languageState->setText(tr("Applies to this copy (European release)."));
    else if (japanese)
        m_languageState->setText(tr("Applies to this copy (Japanese release)."));
    else
        m_languageState->setText(
            tr("Does not apply. The %1 release has one language.").arg(tr("American")));
}

void MainWindow::setDirty(bool dirty)
{
    m_dirty = dirty;
    setWindowTitle(dirty ? tr("Super Mario Strikers Settings (unsaved changes)")
                         : tr("Super Mario Strikers Settings"));
    if (m_saveButton != nullptr)
        m_saveButton->setEnabled(true);
}

bool MainWindow::setValueByKey(const QString& key, const QString& value)
{
    const auto it = m_controlByKey.constFind(IniFile::normalise(key));
    if (it == m_controlByKey.constEnd())
        return false;
    m_controls[it.value()].set(value);
    return true;
}

QString MainWindow::valueByKey(const QString& key) const
{
    const auto it = m_controlByKey.constFind(IniFile::normalise(key));
    if (it == m_controlByKey.constEnd())
        return QString();
    return m_controls[it.value()].get();
}

bool MainWindow::showHelpFor(const QString& key)
{
    InfoButton* info = m_infoByKey.value(IniFile::normalise(key), nullptr);
    if (info == nullptr)
        return false;
    info->showPanel();
    return true;
}
