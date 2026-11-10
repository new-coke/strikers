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
        PreferenceGroup about = section(screen, R.string.sec_about);
        row(about, getText(R.string.game_folder), StrikersHome.homeDir(this).getAbsolutePath(), null);
        row(about, getText(R.string.setup_title), getText(R.string.about_note), null);
        setPreferenceScreen(screen);
    }

    private void display(PreferenceGroup group) {
        choice(group, R.string.res_internal, GamePreferences.RESOLUTION_LINES,
                new String[]{getString(R.string.res_follow), getString(R.string.res_original),
                        "720", "1080", "1440", "2160"}, new int[]{0, 448, 720, 1080, 1440, 2160});
        choice(group, R.string.fps_limit, GamePreferences.FPS_LIMIT,
                new String[]{getString(R.string.fps_follow_display),
                        getString(R.string.fps_unlimited), "30", "60", "120"},
                new int[]{-1, 0, 30, 60, 120});
        toggle(group, R.string.vsync, R.string.vsync_help, GamePreferences.VSYNC, true);
        choice(group, R.string.aspect, GamePreferences.ASPECT_RATIO,
                new String[]{getString(R.string.res_follow), "16:9", "4:3"}, new int[]{0, 1, 2});
        choice(group, R.string.widen, GamePreferences.STRETCH_MENUS,
                new String[]{getString(R.string.widen_fit), getString(R.string.widen_stretch)},
                new int[]{0, 1});
        choice(group, R.string.msaa, GamePreferences.MSAA,
                new String[]{getString(R.string.off), "4x"}, new int[]{1, 4});
        choice(group, R.string.aniso, GamePreferences.ANISO,
                new String[]{"1x", "2x", "4x", "8x", "16x"}, new int[]{1, 2, 4, 8, 16});
        row(group, R.string.video_driver, R.string.drivers_restart_note,
                () -> startActivity(new Intent(this, DriverSettingsActivity.class)));
        row(group, R.string.texture_packs, R.string.texture_packs_summary,
                () -> startActivity(new Intent(this, TexturePacksActivity.class)));
    }

    private void controls(PreferenceGroup group) {
        slider(group, R.string.opacity, GamePreferences.TOUCH_OPACITY, 25, 100);
        slider(group, R.string.size, GamePreferences.TOUCH_SIZE, 75, 130);
        slider(group, R.string.deadzone, GamePreferences.TOUCH_DEADZONE, 0, 35);
        toggle(group, R.string.dpad, 0, GamePreferences.SHOW_DPAD, false);
        toggle(group, R.string.right_stick, 0, GamePreferences.CSTICK, false);
        row(group, getText(R.string.sec_controls), getText(R.string.controls_note), null);
    }

    private void game(PreferenceGroup group) {
        toggle(group, R.string.unlock_all, R.string.unlock_all_help, GamePreferences.UNLOCK_ALL, true);
        toggle(group, R.string.hud, R.string.hud_help, GamePreferences.HUD, false);
        ListPreference language = list(group, R.string.language, AppLocale.displayNames(this), AppLocale.LANGUAGE_TAGS,
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
        toggle.setChecked(prefs.getBoolean(key));
        toggle.setOnPreferenceChangeListener((item, value) -> {
            prefs.setBoolean(key, (boolean) value);
            if (nativeSetting) saveConfig();
            return true;
        });
        group.addPreference(toggle);
    }

    private void choice(PreferenceGroup group, int title, String key, String[] names, int[] values) {
        String[] entries = new String[values.length];
        for (int i = 0; i < values.length; i++) entries[i] = Integer.toString(values[i]);
        ListPreference option = list(group, title, names, entries, Integer.toString(prefs.getInt(key)));
        option.setOnPreferenceChangeListener((item, value) -> {
            prefs.setInt(key, Integer.parseInt(value.toString()));
            saveConfig();
            return true;
        });
    }

    private ListPreference list(PreferenceGroup group, int title, CharSequence[] labels,
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

    private void saveConfig() {
        if (!GameConfig.writeSettings(this, prefs)) toast(R.string.ini_write_failed);
    }
}
