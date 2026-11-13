package io.github.newcoke.strikers;

import android.content.Context;
import android.content.SharedPreferences;

/** Stores Android app settings and their defaults. */
final class GamePreferences {

    // Keep the stored key names so upgrades retain existing settings.
    // Touch controls.
    static final String SHOW_DPAD = "direcional";
    static final String CSTICK = "cstick";
    static final String TOUCH_OPACITY = "opacidade";  // Percent.
    static final String TOUCH_SIZE = "tamanho";  // Percent.
    static final String TOUCH_DEADZONE = "zona_morta";  // Percent.
    static final String HUD = "hud";

    // App locale follows the system when its BCP 47 tag is empty.
    static final String APP_LANGUAGE = "idioma_app";

    // Installed driver folder name. An empty value selects the system driver.
    static final String DRIVER = "driver";
    static final String TEXTURE_PACK = "texture_pack";

    private final SharedPreferences preferences;

    GamePreferences(Context ctx) {
        preferences = ctx.getSharedPreferences("strikers", Context.MODE_PRIVATE);
    }

    boolean getBoolean(String key) {
        return preferences.getBoolean(key, false);
    }

    int getInt(String key) {
        switch (key) {
            case TOUCH_OPACITY: return preferences.getInt(key, 100);
            case TOUCH_SIZE: return preferences.getInt(key, 100);
            case TOUCH_DEADZONE: return preferences.getInt(key, 8);
            default: return preferences.getInt(key, 0);
        }
    }

    void setBoolean(String key, boolean value) {
        preferences.edit().putBoolean(key, value).apply();
    }

    void setInt(String key, int value) {
        preferences.edit().putInt(key, value).apply();
    }

    String getString(String key) {
        return preferences.getString(key, "");
    }

    void setString(String key, String value) {
        preferences.edit().putString(key, value).apply();
    }
}
