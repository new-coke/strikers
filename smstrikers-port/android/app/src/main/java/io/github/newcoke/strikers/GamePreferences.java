package io.github.newcoke.strikers;

import android.content.Context;
import android.content.SharedPreferences;

/** Stores launcher settings and their defaults. GameConfig exports native settings to the INI. */
final class GamePreferences {

    // Keep the stored key names so upgrades retain existing settings.
    // Touch controls.
    static final String SHOW_DPAD = "direcional";
    static final String CSTICK = "cstick";
    static final String TOUCH_OPACITY = "opacidade";  // Percent.
    static final String TOUCH_SIZE = "tamanho";  // Percent.
    static final String TOUCH_DEADZONE = "zona_morta";  // Percent.
    static final String HUD = "hud";

    // Native game settings.
    static final String RESOLUTION_LINES = "res_linhas";
    static final String MSAA = "msaa";
    static final String ANISO = "aniso";
    static final String FPS_LIMIT = "fps_limit";  // -1 follows the display. Zero uncaps.
    static final String VSYNC = "vsync";
    static final String DISC_REGION = "regiao";  // Empty uses the disc default.
    static final String GAME_LANGUAGE = "idioma";  // Empty uses the disc default.
    static final String UNLOCK_ALL = "desbloquear";
    static final String STRETCH_MENUS = "esticar";  // Zero fits. One stretches.
    static final String ASPECT_RATIO = "proporcao";  // 0: display, 1: 16:9, 2: 4:3.

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
            case MSAA: return preferences.getInt(key, 1);
            case ANISO: return preferences.getInt(key, 16);
            case STRETCH_MENUS: return preferences.getInt(key, 1);
            case FPS_LIMIT: return preferences.getInt(key, -1);
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
