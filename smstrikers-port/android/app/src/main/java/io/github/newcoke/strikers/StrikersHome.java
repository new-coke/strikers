package io.github.newcoke.strikers;

import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.AssetManager;
import android.os.Build;
import android.system.ErrnoException;
import android.system.Os;
import android.util.Log;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.Locale;

/** Stages the paths and configuration the native game reads at startup. */
final class StrikersHome {
    private static final String TAG = "strikers";
    private static final String PREFS = "strikers-home";
    private static final String KEY_STAGED_VERSION = "staged-assets-version";

    private StrikersHome() {}

    static File homeDir(Context context) {
        File directory = context.getExternalFilesDir(null);
        return directory != null ? directory : context.getFilesDir();
    }

    /**
     * Must be called before the native library is loaded: the game reads its configuration out of
     * the environment from a library constructor, which runs inside System.loadLibrary.
     */
    static void prepare(Context context) {
        File home = homeDir(context);
        setenv("STRIKERS_HOME", home.getAbsolutePath());

        stageAssets(context, home);
        writeDefaultConfig(home);

        // The game's stderr is a file descriptor nobody reads on Android. Point the port's own log
        // at a file in the home directory instead, where `adb pull` can reach it.
        setenv("STRIKERS_LOG", new File(home, "strikers-log.txt").getAbsolutePath());

        prepararDriver(context);
    }

    /**
     * O driver de vídeo que o jogador escolheu, para o src/platform/vulkan_shim.c.
     *
     * <p>Três caminhos, e nenhum deles é adivinhável do lado nativo:
     *
     * <ul>
     *   <li>o diretório de bibliotecas do aplicativo, que é onde o libadrenotools
     *       procura os ganchos com que foi compilado. Tem de ser exatamente esse,
     *       e não uma cópia;
     *   <li>a pasta do driver, que tem de estar no armazenamento PRIVADO — o
     *       {@code dlopen} recusa uma biblioteca de um lugar que outro aplicativo
     *       poderia ter alterado;
     *   <li>o SONAME do .so, que varia de pacote para pacote e vem do meta.json.
     * </ul>
     *
     * <p>Sem escolha, nenhuma das variáveis é definida e o shim cai no driver do
     * sistema — que é o comportamento certo, e não uma falha.
     */
    private static void prepararDriver(Context context) {
        setenv("STRIKERS_VK_HOOK_DIR", context.getApplicationInfo().nativeLibraryDir);

        Drivers.Driver d = Drivers.porId(context, new Preferencias(context)
                .texto(Preferencias.DRIVER));
        if (d == null) {
            Log.i(TAG, "driver de vídeo: o do sistema");
            return;
        }
        setenv("STRIKERS_VK_DRIVER_DIR", d.pasta.getAbsolutePath() + "/");
        setenv("STRIKERS_VK_DRIVER", d.soname);
    }

    /** App-specific disc files remain usable for sideloaded and existing setups. */
    static File findLocalGameData(Context context) {
        File home = homeDir(context);
        File extracted = new File(home, "files");
        return new File(extracted, "common.ini").isFile() ? extracted : firstDiscImage(home);
    }

    private static void setenv(String name, String value) {
        try {
            Os.setenv(name, value, true);
            Log.i(TAG, name + "=" + value);
        } catch (ErrnoException e) {
            Log.e(TAG, "setenv " + name + " failed", e);
        }
    }

    /**
     * The button prompt artwork, out of the APK and into the home directory, because the game opens
     * it by path and an asset inside an APK has none. Re-staged when the app's version changes, so
     * an upgrade that adds artwork is not left with the old set.
     */
    private static void stageAssets(Context context, File home) {
        SharedPreferences prefs = context.getSharedPreferences(PREFS, Context.MODE_PRIVATE);
        long installed = versionCode(context);
        if (prefs.getLong(KEY_STAGED_VERSION, -1) == installed
                && new File(home, "input-prompts/LICENSE-Kenney.txt").exists()) {
            return;
        }

        try {
            copyAssetTree(context.getAssets(), "input-prompts", new File(home, "input-prompts"));
            prefs.edit().putLong(KEY_STAGED_VERSION, installed).apply();
        } catch (IOException e) {
            // Not fatal: the game says so on stderr and keeps the GameCube prompts.
            Log.w(TAG, "could not stage input-prompts", e);
        }
    }

