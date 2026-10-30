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
        "Internal render resolution, as a multiple of the console's 448 rows. Unset, it follows the "
        "window; on a Steam Deck it is the panel's 800 rows (1.786) even when docked.")));

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

    v.push_back(choice("vsync", "display", Text::tr("Vertical sync"), "0",
        Text::tr(
        "Synchronises each frame with your display's refresh, so the image "
        "cannot tear into two mismatched halves as the camera pans."),
        QStringLiteral(
        "1 waits for each refresh of your display and gives the steadiest picture. 0 makes the "
        "controls more responsive and keeps the frame rate up when your graphics card struggles. "
        "Where your system supports it, 0 still avoids tearing."),
        { QStringLiteral("1"), QStringLiteral("0") },
        { Text::tr("On (steadiest)"),
          Text::tr("Relaxed (more responsive)") }));

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
        "Graphics backend: auto, d3d12, vulkan, metal, opengl, opengles."),
        { QString(), QStringLiteral("d3d12"), QStringLiteral("vulkan"),
          QStringLiteral("metal"), QStringLiteral("opengl"), QStringLiteral("opengles") },
        { Text::tr("Automatic (recommended)"), Text::tr("Direct3D 12"),
          Text::tr("Vulkan"), Text::tr("Metal"), Text::tr("OpenGL"), Text::tr("OpenGL ES") }));

    // Three states, because unset is fullscreen under Steam's Game Mode and 0 has to survive a save.
    v.push_back(choice("fullscreen", "display", Text::tr("Fullscreen"), "",
        Text::tr(
        "Whether the game starts fullscreen. F11 toggles it at any time, so "
        "this only sets the initial state."),
        QStringLiteral(
        "1 opens fullscreen and 0 a window. Unset, it is fullscreen under Steam's Game Mode (gamescope), "
        "which scales a window into the screen with black bars, and a window everywhere else."),
        { QString(), QStringLiteral("1"), QStringLiteral("0") },
        { Text::tr("Automatic (fullscreen in Steam's Game Mode)"), Text::tr("On"), Text::tr("Off") }));

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

    v.push_back(scalar("data", "paths", Text::tr("Game disc"), "",
        Text::tr(
        "Your own copy of the game: a disc image, or a folder extracted from "
        "one. Leave it empty to use whichever is beside the game."),
        QStringLiteral(
        "Where the game data is: either the `files` folder of an extracted disc, or a disc image.")));

    // Unset or a folder means on and 0 means off; the switch keeps a folder the file names.
    v.push_back(toggle("textures", "paths", Text::tr("Packs"), "",
        Text::tr("Use texture packs"),
        Text::tr("Replacement textures, including packs made for Dolphin."),
        QStringLiteral(
        "Texture packs load from mods/textures/ beside the game and mods/textures/ in the user folder. A folder named "
        "here loads too and wins over both; 0 turns texture packs off.")));

    v.push_back(scalar("texture_pack", "paths", Text::tr("Pack"), "",
        Text::tr("Loads one pack when the textures folders hold several."),
        QStringLiteral(
        "One pack out of the textures folders, by its folder name. Unset loads every pack.")));

    v.push_back(toggle("texture_dump", "paths", Text::tr("Pack making"), "0",
        Text::tr("Save the game's textures as it loads them"),
        Text::tr(
        "Saves each texture as a picture. An edited copy that keeps its file "
        "name and proportions replaces the original when put in a pack."),
        QStringLiteral(
        "Save each texture the game loads as a PNG named for a texture pack, in a folder per disc file. "
        "1 saves to texture_dumps/ in the user folder; a path saves there.")));

    // This is the *game's* language, OSGetLanguage(), the setting a European GameCube kept in SRAM;
    // and it has nothing to do with the language this window is written in.
    v.push_back(choice("language", "paths", Text::tr("Language"), "",
        Text::tr(
        "The language of menus and on-screen text. The American release is "
        "English only, and Japanese needs the Japanese release."),
        QStringLiteral(
        "Menu language: english, german, french, spanish or italian, and japanese on the Japanese disc. "
        "Unset keeps the disc's own language. The American disc ignores this."),
        { QString(), QStringLiteral("english"), QStringLiteral("german"),
          QStringLiteral("french"), QStringLiteral("spanish"), QStringLiteral("italian"),
          QStringLiteral("japanese") },
        // The languages the disc can be played in, named in the reader's language rather than each
        // in its own; this is a list to choose from.
        { Text::tr("Disc default"), Text::tr("English"), Text::tr("German"), Text::tr("French"),
          Text::tr("Spanish"), Text::tr("Italian"), Text::tr("Japanese") }));

    v.push_back(toggle("portable", "paths", Text::tr("Portable"), "0",
        Text::tr("Keep saves and caches in the game folder"),
        Text::tr(
        "Saves, controls and the shader cache stay in the game folder instead "
        "of your user profile, so the folder can be moved or copied whole."),
        QStringLiteral(
        "1 keeps the user folder and the shader cache in user/ and cache/ beside the game, so the "
        "game folder can be moved or copied whole. user_dir and cache_dir still apply when set.")));

    v.push_back(toggle("unlock_all", "game", Text::tr("Extras"), "0",
        Text::tr("Unlock every stadium, team and cup"),
        Text::tr(
        "Makes every stadium, team and cup mode available without earning "
        "them first."),
        QStringLiteral(
        "Every stadium, team and cup mode available.")));

    v.push_back(toggle("discord", "discord", Text::tr("Discord (Rich Presence)"), "1",
        Text::tr("Show what you're playing on Discord"),
        Text::tr(
        "Your Discord profile shows the mode, the captains and the score "
        "while you play. Discord has to be open on this computer."),
        QStringLiteral(
        "Your Discord status while Discord's desktop app runs: in the menus, or a match's mode, cup round, "
        "stadium, captains, score and time left. On by default; 0 turns it off. "
        "A room you host gets a Join button once the router opens its port; friends need the game open.")));

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
        "0 stops the keyboard acting as player 1's controller.")));

    v.push_back(choice("button_prompts", "input", Text::tr("Button prompts"), "auto",
        Text::tr(
        "Show the controls bound on the active device, or choose a controller style."),
        QStringLiteral(
        "Button art: auto follows the last input device. gamecube, xbox, playstation, nintendo, steamdeck,\n"
        "generic or keyboard shows that family's buttons instead."),
        { QStringLiteral("auto"), QStringLiteral("gamecube"), QStringLiteral("xbox"),
          QStringLiteral("playstation"), QStringLiteral("nintendo"), QStringLiteral("steamdeck"),
          QStringLiteral("generic"), QStringLiteral("keyboard") },
        { Text::tr("Automatic"), Text::tr("GameCube"), Text::tr("Xbox"), Text::tr("PlayStation"),
          Text::tr("Nintendo"), Text::tr("Steam Deck"), Text::tr("Generic"),
          Text::tr("Keyboard") }));

    v.push_back(toggle("pad_swap_sticks", "input", Text::tr("Sticks"), "0",
        Text::tr("Swap the two sticks"),
        Text::tr(
        "Move with the right stick and aim with the left."),
        QStringLiteral(
        "1 swaps the main stick and the C-stick.")));

    // What the game uses unset (Aurora's 8000 and 31150 of 32767), since a default is saved commented out.
    v.push_back(scalar("pad_deadzone", "input", Text::tr("Stick deadzone"), "0.24",
        Text::tr(
        "How far a stick must travel before input registers. Raise it if a "
        "worn stick drifts at rest."),
        QStringLiteral(
        "Stick deadzone, 0.0 to 0.9, as a fraction of the stick's travel.")));

    v.push_back(scalar("pad_trigger_threshold", "input", Text::tr("Trigger point"), "0.95",
        Text::tr(
        "How far a trigger must travel to register as a press."),
        QStringLiteral(
        "How far an analog trigger is pulled before L or R counts as pressed, as a fraction of its travel.")));

    v.push_back(toggle("pad_rumble", "input", Text::tr("Rumble"), "1",
        Text::tr("Vibrate the controller"),
        Text::tr(
        "Force feedback on tackles, saves and goals."),
        QStringLiteral(
        "0 turns rumble off on every controller.")));

    v.push_back(scalar("pad_rumble_strength", "input", Text::tr("Rumble strength"), "100",
        Text::tr(
        "Force feedback intensity, from a faint buzz to the full motor."),
        QStringLiteral(
        "Rumble strength, 0 to 100.")));

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
