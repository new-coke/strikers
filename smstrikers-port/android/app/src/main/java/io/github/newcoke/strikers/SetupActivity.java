package io.github.newcoke.strikers;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.view.KeyEvent;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;

import java.io.File;
import java.io.IOException;

public class SetupActivity extends Activity {
    private static final int PICK_DISC = 1;

    private Button chooseDisc;
    private Button play;
    private TextView discName;
    private boolean checkingDisc;
    private String appLanguage;

    @Override
    protected void attachBaseContext(Context base) {
        super.attachBaseContext(Idiomas.envolver(base));
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        appLanguage = new Preferencias(this).texto(Preferencias.IDIOMA_APP);
        setContentView(R.layout.setup);

        discName = findViewById(R.id.disc_name);
        chooseDisc = findViewById(R.id.choose_disc);
        play = findViewById(R.id.play);
        chooseDisc.setOnClickListener(view -> chooseDisc());
        play.setOnClickListener(view -> startActivity(
                new Intent(this, StrikersActivity.class).setData(DiscSelection.uri(this))));
        findViewById(R.id.settings).setOnClickListener(view ->
                startActivity(new Intent(this, SettingsActivity.class)));
        updateDisc();
        (play.isEnabled() ? play : chooseDisc).requestFocus();
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (!appLanguage.equals(new Preferencias(this).texto(Preferencias.IDIOMA_APP))) {
            recreate();
            return;
        }
        updateDisc();
    }

    private void updateDisc() {
        String name = DiscSelection.name(this);
        if (name == null) {
            File local = StrikersHome.findLocalGameData(this);
            if (local != null) name = local.getName();
        }
        discName.setText(name != null ? name : getString(R.string.no_disc_selected));
        chooseDisc.setEnabled(!checkingDisc);
        play.setEnabled(!checkingDisc && name != null);
    }

    private void chooseDisc() {
        Intent picker = new Intent(Intent.ACTION_OPEN_DOCUMENT)
                .addCategory(Intent.CATEGORY_OPENABLE)
                .setType("*/*")
                .addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION
                        | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
        try {
            startActivityForResult(picker, PICK_DISC);
        } catch (ActivityNotFoundException error) {
            showDiscError(getString(R.string.disc_open_failed));
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent result) {
        super.onActivityResult(requestCode, resultCode, result);
        if (requestCode != PICK_DISC || resultCode != RESULT_OK || result == null) return;
        Uri uri = result.getData();
        if (uri == null) return;
        int readGrant = result.getFlags() & Intent.FLAG_GRANT_READ_URI_PERMISSION;
        checkingDisc = true;
        updateDisc();
        new Thread(() -> {
            String error = null;
            try {
                DiscSelection.checkImage(this, uri);
                DiscSelection.remember(this, uri, DiscSelection.displayName(this, uri), readGrant);
            } catch (IOException e) {
                error = e.getLocalizedMessage() != null
                        ? e.getLocalizedMessage() : getString(R.string.disc_open_failed);
            } catch (SecurityException e) {
                error = getString(R.string.disc_open_failed);
            }
            String message = error;
            runOnUiThread(() -> {
                if (isFinishing() || isDestroyed()) return;
                checkingDisc = false;
                updateDisc();
                if (message != null) showDiscError(message);
                else play.requestFocus();
            });
        }, "disc-picker").start();
    }

    private void showDiscError(String message) {
        new AlertDialog.Builder(this).setTitle(R.string.disc_image)
                .setMessage(message).setPositiveButton(android.R.string.ok, null).show();
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        View focus = getCurrentFocus();
        if (event.getKeyCode() == KeyEvent.KEYCODE_BUTTON_A && focus instanceof Button
                && focus.isEnabled()) {
            if (event.getAction() == KeyEvent.ACTION_UP && !event.isCanceled()) {
                focus.performClick();
            }
            return true;
        }
        return super.dispatchKeyEvent(event);
    }
}
