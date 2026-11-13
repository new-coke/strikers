package io.github.newcoke.strikers;

import android.content.Context;
import android.content.res.Configuration;

import java.util.Locale;

/** Applies the launcher locale independently of the game disc language. */
final class AppLocale {

    private AppLocale() {}

    // Keep these tags in sync with the translated Android resource directories.
    static final String[] LANGUAGE_TAGS = {
        "", "en", "pt-BR", "es", "fr", "de", "it", "ja", "zh-CN", "ru"
    };

    private static final String[] LANGUAGE_NAMES = {
        null,  // Filled with the translated system default.
        "English",
        "Português (Brasil)",
        "Español",
        "Français",
        "Deutsch",
        "Italiano",
        "日本語",
        "简体中文",
        "Русский",
    };

    static String[] displayNames(Context ctx) {
        String[] n = LANGUAGE_NAMES.clone();
        n[0] = ctx.getString(R.string.language_system);
        return n;
    }

    static String displayName(Context ctx, String tag) {
        for (int i = 0; i < LANGUAGE_TAGS.length; i++) {
            if (LANGUAGE_TAGS[i].equals(tag)) return displayNames(ctx)[i];
        }
        return tag.isEmpty() ? ctx.getString(R.string.language_system) : tag;
    }

    // Call from attachBaseContext before an activity inflates its views.
    static Context wrap(Context base) {
        final String tag = new GamePreferences(base).getString(GamePreferences.APP_LANGUAGE);
        if (tag.isEmpty()) return base;

        final Locale locale = Locale.forLanguageTag(tag);
        final Configuration c = new Configuration(base.getResources().getConfiguration());
        c.setLocale(locale);
        c.setLayoutDirection(locale);
        return base.createConfigurationContext(c);
    }
}
