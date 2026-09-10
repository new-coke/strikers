#include "schema.h"

#include "inifile.h"

#include <QCoreApplication>
#include <QHash>

namespace {

// A translation context of the schema's own, so a translator opening the .ts file meets the
// settings as one list rather than finding them scattered through QObject's context alongside every
// stray string in the application.
class Text
{
    Q_DECLARE_TR_FUNCTIONS(Schema)
};

// Nine fields is too many to initialise positionally without eventually transposing two of them,
// and C++17 has no designated initialisers.

Setting toggle(const char* key, const char* section, const QString& label,
               const char* def, const QString& check, const QString& detail,
               const QString& help)
{
    Setting s;
    s.key = QString::fromLatin1(key);
    s.section = QString::fromLatin1(section);
    s.label = label;
    s.def = QString::fromLatin1(def);
    s.check = check;
    s.detail = detail;
    s.help = help;
    s.helpIsVerbatim = true;
    return s;
}

Setting choice(const char* key, const char* section, const QString& label,
               const char* def, const QString& detail, const QString& help,
               const QStringList& values, const QStringList& valueLabels)
{
    Setting s;
    s.key = QString::fromLatin1(key);
    s.section = QString::fromLatin1(section);
    s.label = label;
    s.def = QString::fromLatin1(def);
    s.detail = detail;
    s.help = help;
    s.values = values;
    s.valueLabels = valueLabels;
    s.helpIsVerbatim = true;
    return s;
}

// Anything the window draws itself: a spin box and a check box, a slider, a path field.
Setting scalar(const char* key, const char* section, const QString& label,
               const char* def, const QString& detail, const QString& help)
{
    Setting s;
    s.key = QString::fromLatin1(key);
    s.section = QString::fromLatin1(section);
    s.label = label;
    s.def = QString::fromLatin1(def);
    s.detail = detail;
    s.help = help;
    s.helpIsVerbatim = true;
    return s;
}

QVector<Setting> makeDisplay()
{
    QVector<Setting> v;

    v.push_back(scalar("res_scale", "display", Text::tr("Render resolution"), "",
        Text::tr(
        "The resolution the game renders at, independent of your window size. "
        "Higher is sharper and costs graphics performance."),
        QStringLiteral(
        "Internal render resolution, as a multiple of the console's 448 rows.")));

    // Only 1 and 4 are offered because only 1 and 4 exist: the value goes straight to the swap
    // chain's sample count, and WebGPU guarantees those two and nothing between them.
    v.push_back(choice("msaa", "display", Text::tr("Anti-aliasing"), "1",
        Text::tr(
        "Multisampling, which smooths the stepped edges on high-contrast "
        "lines like the goal net. Costs some graphics performance."),
        QStringLiteral(
        "Multisampling. Init-time only. 1 is the default and 4 is the only other value worth setting."),
        { QStringLiteral("1"), QStringLiteral("4") },
        { Text::tr("Off"), Text::tr("On (4x)") }));

    v.push_back(choice("aniso", "display", Text::tr("Texture filtering"), "16",
        Text::tr(
        "Anisotropic filtering, which keeps textures sharp at grazing angles, "
        "so the far half of the pitch keeps its grain. Nearly free."),
        QStringLiteral(
        "Anisotropic texture filtering: 1, 2, 4, 8 or 16."),
        { QStringLiteral("1"), QStringLiteral("2"), QStringLiteral("4"),
          QStringLiteral("8"), QStringLiteral("16") },
        { Text::tr("Off (as the console)"), Text::tr("2x"), Text::tr("4x"),
          Text::tr("8x"), Text::tr("16x") }));

    v.push_back(scalar("fps_limit", "display", Text::tr("Frame rate limit"), "",
        Text::tr(
        "Caps the frame rate. Lower it to reduce heat, fan noise and power "
        "draw on a laptop."),
        QStringLiteral(
        "Frame rate cap in Hz, or 0 for unlimited.")));

    v.push_back(choice("vsync", "display", Text::tr("Vertical sync"), "1",
        Text::tr(
        "Synchronises each frame with your display's refresh, so the image "
        "cannot tear into two mismatched halves as the camera pans."),
        QStringLiteral(
        "1 blocks on the vblank (Fifo). 0 selects Mailbox where the driver has it, which is still "
        "tear-free but does not drop to half rate when a frame lands late, worth ~10 fps on a GPU "
        "that is close to its limit."),
        { QStringLiteral("1"), QStringLiteral("0") },
        { Text::tr("On (steadiest)"),
          Text::tr("Relaxed (tear-free, without dropping to half rate)") }));

    v.push_back(scalar("aspect", "display", Text::tr("Aspect ratio"), "",
        Text::tr(
        "The aspect ratio the game renders for. Leave it following the window "
        "unless the image looks stretched."),
        QStringLiteral(
        "Aspect ratio: auto, 16:9, 4:3, 21:9, any W:H, or a decimal.")));

    v.push_back(choice("backend", "display", Text::tr("Graphics API"), "",
        Text::tr(
        "Which graphics API the game renders through. Automatic picks the "
        "right one for your system; change it only to work around a driver "
        "problem."),
        QStringLiteral(
        "Graphics backend: auto, d3d12, vulkan, metal."),
        { QString(), QStringLiteral("d3d12"), QStringLiteral("vulkan"),
          QStringLiteral("metal") },
        { Text::tr("Automatic (recommended)"), Text::tr("Direct3D 12"),
          Text::tr("Vulkan"), Text::tr("Metal") }));

    v.push_back(toggle("fullscreen", "display", Text::tr("Fullscreen"), "0",
        Text::tr("Start the game in fullscreen"),
        Text::tr(
        "Whether the game starts fullscreen. F11 toggles it at any time, so "
        "this only sets the initial state."),
        QStringLiteral(
        "Open fullscreen instead of in a 1920x1080 window.")));

    v.push_back(toggle("pause_on_focus_lost", "display", Text::tr("Pause"), "0",
        Text::tr("Pause when you click away from the game"),
        Text::tr(
        "Suspends the game when the window loses focus, rather than letting "
        "it run in the background."),
        QStringLiteral(
        "Stop the game when its window loses focus. 0 by default and deliberately: the attract "
        "mode and the demo match are worth leaving to run, and a benchmark that stopped would "
        "report nothing.")));

    return v;
}

QVector<Setting> makeAudio()
{
    QVector<Setting> v;

    v.push_back(toggle("audio", "audio", Text::tr("Sound"), "1",
        Text::tr("Play music and sound effects"),
        Text::tr(
        "Music and sound effects. On by default."),
        QStringLiteral(
        "Sound: music, effects, the stadium reverb and the movies' own tracks.")));

    v.push_back(toggle("log_audio", "audio", Text::tr("Diagnostics"), "0",
        Text::tr("Write a sound report when the game quits"),
        Text::tr(
        "Writes a diagnostic report on the mixer when the game exits, to "
        "attach to a bug report."),
        QStringLiteral(
        "Report at exit: ticks rendered, underruns, and whether any non-silent sample was "
        "produced; plus the mixer's state every two seconds.")));

    return v;
}

QVector<Setting> makeGame()
{
    QVector<Setting> v;

    v.push_back(scalar("data", "paths", Text::tr("Game files"), "",
        Text::tr(
        "The extracted contents of your own game disc. Leave it empty to look "
        "for a files folder beside the game."),
        QStringLiteral(
        "Where the extracted disc lives. Leave it unset and the game looks for a\n"
        "`files` folder beside itself, which is what an unzipped release archive is.\n"
        "If it finds nothing it says so on screen and names every path it tried.")));

    // This is the *game's* language, OSGetLanguage(), the setting a European GameCube kept in SRAM;
    // and it has nothing to do with the language this window is written in.
    v.push_back(choice("language", "paths", Text::tr("Language"), "",
        Text::tr(
        "Menu and commentary language. Only the European disc reads it; the "
        "others take it from the disc id."),
        QStringLiteral(
        "The console's own language setting, which only a European disc reads."),
        { QString(), QStringLiteral("german"), QStringLiteral("french"),
          QStringLiteral("spanish"), QStringLiteral("italian") },
        // The languages the disc can be played in, named in the reader's language rather than each
        // in its own; this is a list to choose from.
        { Text::tr("English"), Text::tr("German"), Text::tr("French"),
          Text::tr("Spanish"), Text::tr("Italian") }));

    v.push_back(toggle("unlock_all", "game", Text::tr("Extras"), "0",
        Text::tr("Unlock every stadium, team and cup"),
        Text::tr(
        "Makes every stadium, team and cup mode available without earning "
        "them first."),
        QStringLiteral(
        "Every stadium, team and cup mode available.")));

    // The default is off and the on value is `menu`, not `1`, because `1` is the compact heads-up
    // form and a player who turns "debug mode" on and sees a frame counter has not been given the
    // thing the switch names.
    v.push_back(toggle("overlay", "debug", Text::tr("Debug mode"), "0",
        Text::tr("Show the debug menu"),
        Text::tr(
        "The developer menu: frame timings, cheats, camera and render "
        "controls. F1 opens and closes it."),
        QStringLiteral(
        "The debug overlay and menu. `menu` opens the menu straight away, `1` draws only the compact heads-up form.")));

    return v;
}

QVector<Setting> makeInputSwitches()
{
    QVector<Setting> v;

    v.push_back(toggle("keyboard", "input", Text::tr("Keyboard"), "1",
        Text::tr("Play with the keyboard"),
        Text::tr(
        "Installs the keyboard as a controller. Turn it off to free the keys "
        "if you only play on a pad."),
        QStringLiteral(
        "0 disables the keyboard pad. The keys below are then not installed at all,\n"
        "for a machine that plays with a pad and wants them back.")));

    v.push_back(toggle("pad_swap_sticks", "input", Text::tr("Sticks"), "0",
        Text::tr("Swap the two sticks"),
        Text::tr(
        "Move with the right stick and aim with the left."),
        QStringLiteral(
        "1 swaps the left and right sticks, so the right stick moves the player and\n"
        "the left one aims. The GameCube's own layout is the other way round.")));

    v.push_back(scalar("pad_deadzone", "input", Text::tr("Stick deadzone"), "0.15",
        Text::tr(
        "How far a stick must travel before input registers. Raise it if a "
        "worn stick drifts at rest."),
        QStringLiteral(
        "How far a stick has to move before the game sees it, as a fraction of full\n"
        "travel. Raise it on a worn pad that drifts; lower it for finer control.")));

    v.push_back(scalar("pad_trigger_threshold", "input", Text::tr("Trigger point"), "0.5",
        Text::tr(
        "How far a trigger must travel to register as a press."),
        QStringLiteral(
        "How far a trigger has to be pulled to count as a press, as a fraction of\n"
        "full travel. The console's L and R had a physical click near the bottom;\n"
        "this is where that click goes.")));

    v.push_back(toggle("pad_rumble", "input", Text::tr("Rumble"), "1",
        Text::tr("Vibrate the controller"),
        Text::tr(
        "Force feedback on tackles, saves and goals."),
        QStringLiteral(
        "1 enables force feedback. The game has its own rumble option too, and the\n"
        "shipped `no_pad_rumble` config key turns it off independently of both.")));

    v.push_back(scalar("pad_rumble_strength", "input", Text::tr("Rumble strength"), "100",
        Text::tr(
        "Force feedback intensity, from a faint buzz to the full motor."),
        QStringLiteral(
        "Rumble strength as a percentage. 100 is what the console asked for.")));

    return v;
}

// The group names and direction labels below are identifiers, not display text: `group` is the hash
// key deciding the layout box. groupLabel() and bindingLabel() translate at the point of drawing.
const char* const kFace = "Face buttons";
const char* const kShoulder = "Shoulders and Start";
const char* const kDpad = "D-pad";
const char* const kStick = "Control stick";
const char* const kCstick = "C-stick";

QVector<Binding> makeKeyboardBindings()
{
    const QString face = QString::fromLatin1(kFace);
    const QString shoulder = QString::fromLatin1(kShoulder);
    const QString dpad = QString::fromLatin1(kDpad);
    const QString stick = QString::fromLatin1(kStick);
    const QString cstick = QString::fromLatin1(kCstick);

    return {
        { "key_a", "input", "A", "X", face },
        { "key_b", "input", "B", "Z", face },
        { "key_x", "input", "X", "C", face },
        { "key_y", "input", "Y", "V", face },

        { "key_z", "input", "Z", "Space", shoulder },
        { "key_start", "input", "Start", "Return", shoulder },
        { "key_l", "input", "L", "Q", shoulder },
        { "key_r", "input", "R", "E", shoulder },

        { "key_dpad_up", "input", "Up", "Up", dpad },
        { "key_dpad_down", "input", "Down", "Down", dpad },
        { "key_dpad_left", "input", "Left", "Left", dpad },
        { "key_dpad_right", "input", "Right", "Right", dpad },

        { "key_stick_up", "input", "Up", "W", stick },
        { "key_stick_down", "input", "Down", "S", stick },
        { "key_stick_left", "input", "Left", "A", stick },
        { "key_stick_right", "input", "Right", "D", stick },

        { "key_cstick_up", "input", "Up", "I", cstick },
        { "key_cstick_down", "input", "Down", "K", cstick },
        { "key_cstick_left", "input", "Left", "J", cstick },
        { "key_cstick_right", "input", "Right", "L", cstick },
    };
}

QVector<Binding> makeGamepadBindings()
{
    const QString face = QString::fromLatin1(kFace);
    const QString shoulder = QString::fromLatin1(kShoulder);
    const QString dpad = QString::fromLatin1(kDpad);

    return {
        { "pad_a", "input", "A", "a", face },
        { "pad_b", "input", "B", "b", face },
        { "pad_x", "input", "X", "x", face },
        { "pad_y", "input", "Y", "y", face },

        { "pad_z", "input", "Z", "rightshoulder", shoulder },
        { "pad_start", "input", "Start", "start", shoulder },
        { "pad_l", "input", "L", "lefttrigger", shoulder },
        { "pad_r", "input", "R", "righttrigger", shoulder },

        { "pad_dpad_up", "input", "Up", "dpup", dpad },
        { "pad_dpad_down", "input", "Down", "dpdown", dpad },
        { "pad_dpad_left", "input", "Left", "dpleft", dpad },
        { "pad_dpad_right", "input", "Right", "dpright", dpad },
    };
}

struct Registry
{
    QVector<Setting> display = makeDisplay();
    QVector<Setting> audio = makeAudio();
    QVector<Setting> game = makeGame();
    QVector<Setting> inputSwitches = makeInputSwitches();
    QVector<Binding> keyboard = makeKeyboardBindings();
    QVector<Binding> gamepad = makeGamepadBindings();
    QHash<QString, Setting> byKey;

