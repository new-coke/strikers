package io.github.newcoke.strikers;

import android.content.Context;
import android.net.Uri;
import android.util.Log;

import org.json.JSONObject;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.nio.file.Files;
import java.nio.file.LinkOption;
import java.nio.file.StandardOpenOption;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Locale;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

/** Manages libadrenotools ZIPs containing meta.json and a Vulkan driver library. */
final class Drivers {

    private static final String TAG = "strikers";
    private static final int MAX_ENTRIES = 256;
    private static final long MAX_ARCHIVE_BYTES = 256L * 1024 * 1024;

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
            if (directory.getName().startsWith(".") || !directory.isDirectory()) continue;
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
            if (soname.isEmpty() || !new File(soname).getName().equals(soname)
                    || !Files.isRegularFile(new File(directory, soname).toPath(),
                            LinkOption.NOFOLLOW_LINKS)) {
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
            while ((n = in.read(b)) != -1) {
                if (out.size() + n > 64 * 1024) throw new IOException("Driver metadata is too large");
                out.write(b, 0, n);
            }
            return out.toString("UTF-8");
        }
    }

    static Driver install(Context ctx, InputStream zip, String suggestedName) {
        try {
            return installArchive(installedDirectory(ctx), zip, suggestedName);
        } catch (IOException e) {
            Log.w(TAG, "Could not install driver", e);
            return null;
        }
    }

    static synchronized Driver installArchive(File root, InputStream input, String name)
            throws IOException {
        String id = slug(name);
        if (id.startsWith(".")) throw new IOException("Invalid driver name");
        File destination = new File(root, id);
        File work = Files.createTempDirectory(root.toPath(), ".install-").toFile();
        File staging = new File(work, "new");
        File previous = new File(work, "previous");
        boolean preserveBackup = false;
        try {
            Files.createDirectory(staging.toPath());
            extract(input, staging);
            Driver driver = readMetadata(staging);
            if (driver == null) throw new IOException("Missing or invalid driver metadata");

            if (destination.exists()) Files.move(destination.toPath(), previous.toPath());
            preserveBackup = previous.exists();
            try {
                Files.move(staging.toPath(), destination.toPath());
            } catch (IOException error) {
                if (preserveBackup) {
                    try {
                        Files.move(previous.toPath(), destination.toPath());
                        preserveBackup = false;
                    } catch (IOException restoreError) {
                        error.addSuppressed(restoreError);
                        Log.e(TAG, "Previous driver retained at " + previous, restoreError);
                    }
                }
                throw error;
            }
            preserveBackup = false;
            return new Driver(id, driver.name, driver.description, driver.soname, destination);
        } finally {
            if (!preserveBackup) deleteTree(work);
        }
    }

    private static void extract(InputStream input, File staging) throws IOException {
        byte[] buffer = new byte[64 * 1024];
        long bytes = 0;
        int entries = 0;
        try (ZipInputStream zip = new ZipInputStream(input)) {
            for (ZipEntry entry; (entry = zip.getNextEntry()) != null; ) {
                if (++entries > MAX_ENTRIES) throw new IOException("Too many driver ZIP entries");
                // Preserve support for ZIPs with a top-level folder.
                String name = new File(entry.getName()).getName();
                if (name.isEmpty() || name.equals(".") || name.equals("..")) {
                    throw new IOException("Invalid driver ZIP entry");
                }
                try (OutputStream out = entry.isDirectory() ? null : Files.newOutputStream(
                        new File(staging, name).toPath(), StandardOpenOption.CREATE_NEW,
                        StandardOpenOption.WRITE)) {
                    for (int count; (count = zip.read(buffer)) != -1; ) {
                        bytes += count;
                        if (bytes > MAX_ARCHIVE_BYTES) throw new IOException("Driver ZIP is too large");
                        if (out != null) out.write(buffer, 0, count);
                    }
                }
            }
        }
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

    static synchronized void remove(Driver d) {
        deleteTree(d.directory);
    }

    private static void deleteTree(File f) {
        if (f == null || !f.exists()) return;
        File[] children = Files.isSymbolicLink(f.toPath()) ? null : f.listFiles();
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
