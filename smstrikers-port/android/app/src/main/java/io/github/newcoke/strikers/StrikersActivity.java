package io.github.newcoke.strikers;

import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.ParcelFileDescriptor;
import android.net.Uri;
import android.system.ErrnoException;
import android.system.Os;
import android.util.Log;

import java.io.IOException;
import android.view.Gravity;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;

import org.libsdl.app.SDLActivity;
import org.libsdl.app.SDLSurface;

import dev.encounter.aurora.AuroraSurface;

public class StrikersActivity extends SDLActivity {

    private ParcelFileDescriptor disc;
    private OverlayView overlay;
    private HudView hud;
    private GamePreferences prefs;
    private final Handler handler = new Handler(Looper.getMainLooper());

    @Override
    public void loadLibraries() {
        Uri uri = getIntent().getData();
        try {
            if (uri == null) {
                Os.unsetenv("STRIKERS_ANDROID_DISC_FD");
            } else {
                disc = DiscSelection.open(this, uri);
                Os.setenv("STRIKERS_ANDROID_DISC_FD", Integer.toString(disc.getFd()), true);
            }
        } catch (IOException | SecurityException | ErrnoException error) {
            Log.w("strikers", "Could not open the selected disc", error);
            throw new IllegalStateException(getString(R.string.disc_open_failed), error);
        }
        super.loadLibraries();
    }

    @Override
    protected String[] getLibraries() {
        // SDL is linked into libmain.so.
        return new String[] { "main" };
    }

    @Override
    protected SDLSurface createSDLSurface(Context context) {
        // Aurora releases the swapchain when Android destroys the surface.
        return new AuroraSurface(context);
    }

    @Override
    protected void attachBaseContext(Context base) {
        super.attachBaseContext(AppLocale.wrap(base));
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        // Native library constructors read the environment set by prepare().
        StrikersHome.prepare(this);
        super.onCreate(savedInstanceState);
        if (mBrokenLibraries || isFinishing()) return;

        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        enterImmersiveMode();

        prefs = new GamePreferences(this);
        overlay = new OverlayView(this);
        overlay.applyPreferences(prefs);
        overlay.setSettingsListener(this::openSettings);
        addContentView(overlay, new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));

        hud = new HudView(this);
        hud.setVisibility(prefs.getBoolean(GamePreferences.HUD) ? View.VISIBLE : View.GONE);
        FrameLayout.LayoutParams hudLayout = new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        hudLayout.gravity = Gravity.TOP | Gravity.CENTER_HORIZONTAL;
        hudLayout.topMargin = Math.round(8 * getResources().getDisplayMetrics().density);
        addContentView(hud, hudLayout);

        watchController();
    }

    // Called by the shared invite panel on the SDL thread.
    public void shareRoomAddress(String address) {
        runOnUiThread(() -> {
            if (isFinishing() || isDestroyed()) return;
            Intent share = new Intent(Intent.ACTION_SEND)
                    .setType("text/plain")
                    .putExtra(Intent.EXTRA_TEXT, address);
            startActivity(Intent.createChooser(share, getText(R.string.share_room_address)));
        });
    }

    private void openSettings() {
        if (overlay != null) overlay.releaseAll();
        startActivity(new Intent(this, SettingsActivity.class));
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        // Android may reserve Guide on some devices. Leave gameplay buttons to SDL.
        if (overlay != null && event.isFromSource(InputDevice.SOURCE_GAMEPAD)
                && event.getKeyCode() == KeyEvent.KEYCODE_BUTTON_MODE) {
            if (event.getAction() == KeyEvent.ACTION_UP && !event.isCanceled()) openSettings();
            return true;
        }
        return super.dispatchKeyEvent(event);
    }

    private void enterImmersiveMode() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            getWindow().setDecorFitsSystemWindows(false);
            WindowInsetsController c = getWindow().getInsetsController();
            if (c != null) {
                c.hide(WindowInsets.Type.systemBars());
                c.setSystemBarsBehavior(
                        WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
            }
        } else {
            getWindow().getDecorView().setSystemUiVisibility(
                    View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                  | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                  | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                  | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                  | View.SYSTEM_UI_FLAG_FULLSCREEN
                  | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            getWindow().getAttributes().layoutInDisplayCutoutMode =
                    WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
        }
    }

    // The overlay follows the availability of the native virtual gamepad.
    private void watchController() {
        handler.postDelayed(new Runnable() {
            @Override public void run() {
                if (overlay != null) {
                    final boolean showOverlay = TouchGamepad.isActive();
                    final int visibility = showOverlay ? View.VISIBLE : View.GONE;
                    if (overlay.getVisibility() != visibility) {
                        if (!showOverlay) overlay.releaseAll();
                        overlay.setVisibility(visibility);
                    }
                }
                handler.postDelayed(this, 1000);
            }
        }, 1000);
    }

    @Override
    protected void onResume() {
        super.onResume();
        enterImmersiveMode();
        if (overlay != null && prefs != null) overlay.applyPreferences(prefs);
        if (hud != null && prefs != null) {
            hud.setVisibility(prefs.getBoolean(GamePreferences.HUD) ? View.VISIBLE : View.GONE);
        }
    }

    @Override
    protected void onPause() {
        if (overlay != null) overlay.releaseAll();
        super.onPause();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) enterImmersiveMode();
        else if (overlay != null) overlay.releaseAll();
    }

    @Override
    protected void onDestroy() {
        handler.removeCallbacksAndMessages(null);
        super.onDestroy();
        if (disc != null) {
            try {
                disc.close();
            } catch (IOException error) {
                Log.w("strikers", "Could not close the disc", error);
            }
        }
    }
}