    Registry()
    {
        for (const QVector<Setting>* v : { &display, &audio, &game, &inputSwitches })
        {
            for (const Setting& s : *v)
                byKey.insert(IniFile::normalise(s.key), s);
        }
        for (const QVector<Binding>* v : { &keyboard, &gamepad })
        {
            for (const Binding& b : *v)
            {
                Setting s;
                s.key = b.key;
                s.section = b.section;
                s.label = b.label;
                s.def = b.def;
                byKey.insert(IniFile::normalise(b.key), s);
            }
        }
    }
};

// Not const, and that is the whole of the translation story here: the tables are built on first
// use, so the wording in them is whatever tr() answered at that moment, and Schema::retranslate()
// below throws them away and asks again.
Registry& registry()
{
    static Registry r;
    return r;
}

} // namespace

void Schema::retranslate()
{
    registry() = Registry();
}

const Setting& Schema::get(const QString& key)
{
    static const Setting none;
    const auto it = registry().byKey.constFind(IniFile::normalise(key));
    return it == registry().byKey.constEnd() ? none : it.value();
}

const QVector<Setting>& Schema::display() { return registry().display; }
const QVector<Setting>& Schema::audio() { return registry().audio; }
const QVector<Setting>& Schema::game() { return registry().game; }
const QVector<Setting>& Schema::inputSwitches() { return registry().inputSwitches; }
const QVector<Binding>& Schema::keyboardBindings() { return registry().keyboard; }
const QVector<Binding>& Schema::gamepadBindings() { return registry().gamepad; }

