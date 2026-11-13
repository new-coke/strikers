package io.github.newcoke.strikers;

import android.app.Activity;
import android.graphics.Color;
import android.os.Bundle;
import android.view.Gravity;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

public class TouchLayoutActivity extends Activity {
    @Override
    protected void attachBaseContext(android.content.Context base) {
        super.attachBaseContext(AppLocale.wrap(base));
    }

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        FrameLayout root = new FrameLayout(this);
        root.setBackgroundColor(Color.rgb(32, 43, 56));
        root.setFitsSystemWindows(true);
        OverlayView overlay = new OverlayView(this);
        overlay.applyPreferences(new GamePreferences(this));
        overlay.enableEditing();
        root.addView(overlay, new FrameLayout.LayoutParams(-1, -1));

        LinearLayout actions = new LinearLayout(this);
        Button reset = new Button(this);
        reset.setText(R.string.touch_reset_positions);
        reset.setOnClickListener(view -> overlay.resetPositions());
        actions.addView(reset);
        Button done = new Button(this);
        done.setText(android.R.string.ok);
        done.setOnClickListener(view -> finish());
        actions.addView(done);
        root.addView(actions, new FrameLayout.LayoutParams(-2, -2, Gravity.TOP | Gravity.CENTER_HORIZONTAL));
        setContentView(root);
    }
}
