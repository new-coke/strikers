package io.github.newcoke.strikers;

import android.content.Context;
import android.util.AtomicFile;
import android.util.Log;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStreamReader;
import java.io.IOException;
import java.io.OutputStreamWriter;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/** Writes app settings to the native INI while preserving unrelated keys and comments. */
final class GameConfig {

    private static final String TAG = "strikers";

    private GameConfig() {}

    static boolean writeSettings(Context ctx, GamePreferences prefs) {
        Map<String, String> keys = new LinkedHashMap<>();

        // The engine scales from 448 lines. Zero uses its display-dependent default.
        final int lines = prefs.getInt(GamePreferences.RESOLUTION_LINES);
        keys.put("res_scale", lines <= 0 ? null : String.format(
                java.util.Locale.ROOT, "%.3f", lines / 448f));
        keys.put("msaa", Integer.toString(prefs.getInt(GamePreferences.MSAA)));
        keys.put("aniso", Integer.toString(prefs.getInt(GamePreferences.ANISO)));
        final int fps = prefs.getInt(GamePreferences.FPS_LIMIT);
        keys.put("fps_limit", fps < 0 ? null : Integer.toString(fps));
        keys.put("vsync", prefs.getBoolean(GamePreferences.VSYNC) ? "1" : "0");
        keys.put("widen",
                prefs.getInt(GamePreferences.STRETCH_MENUS) != 0 ? "stretch" : "fit");
        final int aspect = prefs.getInt(GamePreferences.ASPECT_RATIO);
        keys.put("aspect", aspect == 1 ? "16:9" : aspect == 2 ? "4:3" : null);

        keys.put("unlock_all", prefs.getBoolean(GamePreferences.UNLOCK_ALL) ? "1" : null);
        keys.put("region", emptyToNull(prefs.getString(GamePreferences.DISC_REGION)));
        keys.put("language", emptyToNull(prefs.getString(GamePreferences.GAME_LANGUAGE)));

        // The Android HUD owns the performance display.
        keys.put("overlay", null);

        keys.put("backend", "vulkan");

        return rewrite(new File(StrikersHome.homeDir(ctx), "strikers.ini"), keys);
    }

    static boolean writeTexturePack(Context context, String pack) {
        Map<String, String> keys = new LinkedHashMap<>();
        boolean off = TexturePacks.OFF.equals(pack);
        keys.put("textures", off ? "0" : null);
        keys.put("texture_pack", off || pack.isEmpty() ? null : "\"" + pack + "\"");
        return rewrite(new File(StrikersHome.homeDir(context), "strikers.ini"), keys);
    }

    private static String emptyToNull(String s) {
        return s == null || s.isEmpty() ? null : s;
    }

    // Keep commented keys in place so users can find the available settings.
    private static synchronized boolean rewrite(File ini, Map<String, String> keys) {
        AtomicFile file = new AtomicFile(ini);
        List<String> lines = new ArrayList<>();
        if (ini.exists()) {
            try (BufferedReader r = new BufferedReader(
                    new InputStreamReader(file.openRead(), StandardCharsets.UTF_8))) {
                String line;
                while ((line = r.readLine()) != null) lines.add(line);
            } catch (IOException e) {
                Log.w(TAG, "Could not read " + ini, e);
                return false;
            }
        }

        Map<String, Boolean> written = new LinkedHashMap<>();
        for (String k : keys.keySet()) written.put(k, false);

        for (int i = 0; i < lines.size(); i++) {
            final String original = lines.get(i);
            final String name = keyOnLine(original);
            if (name == null || !keys.containsKey(name)) continue;
            if (Boolean.TRUE.equals(written.get(name))) {
                // The engine uses the last assignment, so disable duplicate keys.
                lines.set(i, "#" + original);
                continue;
            }
            final String value = keys.get(name);
            lines.set(i, value == null ? "#" + name + " =" : name + " = " + value);
            written.put(name, true);
        }

        List<String> added = new ArrayList<>();
        for (Map.Entry<String, String> e : keys.entrySet()) {
            if (Boolean.TRUE.equals(written.get(e.getKey()))) continue;
            added.add(e.getValue() == null
                    ? "#" + e.getKey() + " ="
                    : e.getKey() + " = " + e.getValue());
        }
        if (!added.isEmpty()) {
            lines.add("");
            lines.add("# Written by Android settings.");
            lines.addAll(added);
        }

        FileOutputStream stream = null;
        try {
            stream = file.startWrite();
            OutputStreamWriter writer = new OutputStreamWriter(stream, StandardCharsets.UTF_8);
            for (String line : lines) {
                writer.write(line);
                writer.write("\n");
            }
            writer.flush();
            file.finishWrite(stream);
            return true;
        } catch (IOException e) {
            if (stream != null) file.failWrite(stream);
            Log.w(TAG, "Could not write " + ini, e);
            return false;
        }
    }

    // Recognize commented assignments so settings stay beside their documentation.
    private static String keyOnLine(String line) {
        String s = line.trim();
        while (s.startsWith("#") || s.startsWith(";")) s = s.substring(1).trim();
        final int equalsIndex = s.indexOf('=');
        if (equalsIndex <= 0) return null;
        final String name = s.substring(0, equalsIndex).trim();
        if (name.isEmpty()) return null;
        for (int i = 0; i < name.length(); i++) {
            final char c = name.charAt(i);
            if (!Character.isLetterOrDigit(c) && c != '_') return null;
        }
        return name.toLowerCase(java.util.Locale.ROOT);
    }
}