QVector<Setting> Schema::all()
{
    QVector<Setting> v;
    for (const QVector<Setting>* g :
         { &registry().display, &registry().audio, &registry().game,
           &registry().inputSwitches })
        v += *g;
    return v;
}

bool Schema::isOwned(const QString& key)
{
    return registry().byKey.contains(IniFile::normalise(key));
}

QString Schema::groupLabel(const QString& group)
{
    if (group == QLatin1String(kFace))
        return Text::tr("Face buttons");
    if (group == QLatin1String(kShoulder))
        return Text::tr("Shoulders and Start");
    if (group == QLatin1String(kDpad))
        return Text::tr("D-pad");
    if (group == QLatin1String(kStick))
        return Text::tr("Control stick");
    if (group == QLatin1String(kCstick))
        return Text::tr("C-stick");
    return group;
}

QString Schema::bindingLabel(const QString& label)
{
    if (label == QLatin1String("Up"))
        return Text::tr("Up", "d-pad or stick direction");
    if (label == QLatin1String("Down"))
        return Text::tr("Down", "d-pad or stick direction");
    if (label == QLatin1String("Left"))
        return Text::tr("Left", "d-pad or stick direction");
    if (label == QLatin1String("Right"))
        return Text::tr("Right", "d-pad or stick direction");
    // A, B, X, Y, Z, L, R and Start are printed on the pad itself.
    return label;
}

const QStringList& Schema::padButtonNames()
{
    static const QStringList names = {
        "a", "b", "x", "y",
        "back", "guide", "start",
        "leftstick", "rightstick",
        "leftshoulder", "rightshoulder",
        "dpup", "dpdown", "dpleft", "dpright",
        "lefttrigger", "righttrigger",
    };
    return names;
}
