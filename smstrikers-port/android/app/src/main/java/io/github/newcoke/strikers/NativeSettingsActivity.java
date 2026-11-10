package io.github.newcoke.strikers;

import android.content.Context;
import android.os.Bundle;
import android.preference.Preference;
import android.preference.PreferenceActivity;
import android.preference.PreferenceCategory;
import android.preference.PreferenceGroup;
import android.preference.PreferenceScreen;
import android.view.KeyEvent;
import android.widget.Toast;

@SuppressWarnings("deprecation")
abstract class NativeSettingsActivity extends PreferenceActivity {
    GamePreferences prefs;

    @Override
    protected void attachBaseContext(Context base) {
        super.attachBaseContext(AppLocale.wrap(base));
    }

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        prefs = new GamePreferences(this);
        StrikersHome.writeDefaultConfig(StrikersHome.homeDir(this));
        getListView().setFitsSystemWindows(true);
        if (getActionBar() != null) getActionBar().setDisplayHomeAsUpEnabled(true);
    }

    final PreferenceScreen page() {
        PreferenceScreen screen = getPreferenceManager().createPreferenceScreen(this);
        screen.setOrderingAsAdded(true);
        return screen;
    }

    final PreferenceCategory section(PreferenceGroup page, int title) {
        PreferenceCategory category = new PreferenceCategory(this);
        category.setTitle(title);
        page.addPreference(category);
        return category;
    }

    final void row(PreferenceGroup page, int title, int summary, Runnable action) {
        row(page, getText(title), summary == 0 ? null : getText(summary), action);
    }

    final void row(PreferenceGroup page, CharSequence title, CharSequence summary, Runnable action) {
        Preference preference = new Preference(this);
        preference.setTitle(title);
        preference.setSummary(summary);
        preference.setSelectable(action != null);
        if (action != null) {
            preference.setOnPreferenceClickListener(item -> {
                action.run();
                return true;
            });
        }
        page.addPreference(preference);
    }

    final void toast(int message) {
        Toast.makeText(this, message, Toast.LENGTH_LONG).show();
    }

    @Override
    public boolean onOptionsItemSelected(android.view.MenuItem item) {
        if (item.getItemId() == android.R.id.home) {
            finish();
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (event.getKeyCode() == KeyEvent.KEYCODE_BUTTON_B) {
            if (event.getAction() == KeyEvent.ACTION_UP && !event.isCanceled()) finish();
            return true;
        }
        if (event.getKeyCode() == KeyEvent.KEYCODE_BUTTON_A) {
            event = new KeyEvent(event.getDownTime(), event.getEventTime(), event.getAction(),
                    KeyEvent.KEYCODE_DPAD_CENTER, event.getRepeatCount(), event.getMetaState(),
                    event.getDeviceId(), event.getScanCode(), event.getFlags(), event.getSource());
        }
        return super.dispatchKeyEvent(event);
    }
}
