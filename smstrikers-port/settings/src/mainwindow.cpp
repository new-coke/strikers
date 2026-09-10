#include "mainwindow.h"

#include "apppaths.h"
#include "helptext.h"
#include "infobutton.h"
#include "keycapturebutton.h"
#include "keynames.h"
#include "settingspage.h"

#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPair>
#include <QPalette>
#include <QProcess>
#include <QPushButton>
#include <QResizeEvent>
#include <QScreen>
#include <QScrollArea>
#include <QSettings>
#include <QSlider>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QVariant>

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

bool truthy(const QString& v)
{
    const QString s = v.trimmed().toLower();
    return s == QLatin1String("1") || s == QLatin1String("true") ||
           s == QLatin1String("yes") || s == QLatin1String("on");
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

bool backendSupportedHere(const QString& backend)
{
    if (backend.isEmpty())
        return true; // automatic
#if defined(Q_OS_WIN)
    return backend == QLatin1String("d3d12") || backend == QLatin1String("vulkan");
#elif defined(Q_OS_MACOS)
    return backend == QLatin1String("metal");
#else
    return backend == QLatin1String("vulkan");
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
    h->addWidget(second);
    return row;
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

    // Hidden unless the game put this window here (setFirstRun).
    {
        const QPalette pal = palette();
        QColor ground = pal.color(QPalette::Base);
        ground.setAlpha(255);
        const QColor ink = pal.color(QPalette::Text);

        m_firstRunBanner = new QLabel;
        m_firstRunBanner->setObjectName(QStringLiteral("firstRunBanner"));
        m_firstRunBanner->setWordWrap(true);
        // Plain text, because the string is translated and a translator is not an author of markup.
        m_firstRunBanner->setTextFormat(Qt::PlainText);
        m_firstRunBanner->setStyleSheet(
            QStringLiteral("#firstRunBanner { background: rgb(%1,%2,%3);"
                           " color: rgb(%4,%5,%6);"
                           " border-bottom: 1px solid rgba(%4,%5,%6,60);"
                           " padding: 12px 20px; }")
                .arg(ground.red())
                .arg(ground.green())
                .arg(ground.blue())
                .arg(ink.red())
                .arg(ink.green())
                .arg(ink.blue()));
        m_firstRunBanner->hide();
        outer->addWidget(m_firstRunBanner);
    }

    m_tabs = new SettingsTabs;
    m_tabs->addTab(buildDisplayTab(), tr("Display"));
    m_tabs->addTab(buildAudioTab(), tr("Audio"));
    m_tabs->addTab(buildInputTab(), tr("Controls"));
    m_tabs->addTab(buildGameTab(), tr("Game"));
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

void MainWindow::closeEvent(QCloseEvent* event)
{
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
                combo->addItem(MainWindow::tr("%1 (from the file)").arg(v), v);
                i = combo->count() - 1;
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

    // res_scale: the default is computed, so "match my display" is a checkbox rather than a value
    // in the spin box's range.
    {
        const Setting& s = byKey(group, "res_scale");
        auto* autoBox = new QCheckBox(tr("Match my display"));
        auto* spin = new QDoubleSpinBox;
        spin->setRange(1.0, 6.0);
        spin->setSingleStep(0.05);
        spin->setDecimals(2);
        spin->setValue(2.0);
        spin->setSuffix(tr("×"));
        connect(autoBox, &QCheckBox::toggled, spin, &QWidget::setDisabled);
        connect(autoBox, &QCheckBox::toggled, this, &MainWindow::markDirty);
        connect(spin, &QDoubleSpinBox::valueChanged, this, &MainWindow::markDirty);
        page->addSetting(s.label, pair(autoBox, spin), infoFor(s));
        registerControl(
            s,
            [autoBox, spin] {
                return autoBox->isChecked() ? QString() : trimNumber(spin->value());
            },
            [autoBox, spin](const QString& v) {
                bool ok = false;
                const double d = v.toDouble(&ok);
                autoBox->setChecked(!ok || v.trimmed().isEmpty());
                spin->setDisabled(autoBox->isChecked());
                if (ok)
                    spin->setValue(d);
            });
    }

    addChoice(page, byKey(group, "msaa"));
    addChoice(page, byKey(group, "aniso"));

    page->beginSection(tr("Frame pacing"));

    // fps_limit: three states in one control, follow the display, unlimited, or an exact number. 0
    // is the game's own spelling of unlimited, so it is a value here and not a third checkbox.
    {
        const Setting& s = byKey(group, "fps_limit");
        auto* monitorBox = new QCheckBox(tr("Follow my display"));
        auto* spin = new QDoubleSpinBox;
        spin->setRange(0.0, 1000.0);
        spin->setDecimals(2);
        spin->setSingleStep(1.0);
        spin->setValue(60.0);
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
        for (const char* a : { "16:9", "4:3", "21:9", "32:9" })
            combo->addItem(QString::fromLatin1(a), QString::fromLatin1(a));
        combo->addItem(tr("Custom…"), QStringLiteral("\001custom"));
        auto* custom = new QLineEdit;
        custom->setPlaceholderText(tr("width : height"));
        custom->setEnabled(false);
        custom->setMaximumWidth(140);

        auto isCustom = [combo] {
            return combo->currentData().toString() == QLatin1String("\x01custom");
        };
        connect(combo, &QComboBox::currentIndexChanged, this, [this, custom, isCustom] {
            custom->setEnabled(isCustom());
            markDirty();
        });
        connect(custom, &QLineEdit::textChanged, this, &MainWindow::markDirty);
        page->addSetting(s.label, pair(combo, custom), infoFor(s));
        registerControl(
            s,
            [combo, custom, isCustom] {
                return isCustom() ? custom->text().trimmed() : combo->currentData().toString();
            },
            [combo, custom](const QString& v) {
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
                continue;
            combo->setItemData(i, QVariant(0), Qt::UserRole - 1); // disable the item
            combo->setItemText(i, tr("%1 (not on this computer)").arg(combo->itemText(i)));
        }
    }

    addSwitch(page, byKey(group, "fullscreen"));
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
    page->addFieldNote(SettingsPage::note(tr(
        "Music, effects and commentary volumes are the game's own settings, and "
        "live in its options screen rather than here.")));

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
        s.detail = tr("Click a binding and press the key you want. Escape cancels.");
        s.help = tr(
            "Bindings are stored as SDL scancode names, which are physical key\n"
            "positions rather than letters. On a US or UK layout what you press is\n"
            "what is stored. On AZERTY or Dvorak the position and the letter differ,\n"
            "and it is the letter that gets written, Qt cannot see the position.\n"
            "\n"
            "Keys with no name, the Command and Menu keys, media keys, and anything\n"
            "your layout only produces with a modifier, are refused rather than\n"
            "guessed at.");
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
        s.detail = tr("Choose which button on your controller each GameCube "
                      "button should come from.");
        s.help = tr(
            "The names are SDL's, and they are positions on SDL's idea of a\n"
            "controller rather than whatever your own controller prints on itself:\n"
            "`a` is the bottom face button on every pad, wherever its label sits.\n"
            "\n"
            "There is nothing here to press because Qt has no controller API. Run the\n"
            "game with the pad probe switched on to see the bindings that are actually\n"
            "in force, which is also the answer to \"what are my controls\".");
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
                    combo->addItem(tr("%1 (from the file)").arg(val), val);
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
        readout->setMinimumWidth(48);

        const bool percent = scale == 1;
        auto text = [percent, scale, suffix](int raw) {
            return percent ? QStringLiteral("%1%2").arg(raw).arg(suffix)
                           : trimNumber(double(raw) / scale) + suffix;
        };
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
    addSlider(Schema::get(QStringLiteral("pad_trigger_threshold")), 0.1, 0.9, 100, QString());
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
        m_dataEdit->setMinimumWidth(260);
        auto* browse = new QPushButton(tr("Choose…"));
        m_dataState = SettingsPage::note(QString());

        connect(browse, &QPushButton::clicked, this, [this] {
            const QString dir = QFileDialog::getExistingDirectory(
                this, tr("Choose the folder holding the game's files"), m_dataEdit->text());
            if (!dir.isEmpty())
                m_dataEdit->setText(dir);
        });
        connect(m_dataEdit, &QLineEdit::textChanged, this, [this] {
            updateDataState();
            markDirty();
        });

        page->addSetting(tr("Folder"), pair(m_dataEdit, browse, 1), infoFor(s));
        page->addFieldNote(m_dataState);
        registerControl(
            s, [this] { return m_dataEdit->text().trimmed(); },
            [this](const QString& val) { m_dataEdit->setText(val); });
    }

    page->beginSection(tr("Options"));

    // The combo's first entry writes nothing: the console's own default is English, and a file with
    // no `language` line gets exactly that.
    m_language = addChoice(page, Schema::get(QStringLiteral("language")));
    m_languageState = SettingsPage::note(QString());
    page->addFieldNote(m_languageState);

    addSwitch(page, Schema::get(QStringLiteral("unlock_all")));

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
        page->addFieldNote(SettingsPage::note(tr(
            "The menu opens as soon as the game starts. F1 opens and closes it "
            "while you play.")));
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
    bv->addWidget(SettingsPage::note(tr(
        "Everything else the file says, so editing it here loses nothing. A row that "
        "is switched off in the file starts empty; give it a value and saving turns "
        "it on, clear the value and saving comments it out again.")));

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
        showPath(tr("%1 (new, from %2 when you save)")
                     .arg(QDir::toNativeSeparators(path), QFileInfo(seeded).fileName()));
    else
        showPath(tr("%1 (new, created when you save)")
                     .arg(QDir::toNativeSeparators(path)));
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
    collectFromUi();
    return m_ini.save(path, error);
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
        tr("Put every setting in this window back to the way it came?\n\n"
           "Anything under Advanced is left alone: those are not this window's to "
           "reset. Nothing is written until you press Save."));
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
    if (m_firstRun)
    {
        // Nothing to look for: the game is the process that started this window and is sitting in
        // PortConfigLoad waiting for it to close.
        m_playButton->setEnabled(true);
        m_playButton->setToolTip(
            tr("Save these settings and let the game carry on starting."));
        return;
    }

    QString reason;
    const QString exe = AppPaths::findGame(AppPaths::archiveRoot(), &reason);
    m_playButton->setEnabled(!exe.isEmpty());
    m_playButton->setToolTip(exe.isEmpty() ? reason
                                           : tr("Save and start %1.")
                                                 .arg(QDir::toNativeSeparators(exe)));
}

void MainWindow::onPlay()
{
    // On a first run the game is already running, it is what started this window, and it resumes
    // the moment this process exits.
    QString exe;
    if (!m_firstRun)
    {
        QString reason;
        exe = AppPaths::findGame(AppPaths::archiveRoot(), &reason);
        if (exe.isEmpty())
        {
            QMessageBox::warning(this, tr("Play"), reason);
            return;
        }
    }

    QString error;
    if (!saveTo(m_path, &error))
    {
        QMessageBox::critical(this, tr("Play"),
                              tr("Could not save the settings first: %1").arg(error));
        return;
    }
    setDirty(false);

    if (!m_firstRun && !QProcess::startDetached(exe, {}, QFileInfo(exe).absolutePath()))
    {
        QMessageBox::critical(this, tr("Play"),
                              tr("Could not start %1.").arg(QDir::toNativeSeparators(exe)));
        return;
    }
    close();
}

void MainWindow::setFirstRun(bool firstRun)
{
    m_firstRun = firstRun;

    m_firstRunBanner->setText(
        tr("Super Mario Strikers is starting for the first time and is waiting "
           "on this window. Nothing here has to be changed, every setting "
           "already has a sensible default. Save and Play when you are done, or "
           "just close this window; you can open it again at any time."));
    m_firstRunBanner->setVisible(firstRun);

    // "Play" is a promise this window cannot keep on a first run: the game is already playing, or
    // will be a moment after this closes.
    m_playButton->setText(firstRun ? tr("Save and Play") : tr("Play"));
    m_playButton->setDefault(firstRun);
    m_saveButton->setDefault(!firstRun);
    updatePlayButton();

    // The banner is a widget the window was not measured with.
    if (firstRun)
    {
        adjustSize();
        resize(qMax(width(), 620), height());
        fitToCurrentTab();
    }
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
                : tr("%1 is used by more than one control. The game takes the first "
                     "one it reads, so the others will not work.")
                      .arg(clashes.join(QStringLiteral(", "))));
    }
}

