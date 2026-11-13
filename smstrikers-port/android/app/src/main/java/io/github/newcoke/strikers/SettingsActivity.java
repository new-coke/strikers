package io.github.newcoke.strikers;

import android.app.AlertDialog;
import android.content.Intent;
import android.os.Bundle;
import android.preference.ListPreference;
import android.preference.Preference;
import android.preference.PreferenceGroup;
import android.preference.PreferenceScreen;
import android.preference.SwitchPreference;
import android.widget.SeekBar;

@SuppressWarnings("deprecation")
public class SettingsActivity extends NativeSettingsActivity {
    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        PreferenceScreen screen = page();
        display(section(screen, R.string.sec_video));
        controls(section(screen, R.string.sec_controls));
        game(section(screen, R.string.sec_game));
        controller(section(screen, R.string.controller_bindings));
        PreferenceGroup about = section(screen, R.string.sec_about);
        row(about, getText(R.string.game_folder), StrikersHome.homeDir(this).getAbsolutePath(), null);
        row(about, getText(R.string.setup_title), getText(R.string.about_note), null);
        setPreferenceScreen(screen);
    }

    private void display(PreferenceGroup group) {
        row(group, R.string.performance_presets, R.string.performance_presets_help,
                this::choosePerformancePreset);
        choice(group, R.string.res_internal, "res_scale",
                new String[]{getString(R.string.res_follow), getString(R.string.res_original),
                        "720", "1080", "1440", "2160"}, new String[]{"", "1.000", "1.607", "2.411", "3.214", "4.821"}, "");
        choice(group, R.string.fps_limit, "fps_limit",
                new String[]{getString(R.string.fps_follow_display),
                        getString(R.string.fps_unlimited), "30", "60", "120"},
                new String[]{"", "0", "30", "60", "120"}, "");
        toggle(group, R.string.vsync, R.string.vsync_help, "vsync", true);
        choice(group, R.string.aspect, "aspect",
                new String[]{getString(R.string.res_follow), "16:9", "4:3"}, new String[]{"", "16:9", "4:3"}, "");
        choice(group, R.string.widen, "widen",
                new String[]{getString(R.string.widen_fit), getString(R.string.widen_stretch)},
                new String[]{"fit", "stretch"}, "stretch");
        choice(group, R.string.msaa, "msaa",
                new String[]{getString(R.string.off), "4x"}, new String[]{"1", "4"}, "1");
        choice(group, R.string.aniso, "aniso",
                new String[]{"1x", "2x", "4x", "8x", "16x"}, new String[]{"1", "2", "4", "8", "16"}, "16");
        row(group, R.string.video_driver, R.string.drivers_restart_note,
                () -> startActivity(new Intent(this, DriverSettingsActivity.class)));
        row(group, R.string.texture_packs, R.string.texture_packs_summary,
                () -> startActivity(new Intent(this, TexturePacksActivity.class)));
    }

    private void choosePerformancePreset() {
        int[] labels = {R.string.performance_battery, R.string.performance_balanced,
                R.string.performance_high_refresh};
        String[] names = new String[labels.length];
        for (int i = 0; i < labels.length; i++) names[i] = getString(labels[i]);
        new AlertDialog.Builder(this).setTitle(R.string.performance_presets)
                .setItems(names, (dialog, preset) -> {
                    String resolution = preset == 0 ? "1.000" : "1.607";
                    String fps = preset == 0 ? "30" : preset == 1 ? "60" : null;
                    if (GameConfig.writePerformancePreset(this, resolution, fps)) recreate();
                    else toast(R.string.ini_write_failed);
                })
                .setNegativeButton(android.R.string.cancel, null).show();
    }

    private void controls(PreferenceGroup group) {
        String layout = prefs.getString(GamePreferences.TOUCH_LAYOUT);
        ListPreference layoutOption = list(group, getText(R.string.touch_layout),
                new String[]{getString(R.string.touch_standard), getString(R.string.touch_compact),
                        getString(R.string.touch_tablet)},
                new String[]{GamePreferences.TOUCH_STANDARD, GamePreferences.TOUCH_COMPACT,
                        GamePreferences.TOUCH_TABLET},
                layout.isEmpty() ? GamePreferences.TOUCH_STANDARD : layout);
        layoutOption.setOnPreferenceChangeListener((item, value) -> {
            prefs.setString(GamePreferences.TOUCH_LAYOUT, value.toString());
            return true;
        });
        row(group, R.string.touch_edit_positions, R.string.touch_edit_positions_help,
                () -> startActivity(new Intent(this, TouchLayoutActivity.class)));
        slider(group, R.string.opacity, GamePreferences.TOUCH_OPACITY, 25, 100);
        slider(group, R.string.size, GamePreferences.TOUCH_SIZE, 75, 130);
        slider(group, R.string.deadzone, GamePreferences.TOUCH_DEADZONE, 0, 35);
        toggle(group, R.string.dpad, 0, GamePreferences.SHOW_DPAD, false);
        toggle(group, R.string.right_stick, 0, GamePreferences.CSTICK, false);
        row(group, getText(R.string.sec_controls), getText(R.string.controls_note), null);
    }

    private void controller(PreferenceGroup group) {
        row(group, getText(R.string.controller_bindings),
                getText(R.string.controller_bindings_help), null);
        choice(group, R.string.controller_swap_sticks, "pad_swap_sticks",
                new String[]{getString(R.string.controller_default),
                        getString(R.string.controller_swapped)}, new String[]{"", "1"}, "");
        choice(group, R.string.controller_deadzone, "pad_deadzone",
                new String[]{getString(R.string.controller_default), "0%", "10%", "15%",
                        "20%", "24%", "30%", "40%"},
                new String[]{"", "0", "0.10", "0.15", "0.20", "0.24", "0.30", "0.40"}, "");
        choice(group, R.string.controller_rumble, "pad_rumble",
                new String[]{getString(R.string.controller_default), getString(R.string.off)},
                new String[]{"", "0"}, "");
        choice(group, R.string.controller_rumble_strength, "pad_rumble_strength",
                new String[]{getString(R.string.controller_default), "25%", "50%", "75%", "100%"},
                new String[]{"", "25", "50", "75", "100"}, "");
        String[] buttons = {"", "a", "b", "x", "y", "leftshoulder", "rightshoulder",
                "back", "start", "leftstick", "rightstick", "dpup", "dpdown", "dpleft", "dpright"};
        String[] labels = {getString(R.string.controller_default), "A ↓", "B →", "X ←", "Y ↑",
                "LB / L1", "RB / R1", "Back / Select", "Start", "L3", "R3", "D-pad ↑",
                "D-pad ↓", "D-pad ←", "D-pad →"};
        String[] keys = {"a", "b", "x", "y", "z", "start", "l", "r",
                "dpad_up", "dpad_down", "dpad_left", "dpad_right"};
        String[] targets = {"A", "B", "X", "Y", "Z", "Start", "L", "R",
                "D-pad ↑", "D-pad ↓", "D-pad ←", "D-pad →"};
        String[] triggerButtons = java.util.Arrays.copyOf(buttons, buttons.length + 2);
        String[] triggerLabels = java.util.Arrays.copyOf(labels, labels.length + 2);
        triggerButtons[buttons.length] = "lefttrigger";
        triggerButtons[buttons.length + 1] = "righttrigger";
        triggerLabels[labels.length] = "LT / L2";
        triggerLabels[labels.length + 1] = "RT / R2";
        for (int i = 0; i < keys.length; i++) {
            boolean trigger = keys[i].equals("l") || keys[i].equals("r");
            choice(group, targets[i], "pad_" + keys[i], trigger ? triggerLabels : labels,
                    trigger ? triggerButtons : buttons, "");
        }
    }

    private void game(PreferenceGroup group) {
        toggle(group, R.string.unlock_all, R.string.unlock_all_help, "unlock_all", true);
        toggle(group, R.string.hud, R.string.hud_help, GamePreferences.HUD, false);
        ListPreference language = list(group, getText(R.string.language), AppLocale.displayNames(this), AppLocale.LANGUAGE_TAGS,
                prefs.getString(GamePreferences.APP_LANGUAGE));
        language.setOnPreferenceChangeListener((item, value) -> {
            if (!value.equals(prefs.getString(GamePreferences.APP_LANGUAGE))) {
                prefs.setString(GamePreferences.APP_LANGUAGE, value.toString());
                recreate();
            }
            return true;
        });
    }

    private void toggle(PreferenceGroup group, int title, int summary, String key, boolean nativeSetting) {
        SwitchPreference toggle = new SwitchPreference(this);
        toggle.setPersistent(false);
        toggle.setTitle(title);
        if (summary != 0) toggle.setSummary(summary);
        String current = nativeSetting ? GameConfig.readSetting(this, key, null) : null;
        toggle.setChecked(nativeSetting
                ? current != null && (key.equals("unlock_all") || enabled(current))
                : prefs.getBoolean(key));
        toggle.setOnPreferenceChangeListener((item, value) -> {
            if (nativeSetting) {
                return saveSetting(key, (boolean) value ? "1" : key.equals("unlock_all") ? null : "0");
            }
            prefs.setBoolean(key, (boolean) value);
            return true;
        });
        group.addPreference(toggle);
    }

    private static boolean enabled(String value) {
        try {
            return Long.parseUnsignedLong(value) != 0;
        } catch (NumberFormatException ignored) {
            return false;
        }
    }

    private void choice(PreferenceGroup group, int title, String key, String[] names,
                        String[] values, String fallback) {
        choice(group, getText(title), key, names, values, fallback);
    }

    private void choice(PreferenceGroup group, CharSequence title, String key, String[] names,
                        String[] values, String fallback) {
        String selected = GameConfig.readSetting(this, key, fallback);
        ListPreference option = list(group, title, names, values, selected);
        if (option.findIndexOfValue(selected) < 0) option.setSummary(selected.replace("%", "%%"));
        option.setOnPreferenceChangeListener((item, value) -> {
            if (!saveSetting(key, value.toString().isEmpty() ? null : value.toString())) return false;
            option.setSummary("%s");
            return true;
        });
    }

    private ListPreference list(PreferenceGroup group, CharSequence title, CharSequence[] labels,
                                CharSequence[] values, String selected) {
        ListPreference option = new ListPreference(this);
        option.setPersistent(false);
        option.setTitle(title);
        option.setDialogTitle(title);
        option.setEntries(labels);
        option.setEntryValues(values);
        option.setValue(selected);
        option.setSummary("%s");
        group.addPreference(option);
        return option;
    }

    private void slider(PreferenceGroup group, int title, String key, int min, int max) {
        Preference row = new Preference(this);
        row.setTitle(title);
        row.setSummary(percent(prefs.getInt(key)));
        row.setOnPreferenceClickListener(item -> {
            SeekBar seek = new SeekBar(this);
            seek.setMin(min);
            seek.setMax(max);
            seek.setProgress(prefs.getInt(key));
            seek.setContentDescription(getText(title));
            AlertDialog dialog = new AlertDialog.Builder(this)
                    .setTitle(getString(R.string.setting_value, getText(title), percent(seek.getProgress())))
                    .setView(seek)
                    .setNegativeButton(android.R.string.cancel, null)
                    .setPositiveButton(android.R.string.ok, (window, which) -> {
                        prefs.setInt(key, seek.getProgress());
                        row.setSummary(percent(seek.getProgress()));
                    }).create();
            seek.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
                public void onProgressChanged(SeekBar bar, int value, boolean fromUser) {
                    dialog.setTitle(getString(R.string.setting_value, getText(title), percent(value)));
                }
                public void onStartTrackingTouch(SeekBar bar) {}
                public void onStopTrackingTouch(SeekBar bar) {}
            });
            dialog.show();
            return true;
        });
        group.addPreference(row);
    }

    private String percent(int value) {
        return java.text.NumberFormat.getPercentInstance().format(value / 100.0);
    }

    private boolean saveSetting(String key, String value) {
        if (GameConfig.writeSetting(this, key, value)) return true;
        toast(R.string.ini_write_failed);
        return false;
    }
}
