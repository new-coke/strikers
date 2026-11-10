package io.github.newcoke.strikers;

import android.content.Context;
import android.net.Uri;
import android.util.Log;

import org.json.JSONObject;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Locale;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

/** Manages libadrenotools ZIPs containing meta.json and a Vulkan driver library. */
final class Drivers {

    private static final String TAG = "strikers";

    static final class Driver {
        final String id;  // Installed folder name.
        final String name;
        final String description;
        final String soname;
        final File directory;

        Driver(String id, String name, String description, String soname, File directory) {
            this.id = id;
            this.name = name;
            this.description = description;
            this.soname = soname;
            this.directory = directory;
        }
    }

    static final class Download {
        final String name;
        final String description;
        final String url;
        final String installId;

        Download(String name, String description, String url, String installId) {
            this.name = name;
            this.description = description;
            this.url = url;
            this.installId = installId;
        }
    }

    static List<Download> downloads(Context ctx) {
        List<Download> f = new ArrayList<>();
        f.add(new Download("Turnip 26.0 — Adreno 8xx",
                ctx.getString(R.string.drv_a8xx_desc),
                "https://github.com/K11MCH1/AdrenoToolsDrivers/releases/download/v26.0.0-rc08/"
              + "turnip_a8xx.zip", "turnip-26.0-adreno-8xx"));
        // Keep the previous install ID so existing driver selections still resolve.
        f.add(new Download("Turnip 26.0 — General",
                ctx.getString(R.string.drv_general_desc),
                "https://github.com/K11MCH1/AdrenoToolsDrivers/releases/download/v26.0.0-rc08/"
              + "Turnip_v26.0.0_R8.zip", "turnip-26.0-geral"));
        f.add(new Download("Qualcomm 840",
                ctx.getString(R.string.drv_qualcomm_desc),
                "https://github.com/K11MCH1/AdrenoToolsDrivers/releases/download/v840/"
              + "Qualcomm_840_adpkg.zip", "qualcomm-840"));
        return f;
    }

    private Drivers() {}

    // Android loads custom libraries from app-private storage.
    static File installedDirectory(Context ctx) {
        File d = new File(ctx.getFilesDir(), "drivers");
        //noinspection ResultOfMethodCallIgnored
        d.mkdirs();
        return d;
    }

    static List<Driver> list(Context ctx) {
        List<Driver> drivers = new ArrayList<>();
        File[] directories = installedDirectory(ctx).listFiles();
        if (directories == null) return drivers;
        for (File directory : directories) {
            if (!directory.isDirectory()) continue;
            Driver d = readMetadata(directory);
            if (d != null) drivers.add(d);
        }
        Collections.sort(drivers, (a, b) -> a.name.compareToIgnoreCase(b.name));
        return drivers;
    }

    static Driver findById(Context ctx, String id) {
        if (id == null || id.isEmpty()) return null;
        for (Driver d : list(ctx)) {
            if (d.id.equals(id)) return d;
        }
        return null;
    }

    private static Driver readMetadata(File directory) {
        File meta = new File(directory, "meta.json");
        if (!meta.isFile()) return null;
        try {
            JSONObject j = new JSONObject(readText(meta));
            String soname = j.optString("libraryName", "");
            if (soname.isEmpty() || !new File(directory, soname).isFile()) {
                Log.w(TAG, "Driver in " + directory + " is missing the library named in meta.json");
                return null;
            }
            return new Driver(directory.getName(),
                    j.optString("name", directory.getName()),
                    j.optString("description", ""),
                    soname, directory);
        } catch (Exception e) {
            Log.w(TAG, "Could not read meta.json in " + directory, e);
            return null;
        }
    }

    private static String readText(File f) throws IOException {
        try (InputStream in = new java.io.FileInputStream(f)) {
            java.io.ByteArrayOutputStream out = new java.io.ByteArrayOutputStream();
            byte[] b = new byte[8192];
            int n;
            while ((n = in.read(b)) > 0) out.write(b, 0, n);
            return out.toString("UTF-8");
        }
    }

    // Stage and validate a complete package before replacing the installed copy.
    static Driver install(Context ctx, InputStream zip, String suggestedName) {
        File destination = new File(installedDirectory(ctx), slug(suggestedName));
        File staging = new File(installedDirectory(ctx), ".tmp-" + System.currentTimeMillis());
        //noinspection ResultOfMethodCallIgnored
        staging.mkdirs();
        try (ZipInputStream z = new ZipInputStream(zip)) {
            ZipEntry e;
            while ((e = z.getNextEntry()) != null) {
                if (e.isDirectory()) continue;
                // Flatten entry paths so extraction stays inside the staging directory.
                String name = new File(e.getName()).getName();
                if (name.isEmpty()) continue;
                try (OutputStream out = new FileOutputStream(new File(staging, name))) {
                    byte[] b = new byte[64 * 1024];
                    int n;
                    while ((n = z.read(b)) > 0) out.write(b, 0, n);
                }
            }
        } catch (IOException e) {
            Log.w(TAG, "Could not extract driver", e);
            deleteTree(staging);
            return null;
        }

        Driver staged = readMetadata(staging);
        if (staged == null) {
            deleteTree(staging);
            return null;
        }
        deleteTree(destination);
        if (!staging.renameTo(destination)) {
            deleteTree(staging);
            return null;
        }
        return readMetadata(destination);
    }

    // Performs network and disk I/O. Call from a worker thread.
    static Driver download(Context ctx, Download source) {
        HttpURLConnection c = null;
        try {
            c = (HttpURLConnection) new URL(source.url).openConnection();
            c.setInstanceFollowRedirects(true);
            c.setConnectTimeout(20000);
            c.setReadTimeout(60000);
            if (c.getResponseCode() / 100 != 2) {
                Log.w(TAG, "Download from " + source.url + " returned HTTP " + c.getResponseCode());
                return null;
            }
            try (InputStream in = c.getInputStream()) {
                return install(ctx, in, source.installId);
            }
        } catch (Exception e) {
            Log.w(TAG, "Could not download " + source.url, e);
            return null;
        } finally {
            if (c != null) c.disconnect();
        }
    }

    static Driver importZip(Context ctx, Uri uri) {
        try (InputStream in = ctx.getContentResolver().openInputStream(uri)) {
            if (in == null) return null;
            String name = uri.getLastPathSegment();
            return install(ctx, in, name != null ? name : "driver");
        } catch (Exception e) {
            Log.w(TAG, "Could not import " + uri, e);
            return null;
        }
    }

    static void remove(Context ctx, Driver d) {
        deleteTree(d.directory);
    }

    private static void deleteTree(File f) {
        if (f == null || !f.exists()) return;
        File[] children = f.listFiles();
        if (children != null) {
            for (File c : children) deleteTree(c);
        }
        //noinspection ResultOfMethodCallIgnored
        f.delete();
    }

    private static String slug(String name) {
        String s = name == null ? "driver" : name;
        s = s.toLowerCase(Locale.ROOT).replaceAll("\\.zip$", "");
        s = s.replaceAll("[^a-z0-9._-]+", "-").replaceAll("^-+|-+$", "");
        return s.isEmpty() ? "driver" : s;
    }
}
