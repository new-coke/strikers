package io.github.newcoke.strikers;

import android.app.AlertDialog;
import android.content.ActivityNotFoundException;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.preference.PreferenceGroup;
import android.preference.PreferenceScreen;
import android.widget.Toast;

@SuppressWarnings("deprecation")
public class TelaDrivers extends NativeSettingsActivity {
    private static final int PICK_ZIP = 4001;
    private boolean installing;

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        refresh();
    }

    private void refresh() {
        PreferenceScreen screen = page();
        PreferenceGroup installed = section(screen, R.string.sec_inuse);
        String selected = prefs.texto(Preferencias.DRIVER);
        driver(installed, getString(R.string.driver_system), getString(R.string.driver_system_sub),
                "", selected, null);
        for (Drivers.Driver driver : Drivers.lista(this)) {
            driver(installed, driver.nome, driver.descricao, driver.id, selected, driver);
        }
        PreferenceGroup add = section(screen, R.string.sec_add);
        row(add, R.string.import_zip, R.string.import_zip_sub, installing ? null : this::pickZip);
        for (Drivers.Fonte source : Drivers.fontes(this)) {
            row(add, source.nome, source.descricao + "\n" + source.url,
                    installing ? null : () -> install(null, source));
        }
        row(screen, R.string.video_driver, R.string.drivers_download_note, null);
        setPreferenceScreen(screen);
    }

    private void driver(PreferenceGroup group, String title, String description, String id,
                        String selected, Drivers.Driver driver) {
        String summary = id.equals(selected)
                ? getString(R.string.selected_item, description) : description;
        row(group, title, summary, () -> {
            if (driver == null) {
                select("");
                return;
            }
            new AlertDialog.Builder(this).setTitle(title)
                    .setItems(new CharSequence[]{getText(R.string.use_item), getText(R.string.remove_item)},
                            (dialog, choice) -> {
                                if (choice == 0) select(id);
                                else confirmRemove(driver);
                            }).show();
        });
    }

    private void select(String id) {
        prefs.definirTexto(Preferencias.DRIVER, id);
        refresh();
    }

    private void confirmRemove(Drivers.Driver driver) {
        new AlertDialog.Builder(this).setTitle(R.string.remove_item).setMessage(driver.nome)
                .setNegativeButton(android.R.string.cancel, null)
                .setPositiveButton(android.R.string.ok, (dialog, which) -> {
                    Drivers.remover(this, driver);
                    if (driver.id.equals(prefs.texto(Preferencias.DRIVER))) {
                        prefs.definirTexto(Preferencias.DRIVER, "");
                    }
                    toast(R.string.driver_removed);
                    refresh();
                }).show();
    }

    private void pickZip() {
        try {
            startActivityForResult(new Intent(Intent.ACTION_OPEN_DOCUMENT)
                    .addCategory(Intent.CATEGORY_OPENABLE).setType("*/*"), PICK_ZIP);
        } catch (ActivityNotFoundException e) {
            toast(R.string.no_file_picker);
        }
    }

    @Override
    protected void onActivityResult(int request, int result, Intent data) {
        super.onActivityResult(request, result, data);
        if (request == PICK_ZIP && result == RESULT_OK && data != null && data.getData() != null) {
            install(data.getData(), null);
        }
    }

    private void install(Uri uri, Drivers.Fonte source) {
        if (installing) return;
        installing = true;
        Toast.makeText(this, source == null ? getString(R.string.installing)
                : getString(R.string.downloading, source.nome), Toast.LENGTH_SHORT).show();
        refresh();
        new Thread(() -> {
            Drivers.Driver driver = uri == null ? Drivers.baixar(this, source) : Drivers.importar(this, uri);
            if (driver != null) prefs.definirTexto(Preferencias.DRIVER, driver.id);
            runOnUiThread(() -> {
                if (isDestroyed() || isFinishing()) return;
                installing = false;
                if (driver == null) toast(R.string.driver_failed);
                else Toast.makeText(this, getString(R.string.driver_installed, driver.nome),
                        Toast.LENGTH_LONG).show();
                refresh();
            });
        }, "install-driver").start();
    }
}