    private static long versionCode(Context context) {
        try {
            android.content.pm.PackageInfo info =
                    context.getPackageManager().getPackageInfo(context.getPackageName(), 0);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                return info.getLongVersionCode();
            }
            return info.versionCode;
        } catch (Exception e) {
            return -1;
        }
    }

    private static void copyAssetTree(AssetManager assets, String path, File target)
            throws IOException {
        String[] children = assets.list(path);
        if (children == null || children.length == 0) {
            copyAssetFile(assets, path, target);
            return;
        }
        if (!target.isDirectory() && !target.mkdirs()) {
            throw new IOException("could not create " + target);
        }
        for (String child : children) {
            copyAssetTree(assets, path + "/" + child, new File(target, child));
        }
    }

    private static void copyAssetFile(AssetManager assets, String path, File target)
            throws IOException {
        File parent = target.getParentFile();
        if (parent != null && !parent.isDirectory() && !parent.mkdirs()) {
            throw new IOException("could not create " + parent);
        }
        byte[] buffer = new byte[64 * 1024];
        try (InputStream in = assets.open(path);
             OutputStream out = new FileOutputStream(target)) {
            int read;
            while ((read = in.read(buffer)) > 0) {
                out.write(buffer, 0, read);
            }
        }
    }

    /**
     * A strikers.ini in the home directory if there is not one already, so the player has a file to
     * edit and can see the keys that matter on a handheld with the defaults this port picked.
     */
    static void writeDefaultConfig(File home) {
        File ini = new File(home, "strikers.ini");
        if (ini.exists()) {
            return;
        }
        String contents =
                "# Super Mario Strikers, native port: Android settings.\n"
              + "#\n"
              + "# Every key here is also an environment variable with a STRIKERS_ prefix, and the\n"
              + "# desktop port's strikers.ini.example documents the full set. This file is written\n"
              + "# once, on first run, and never overwritten: it is yours to edit.\n"
              + "\n"
              + "# Where the game data is. Left unset, the game looks for a `files` folder or a\n"
              + "# disc image (.iso, .gcm, .ciso, .gcz) in this same directory:\n"
              + "#   " + home.getAbsolutePath() + "\n"
              + "#data =\n"
              + "\n"
              + "# Two keys are set here for Android. Other settings use the port defaults.\n"
              + "\n"
              + "# Vulkan is the only backend Dawn has on Android. `auto` would land here anyway;\n"
              + "# it is written out so the answer is in the file rather than in someone's head.\n"
              + "backend = vulkan\n"
              + "\n"
              + "# By default, the frame rate follows the display. Set a limit to save battery.\n"
              + "#fps_limit = 60\n"
              + "\n"
              + "# Fifo blocks and lets the GPU idle between frames. On a battery that is the point.\n"
              + "vsync = 1\n"
              + "\n"
              + "# Left at their defaults, and here only so they are easy to find. Internal render\n"
              + "# resolution follows the window, which on a phone is the panel itself; multisampling\n"
              + "# is off; anisotropic filtering is 16x. On a device that runs warm, res_scale is the\n"
              + "# one worth turning down first.\n"
              + "#res_scale = 1.5\n"
              + "#msaa = 4\n"
              + "#aniso = 4\n"
              + "\n"
              + "# The on-screen controls, which exist only on Android. They hide themselves while a\n"
              + "# real controller is connected; 0 here turns them off for good.\n"
              + "#touch = 0\n";
        try (OutputStream out = new FileOutputStream(ini)) {
            out.write(contents.getBytes("UTF-8"));
        } catch (IOException e) {
            Log.w(TAG, "could not write " + ini, e);
        }
    }

    private static File firstDiscImage(File dir) {
        File[] entries = dir.listFiles();
        if (entries == null) {
            return null;
        }
        File best = null;
        for (File entry : entries) {
            if (!entry.isFile()) {
                continue;
            }
            String name = entry.getName().toLowerCase(Locale.ROOT);
            if (!name.endsWith(".iso") && !name.endsWith(".gcm")
                    && !name.endsWith(".ciso") && !name.endsWith(".gcz")) {
                continue;
            }
            // Lowest name wins, the same rule src/platform/dvd.c uses, so the two agree on which of
            // two images in one folder is "the" one.
            if (best == null || entry.getName().compareToIgnoreCase(best.getName()) < 0) {
                best = entry;
            }
        }
        return best;
    }
}
