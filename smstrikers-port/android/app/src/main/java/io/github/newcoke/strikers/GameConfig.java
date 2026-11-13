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

    static String readSetting(Context context, String key, String fallback) {
        AtomicFile file = new AtomicFile(configFile(context));
        try (BufferedReader reader = new BufferedReader(
                new InputStreamReader(file.openRead(), StandardCharsets.UTF_8))) {
            for (String line; (line = reader.readLine()) != null; ) {
                String trimmed = line.trim();
                if (trimmed.startsWith("#") || trimmed.startsWith(";")) continue;
                if (!key.equals(keyOnLine(trimmed))) continue;
                String value = trimmed.substring(trimmed.indexOf('=') + 1).trim();
                if (value.length() >= 2 && (value.startsWith("\"") && value.endsWith("\"")
                        || value.startsWith("'") && value.endsWith("'"))) {
                    value = value.substring(1, value.length() - 1);
                }
                return value;
            }
        } catch (java.io.FileNotFoundException ignored) {
            return fallback;
        } catch (IOException e) {
            Log.w(TAG, "Could not read game setting " + key, e);
        }
        return fallback;
    }

    static boolean writeSetting(Context context, String key, String value) {
        return rewrite(configFile(context), java.util.Collections.singletonMap(key, value));
    }

    private static File configFile(Context context) {
        return new File(StrikersHome.homeDir(context), "strikers.ini");
    }

    static boolean writePerformancePreset(Context context, String resolution, String fps) {
        Map<String, String> keys = new LinkedHashMap<>();
        keys.put("res_scale", resolution);
        keys.put("fps_limit", fps);
        keys.put("msaa", "1");
        return rewrite(configFile(context), keys);
    }

    static boolean writeTexturePack(Context context, String pack) {
        Map<String, String> keys = new LinkedHashMap<>();
        boolean off = TexturePacks.OFF.equals(pack);
        keys.put("textures", off ? "0" : null);
        keys.put("texture_pack", off || pack.isEmpty() ? null : "\"" + pack + "\"");
        return rewrite(configFile(context), keys);
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
            final String trimmed = original.trim();
            final String name = keyOnLine(original);
            if (name == null || !keys.containsKey(name)) continue;
            if (Boolean.TRUE.equals(written.get(name))) {
                if (!trimmed.startsWith("#") && !trimmed.startsWith(";")) {
                    lines.set(i, "#" + original);
                }
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
        String normalized = name.toLowerCase(java.util.Locale.ROOT);
        return normalized.startsWith("strikers_") ? normalized.substring(9) : normalized;
    }
}