void MainWindow::updateDataState()
{
    if (m_dataEdit == nullptr || m_dataState == nullptr)
        return;

    const QString path = m_dataEdit->text().trimmed();
    if (path.isEmpty())
    {
        m_dataState->setText(tr("The game will look for a \"files\" folder beside itself."));
        m_dataState->setStyleSheet(QString());
        updateLanguageState(QString());
        return;
    }

    const QDir dir(path);
    const bool ok = dir.exists() &&
                    (dir.exists(QStringLiteral("common.ini")) ||
                     dir.exists(QStringLiteral("COMMON.INI")));
    if (ok)
    {
        m_dataState->setText(tr("✓ This is the right folder."));
        m_dataState->setStyleSheet(QStringLiteral("color: #3f9142;"));
        updateLanguageState(discGameId(dir));
    }
    else
    {
        updateLanguageState(QString());
        m_dataState->setText(dir.exists()
                                 ? tr("✗ No game files here. Look for the folder with "
                                      "common.ini in it.")
                                 : tr("✗ There is no such folder."));
        m_dataState->setStyleSheet(QStringLiteral("color: #d64545;"));
    }
}

// The six-character game id from the extracted disc's sys/boot.bin, which the extractor writes
// beside the files folder and the game itself reads to pick its region.
QString MainWindow::discGameId(const QDir& files)
{
    QFile boot(files.filePath(QStringLiteral("../sys/boot.bin")));
    if (!boot.open(QIODevice::ReadOnly))
        return QString();
    const QByteArray id = boot.read(6);
    if (id.size() != 6 || !id.startsWith("G4Q"))
        return QString();
    return QString::fromLatin1(id);
}

// Only Mario Smash Football (G4QP01) asks the console for a language, so the combo is greyed out
// under any other disc rather than left looking like it works.
void MainWindow::updateLanguageState(const QString& gameId)
{
    if (m_language == nullptr || m_languageState == nullptr)
        return;

    const bool european = gameId.startsWith(QStringLiteral("G4QP"));
    const bool known = !gameId.isEmpty();
    m_language->setEnabled(!known || european);
    if (!known)
        m_languageState->setText(tr("Read by the European release only."));
    else if (european)
        m_languageState->setText(tr("This copy is the European release, so this applies."));
    else
        m_languageState->setText(
            tr("This copy is the %1 release, which has one language of its own.")
                .arg(gameId.startsWith(QStringLiteral("G4QJ")) ? tr("Japanese") : tr("American")));
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
