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
    private Button forgetDisc;
    private TextView discName;
    private boolean checkingDisc;
    private String appLanguage;

    @Override
    protected void attachBaseContext(Context base) {
        super.attachBaseContext(AppLocale.wrap(base));
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        appLanguage = new GamePreferences(this).getString(GamePreferences.APP_LANGUAGE);
        setContentView(R.layout.setup);

        discName = findViewById(R.id.disc_name);
        chooseDisc = findViewById(R.id.choose_disc);
        play = findViewById(R.id.play);
        forgetDisc = findViewById(R.id.forget_disc);
        forgetDisc.setOnClickListener(view -> {
            DiscSelection.forget(this);
            updateDisc();
            chooseDisc.requestFocus();
        });
        chooseDisc.setOnClickListener(view -> chooseDisc());
        play.setOnClickListener(view -> launchGame());
        findViewById(R.id.settings).setOnClickListener(view ->
                startActivity(new Intent(this, SettingsActivity.class)));
        updateDisc();
        (play.isEnabled() ? play : chooseDisc).requestFocus();
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (!appLanguage.equals(new GamePreferences(this).getString(GamePreferences.APP_LANGUAGE))) {
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
        forgetDisc.setVisibility(DiscSelection.uri(this) == null ? View.GONE : View.VISIBLE);
        forgetDisc.setEnabled(!checkingDisc);
        chooseDisc.setEnabled(!checkingDisc);
        play.setEnabled(!checkingDisc && name != null);
    }

    private void launchGame() {
        Uri uri = DiscSelection.uri(this);
        if (uri == null) {
            if (StrikersHome.findLocalGameData(this) == null) {
                updateDisc();
                showDiscError(getString(R.string.disc_open_failed));
                return;
            }
            startActivity(new Intent(this, StrikersActivity.class));
            return;
        }

        checkingDisc = true;
        updateDisc();
        new Thread(() -> {
            String error = null;
            try {
                DiscSelection.checkImage(this, uri);
            } catch (IOException | SecurityException e) {
                error = discErrorMessage(e);
            }
            String message = error;
            runOnUiThread(() -> {
                if (isFinishing() || isDestroyed()) return;
                checkingDisc = false;
                updateDisc();
                if (message == null) {
                    startActivity(new Intent(this, StrikersActivity.class).setData(uri));
                } else {
                    chooseDisc.requestFocus();
                    new AlertDialog.Builder(this).setTitle(R.string.disc_image)
                            .setMessage(message)
                            .setPositiveButton(R.string.disc_image, (dialog, which) -> chooseDisc())
                            .setNegativeButton(android.R.string.cancel, null)
                            .show();
                }
            });
        }, "disc-launch-check").start();
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
            } catch (IOException | SecurityException e) {
                error = discErrorMessage(e);
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

    private String discErrorMessage(Exception error) {
        String message = error instanceof IOException ? error.getLocalizedMessage() : null;
        return message != null ? message : getString(R.string.disc_open_failed);
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
