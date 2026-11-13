package io.github.newcoke.strikers;

import android.app.AlertDialog;
import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.preference.PreferenceGroup;
import android.preference.PreferenceScreen;
import android.util.Log;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;

@SuppressWarnings("deprecation")
public class TexturePacksActivity extends NativeSettingsActivity {
    private static final int PICK_PACK = 1;
    private boolean importing;

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        refresh();
    }

    private File folder() {
        return new File(StrikersHome.homeDir(this), "mods/textures");
    }

    private void refresh() {
        PreferenceScreen screen = page();
        PreferenceGroup packs = section(screen, R.string.texture_packs);
        String selected = prefs.getString(GamePreferences.TEXTURE_PACK);
        pack(packs, getString(R.string.off), TexturePacks.OFF, selected, false);
        pack(packs, getString(R.string.texture_packs_all), "", selected, false);
        for (String name : TexturePacks.list(folder())) {
            pack(packs, name, name, selected, true);
        }
        row(screen, R.string.texture_import, importing ? R.string.installing : R.string.texture_import_hint,
                importing ? null : this::pickZip);
        row(screen, getText(R.string.game_folder), folder().getAbsolutePath(), null);
        row(screen, R.string.texture_packs, R.string.drivers_restart_note, null);
        setPreferenceScreen(screen);
    }

    private void pack(PreferenceGroup group, String title, String id, String selected, boolean removable) {
        row(group, title, id.equals(selected) ? getText(R.string.item_selected) : null, () -> {
            if (!removable) {
                select(id);
                return;
            }
            new AlertDialog.Builder(this).setTitle(title)
                    .setItems(new CharSequence[]{getText(R.string.use_item), getText(R.string.remove_item)},
                            (dialog, choice) -> {
                                if (choice == 0) select(id);
                                else confirmRemove(id);
                            }).show();
        });
    }

    private void select(String name) {
        if (GameConfig.writeTexturePack(this, name)) prefs.setString(GamePreferences.TEXTURE_PACK, name);
        else toast(R.string.ini_write_failed);
        refresh();
    }

    private void confirmRemove(String name) {
        new AlertDialog.Builder(this).setTitle(R.string.remove_item).setMessage(name)
                .setNegativeButton(android.R.string.cancel, null)
                .setPositiveButton(android.R.string.ok, (dialog, which) -> {
                    try {
                        TexturePacks.remove(folder(), name);
                        if (name.equals(prefs.getString(GamePreferences.TEXTURE_PACK))) select("");
                        else refresh();
                    } catch (IOException error) {
                        Log.w("strikers", "Could not remove texture pack", error);
                        toast(R.string.texture_remove_failed);
                    }
                }).show();
    }

    private void pickZip() {
        try {
            startActivityForResult(new Intent(Intent.ACTION_OPEN_DOCUMENT)
                    .addCategory(Intent.CATEGORY_OPENABLE).setType("*/*"), PICK_PACK);
        } catch (ActivityNotFoundException error) {
            toast(R.string.no_file_picker);
        }
    }

    @Override
    protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        if (request != PICK_PACK || result != RESULT_OK || data == null || importing) return;
        Uri uri = data.getData();
        if (uri == null) return;
        importing = true;
        refresh();
        new Thread(() -> {
            String pack = null;
            try (InputStream input = getContentResolver().openInputStream(uri)) {
                pack = TexturePacks.install(folder(), input, DiscSelection.displayName(this, uri));
            } catch (IOException | IllegalArgumentException | SecurityException error) {
                Log.w("strikers", "Could not import texture pack", error);
            }
            String imported = pack;
            runOnUiThread(() -> {
                if (isDestroyed() || isFinishing()) return;
                importing = false;
                if (imported == null) {
                    toast(R.string.texture_import_failed);
                    refresh();
                } else {
                    select(imported);
                }
            });
        }, "import-textures").start();
    }
}
