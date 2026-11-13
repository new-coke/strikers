package io.github.newcoke.strikers;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.RectF;
import android.graphics.Typeface;
import android.os.Build;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowInsets;

import java.util.ArrayList;
import java.util.List;

/** Draws the touch gamepad and converts pointer input to virtual controller state. */
public class OverlayView extends View {

    private static final int CONTROL_ACCENT = 0xFFF2E9D4;
    private static final int SETTINGS_ACCENT = 0xFFFFC552;

    private enum Role { BUTTON, LEFT_TRIGGER, RIGHT_TRIGGER, STICK, C_STICK, MENU }

    private enum Shape { CIRCLE, RECTANGLE, STICK }

    private static final class Control {
        final String label;
        final Shape shape;
        Role role = Role.BUTTON;
        int buttonMask;
        float cx, cy, radius;
        final RectF bounds = new RectF();
        int pointerId = -1;  // Active Android pointer ID, or -1.
        float pressure;  // Animated press, from 0 to 1.

        Control(String label, Shape shape) {
            this.label = label;
            this.shape = shape;
        }

        boolean contains(float x, float y) {
            if (shape == Shape.CIRCLE || shape == Shape.STICK) {
                final float dx = x - cx, dy = y - cy;
                // Allow touches just outside the visible circle.
                final float hitRadius = radius * 1.25f;
                return dx * dx + dy * dy <= hitRadius * hitRadius;
            }
            return bounds.contains(x, y);
        }
    }

    private final List<Control> controls = new ArrayList<>();
    private final Paint controlPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint labelPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Path path = new Path();

    private float leftStickDx, leftStickDy, cStickDx, cStickDy;
    private Control leftStick, cStick;

    private android.content.SharedPreferences positions;
    private Control dragging;
    private boolean editing;
    private float dragX, dragY;

    void enableEditing() {
        editing = true;
        opacity = Math.max(opacity, 0.75f);
        showDpad = showCStick = true;
    }

    void resetPositions() {
        positions.edit().clear().apply();
        layoutControls(getWidth(), getHeight());
        invalidate();
    }

    private float opacity = 1f;
    private String layout = GamePreferences.TOUCH_STANDARD;
    private float size = 1f;
    private float deadzone = 0.08f;
    private boolean showDpad;
    private boolean showCStick;

    // Keep controls clear of system bars and cutouts.
    private int leftInset, rightInset, topInset, bottomInset;

    interface SettingsListener { void openSettings(); }
    private SettingsListener settingsListener;

    public OverlayView(Context ctx) {
        super(ctx);
        setFocusable(false);
        labelPaint.setTypeface(Typeface.create("sans-serif-medium", Typeface.NORMAL));
        labelPaint.setTextAlign(Paint.Align.CENTER);
    }

    void setSettingsListener(SettingsListener listener) {
        settingsListener = listener;
    }

    void applyPreferences(GamePreferences prefs) {
        opacity = prefs.getInt(GamePreferences.TOUCH_OPACITY) / 100f;
        layout = prefs.getString(GamePreferences.TOUCH_LAYOUT);
        positions = getContext().getSharedPreferences("touch_positions_" + layout, Context.MODE_PRIVATE);
        size = prefs.getInt(GamePreferences.TOUCH_SIZE) / 100f;
        deadzone = prefs.getInt(GamePreferences.TOUCH_DEADZONE) / 100f;
        showDpad = prefs.getBoolean(GamePreferences.SHOW_DPAD);
        showCStick = prefs.getBoolean(GamePreferences.CSTICK);
        if (getWidth() > 0) {
            layoutControls(getWidth(), getHeight());
        }
        invalidate();
    }

    @Override
    public WindowInsets onApplyWindowInsets(WindowInsets insets) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            android.graphics.Insets b = insets.getInsets(
                    WindowInsets.Type.systemBars() | WindowInsets.Type.displayCutout());
            leftInset = b.left; rightInset = b.right; topInset = b.top; bottomInset = b.bottom;
        } else {
            leftInset = insets.getSystemWindowInsetLeft();
            rightInset = insets.getSystemWindowInsetRight();
            topInset = insets.getSystemWindowInsetTop();
            bottomInset = insets.getSystemWindowInsetBottom();
        }
        if (getWidth() > 0) {
            layoutControls(getWidth(), getHeight());
            invalidate();
        }
        return insets;
    }

    @Override
    protected void onSizeChanged(int w, int h, int ow, int oh) {
        layoutControls(w, h);
    }

    // Scale positions against the landscape bounds to keep controls within thumb reach.
    private void layoutControls(int w, int h) {
        releaseAll();
        dragging = null;
        controls.clear();
        leftStick = cStick = null;
        if (w == 0 || h == 0) return;

        final float u = Math.min(w, h);
        final boolean compact = GamePreferences.TOUCH_COMPACT.equals(layout);
        final boolean tablet = GamePreferences.TOUCH_TABLET.equals(layout);
        final float scale = size * (compact ? 0.85f : 1f);
        final float reachInset = tablet ? u * 0.06f : 0f;
        final float r = u * 0.062f * scale;
        final float margin = u * 0.055f;

        final float left = margin + leftInset + reachInset;
        final float right = w - margin - rightInset - reachInset;
        final float top = margin + topInset;
        final float bottom = h - margin - bottomInset;

        // LT and RT map to GameCube L and R. Either bumper maps to Z.
        final float shoulderWidth = u * 0.155f * scale, shoulderHeight = u * 0.085f * scale;

        float y = top;
        final Role[] leftRoles = {Role.LEFT_TRIGGER, Role.BUTTON, Role.BUTTON};
        final int[] leftMasks = {0, TouchGamepad.LB, TouchGamepad.L3};
        final String[] leftLabels = {"LT", "LB", "L3"};
        for (int i = 0; i < 3; i++) {
            Control c = new Control(leftLabels[i], Shape.RECTANGLE);
            c.role = leftRoles[i];
            c.buttonMask = leftMasks[i];
            c.bounds.set(left, y, left + shoulderWidth, y + shoulderHeight);
            controls.add(c);
            y += shoulderHeight * 1.18f;
        }

        y = top;
        final Role[] rightRoles = {Role.RIGHT_TRIGGER, Role.BUTTON, Role.BUTTON};
        final int[] rightMasks = {0, TouchGamepad.Z, TouchGamepad.R3};
        final String[] rightLabels = {"RT", "RB", "R3"};
        for (int i = 0; i < 3; i++) {
            Control c = new Control(rightLabels[i], Shape.RECTANGLE);
            c.role = rightRoles[i];
            c.buttonMask = rightMasks[i];
            c.bounds.set(right - shoulderWidth, y, right, y + shoulderHeight);
            controls.add(c);
            y += shoulderHeight * 1.18f;
        }

        final float fx = right - r * 2.4f;
        final float fy = bottom - r * 2.4f;
        final float d = r * 1.55f;
        controls.add(face("Y", fx, fy - d, r, TouchGamepad.Y));
        controls.add(face("X", fx - d, fy, r, TouchGamepad.X));
        controls.add(face("B", fx + d, fy, r, TouchGamepad.B));
        controls.add(face("A", fx, fy + d, r, TouchGamepad.A));

        final float rm = u * 0.115f * scale;
        leftStick = new Control("", Shape.STICK);
        leftStick.role = Role.STICK;
        leftStick.cx = left + rm * 1.15f;
        leftStick.cy = bottom - rm * 1.25f;
        leftStick.radius = rm;
        controls.add(leftStick);

        if (showCStick) {
            cStick = new Control("", Shape.STICK);
            cStick.role = Role.C_STICK;
            cStick.cx = w * 0.665f;
            cStick.cy = bottom - rm * 1.55f;
            cStick.radius = rm * 0.86f;
            controls.add(cStick);
        }

        if (showDpad) {
            final float rd = u * 0.045f * scale;
            final float dcx = left + rm * 1.15f;
            final float dcy = h * 0.50f;
            final float spacing = rd * 1.95f;
            controls.add(face("▲", dcx, dcy - spacing, rd, TouchGamepad.UP));
            controls.add(face("▼", dcx, dcy + spacing, rd, TouchGamepad.DOWN));
            controls.add(face("◀", dcx - spacing, dcy, rd, TouchGamepad.LEFT));
            controls.add(face("▶", dcx + spacing, dcy, rd, TouchGamepad.RIGHT));
        }

        final float centerButtonWidth = u * 0.115f * scale, centerButtonHeight = u * 0.052f * scale;
        final float settingsRadius = u * 0.036f * scale;
        final float centerGap = u * 0.028f;

        Control back = new Control("BACK", Shape.RECTANGLE);
        back.buttonMask = TouchGamepad.BACK;
        back.bounds.set(w * 0.5f - settingsRadius - centerGap - centerButtonWidth, bottom - centerButtonHeight,
                       w * 0.5f - settingsRadius - centerGap, bottom);
        controls.add(back);

        Control start = new Control("START", Shape.RECTANGLE);
        start.buttonMask = TouchGamepad.START;
        start.bounds.set(w * 0.5f + settingsRadius + centerGap, bottom - centerButtonHeight,
                         w * 0.5f + settingsRadius + centerGap + centerButtonWidth, bottom);
        controls.add(start);

        Control menu = new Control("", Shape.CIRCLE);
        menu.role = Role.MENU;
        menu.cx = w * 0.5f;
        menu.cy = bottom - centerButtonHeight / 2f;
        menu.radius = settingsRadius;
        controls.add(menu);
        for (Control control : controls) {
            String key = positionKey(control);
            if (positions != null && positions.contains(key + "_x")) {
                moveControl(control, leftInset + positions.getFloat(key + "_x", 0f) * usableWidth(),
                        topInset + positions.getFloat(key + "_y", 0f) * usableHeight());
            }
        }
    }

    private Control face(String label, float cx, float cy, float radius, int buttonMask) {
        Control c = new Control(label, Shape.CIRCLE);
        c.cx = cx; c.cy = cy; c.radius = radius; c.buttonMask = buttonMask;
        return c;
    }

    @Override
    protected void onDraw(Canvas canvas) {
        for (Control c : controls) {
            final float targetPressure = c.pointerId >= 0 ? 1f : 0f;
            c.pressure += (targetPressure - c.pressure) * 0.35f;
            if (Math.abs(targetPressure - c.pressure) > 0.003f) postInvalidateOnAnimation();
            else c.pressure = targetPressure;

            switch (c.shape) {
                case RECTANGLE: drawRectangle(canvas, c); break;
                case CIRCLE:    drawCircle(canvas, c);    break;
                case STICK:  drawStick(canvas, c);  break;
            }
        }
    }

    private int alpha(float value) {
        return Math.max(0, Math.min(255, Math.round(value * opacity)));
    }

    private void paintControl(Canvas canvas, Path shape, float pressure, int accent) {
        controlPaint.setColor(0xFF151D29);
        controlPaint.setAlpha(alpha(155));
        canvas.drawPath(shape, controlPaint);
        controlPaint.setColor(accent);
        controlPaint.setAlpha(alpha(22 + 62 * pressure));
        canvas.drawPath(shape, controlPaint);

        final float d = Math.max(1f, getResources().getDisplayMetrics().density);
        controlPaint.setStyle(Paint.Style.STROKE);
        controlPaint.setStrokeWidth(d * 2f);
        controlPaint.setColor(Color.BLACK);
        controlPaint.setAlpha(alpha(46));
        canvas.drawPath(shape, controlPaint);
        controlPaint.setStrokeWidth(d);
        controlPaint.setColor(accent);
        controlPaint.setAlpha(alpha(125 + 90 * pressure));
        canvas.drawPath(shape, controlPaint);
        controlPaint.setStyle(Paint.Style.FILL);
    }

    private void drawRectangle(Canvas canvas, Control c) {
        final float radius = c.bounds.height() * 0.42f;
        path.reset();
        path.addRoundRect(c.bounds, radius, radius, Path.Direction.CW);
        paintControl(canvas, path, c.pressure, c.role == Role.MENU ? SETTINGS_ACCENT : CONTROL_ACCENT);
        final float textSize = c.bounds.height() * (c.label.length() > 2 ? 0.30f : 0.42f);
        drawLabel(canvas, c.label, c.bounds.centerX(), c.bounds.centerY(), textSize, c.pressure);
    }

    private void drawCircle(Canvas canvas, Control c) {
        path.reset();
        path.addCircle(c.cx, c.cy, c.radius, Path.Direction.CW);
        paintControl(canvas, path, c.pressure, c.role == Role.MENU ? SETTINGS_ACCENT : CONTROL_ACCENT);
        if (c.role == Role.MENU) {
            drawSettingsIcon(canvas, c);
        } else {
            drawLabel(canvas, c.label, c.cx, c.cy, c.radius * 0.78f, c.pressure);
        }
    }

    private void drawSettingsIcon(Canvas canvas, Control c) {
        final float halfWidth = c.radius * 0.98f;
        final float d = Math.max(1f, getResources().getDisplayMetrics().density);
        final float[] knobOffsets = {-0.22f, 0.55f, -0.55f};

        controlPaint.setStrokeCap(Paint.Cap.ROUND);
        for (int i = 0; i < 3; i++) {
            final float y = c.cy + (i - 1) * halfWidth * 0.62f;
            controlPaint.setStyle(Paint.Style.STROKE);
            controlPaint.setStrokeWidth(d * 1.6f);
            controlPaint.setColor(Color.WHITE);
            controlPaint.setAlpha(alpha(228 + 27 * c.pressure));
            canvas.drawLine(c.cx - halfWidth, y, c.cx + halfWidth, y, controlPaint);
            controlPaint.setStyle(Paint.Style.FILL);
            canvas.drawCircle(c.cx + knobOffsets[i] * halfWidth, y, d * 2.6f, controlPaint);
        }
        controlPaint.setStrokeCap(Paint.Cap.BUTT);
    }

    private void drawStick(Canvas canvas, Control c) {
        final boolean isLeftStick = c == leftStick;
        final float dx = isLeftStick ? leftStickDx : cStickDx;
        final float dy = isLeftStick ? leftStickDy : cStickDy;

        // Draw a dark outline so the stick remains visible over bright scenes.
        final float d1 = getResources().getDisplayMetrics().density;
        controlPaint.setStyle(Paint.Style.STROKE);
        controlPaint.setStrokeWidth(d1 * 2.2f);
        controlPaint.setColor(Color.BLACK);
        controlPaint.setAlpha(alpha(58));
        canvas.drawCircle(c.cx, c.cy, c.radius, controlPaint);
        controlPaint.setStrokeWidth(d1 * 1.2f);
        controlPaint.setColor(Color.WHITE);
        controlPaint.setAlpha(alpha(120));
        canvas.drawCircle(c.cx, c.cy, c.radius, controlPaint);
        controlPaint.setStyle(Paint.Style.FILL);

        final float maxTravel = c.radius * 0.52f;
        final float dist = (float) Math.hypot(dx, dy);
        final float k = dist > maxTravel ? maxTravel / dist : 1f;
        path.reset();
        path.addCircle(c.cx + dx * k, c.cy + dy * k, c.radius * 0.56f, Path.Direction.CW);
        paintControl(canvas, path, c.pressure, c.role == Role.MENU ? SETTINGS_ACCENT : CONTROL_ACCENT);
    }

    private void drawLabel(Canvas canvas, String s, float cx, float cy,
                          float textSize, float pressure) {
        if (s == null || s.isEmpty()) return;
        labelPaint.setTextSize(textSize);
        labelPaint.setColor(Color.WHITE);
        labelPaint.setAlpha(alpha(216 + 39 * pressure));
        labelPaint.setShadowLayer(textSize * 0.18f, 0, textSize * 0.04f, 0x66000000);
        final Paint.FontMetrics fm = labelPaint.getFontMetrics();
        canvas.drawText(s, cx, cy - (fm.ascent + fm.descent) / 2f, labelPaint);
        labelPaint.clearShadowLayer();
    }

    // A pointer stays with the control it pressed until release or cancellation.
    @Override
    public boolean onTouchEvent(MotionEvent e) {
        if (editing) return editTouch(e);
        final int action = e.getActionMasked();
        switch (action) {
            case MotionEvent.ACTION_DOWN:
            case MotionEvent.ACTION_POINTER_DOWN: {
                final int i = e.getActionIndex();
                press(e.getPointerId(i), e.getX(i), e.getY(i));
                break;
            }
            case MotionEvent.ACTION_MOVE: {
                for (int i = 0; i < e.getPointerCount(); i++) {
                    move(e.getPointerId(i), e.getX(i), e.getY(i));
                }
                break;
            }
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_POINTER_UP: {
                final int index = e.getActionIndex();
                release(e.getPointerId(index), e.getX(index), e.getY(index));
                break;
            }
            case MotionEvent.ACTION_CANCEL:
                releaseAll();
                return true;
        }
        sendState();
        invalidate();
        return true;
    }

    private static String positionKey(Control control) {
        return control.role.name() + "_" + control.buttonMask;
    }

    private int usableWidth() {
        return Math.max(1, getWidth() - leftInset - rightInset);
    }

    private int usableHeight() {
        return Math.max(1, getHeight() - topInset - bottomInset);
    }

    private void moveControl(Control control, float x, float y) {
        boolean rectangle = control.shape == Shape.RECTANGLE;
        float halfWidth = rectangle ? control.bounds.width() / 2f : control.radius;
        float halfHeight = rectangle ? control.bounds.height() / 2f : control.radius;
        x = Math.max(leftInset + halfWidth, Math.min(x, getWidth() - rightInset - halfWidth));
        y = Math.max(topInset + halfHeight, Math.min(y, getHeight() - bottomInset - halfHeight));
        if (rectangle) control.bounds.offsetTo(x - halfWidth, y - halfHeight);
        else {
            control.cx = x;
            control.cy = y;
        }
    }

    private boolean editTouch(MotionEvent event) {
        float x = event.getX(), y = event.getY();
        switch (event.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
                for (int i = controls.size() - 1; i >= 0; --i) {
                    Control control = controls.get(i);
                    if (!control.contains(x, y)) continue;
                    dragging = control;
                    dragX = x - (control.shape == Shape.RECTANGLE ? control.bounds.centerX() : control.cx);
                    dragY = y - (control.shape == Shape.RECTANGLE ? control.bounds.centerY() : control.cy);
                    break;
                }
                break;
            case MotionEvent.ACTION_MOVE:
                if (dragging != null && event.getPointerCount() == 1) moveControl(dragging, x - dragX, y - dragY);
                break;
            case MotionEvent.ACTION_UP:
                if (dragging != null) {
                    float cx = dragging.shape == Shape.RECTANGLE ? dragging.bounds.centerX() : dragging.cx;
                    float cy = dragging.shape == Shape.RECTANGLE ? dragging.bounds.centerY() : dragging.cy;
                    String key = positionKey(dragging);
                    positions.edit()
                            .putFloat(key + "_x", (cx - leftInset) / usableWidth())
                            .putFloat(key + "_y", (cy - topInset) / usableHeight()).apply();
                    dragging = null;
                    performClick();
                }
                break;
            case MotionEvent.ACTION_POINTER_DOWN:
            case MotionEvent.ACTION_CANCEL:
                if (dragging != null) layoutControls(getWidth(), getHeight());
                break;
        }
        invalidate();
        return true;
    }

    @Override
    public boolean performClick() {
        return super.performClick();
    }

    private void press(int pointerId, float x, float y) {
        for (int i = controls.size() - 1; i >= 0; --i) {
            Control c = controls.get(i);
            if (c.pointerId < 0 && c.contains(x, y)) {
                c.pointerId = pointerId;
                if (c.shape == Shape.STICK) move(pointerId, x, y);
                return;
            }
        }
    }

    private void move(int pointerId, float x, float y) {
        for (Control c : controls) {
            if (c.pointerId == pointerId && c.shape == Shape.STICK) {
                if (c == leftStick) { leftStickDx = x - c.cx; leftStickDy = y - c.cy; }
                else             { cStickDx = x - c.cx; cStickDy = y - c.cy; }
            }
        }
    }

    private void release(int pointerId, float x, float y) {
        for (Control control : controls) {
            if (control.pointerId != pointerId) continue;
            control.pointerId = -1;
            if (control == leftStick) leftStickDx = leftStickDy = 0;
            if (control == cStick) cStickDx = cStickDy = 0;
            if (control.role == Role.MENU && control.contains(x, y) && settingsListener != null) {
                settingsListener.openSettings();
            }
            return;
        }
    }

    void releaseAll() {
        for (Control control : controls) {
            control.pointerId = -1;
        }
        leftStickDx = leftStickDy = cStickDx = cStickDy = 0;
        sendState();
        invalidate();
    }

    private void sendState() {
        if (editing) return;
        int buttons = 0;
        int leftTrigger = 0, rightTrigger = 0;
        for (Control c : controls) {
            if (c.pointerId < 0) continue;
            switch (c.role) {
                case BUTTON:       buttons |= c.buttonMask; break;
                case LEFT_TRIGGER: leftTrigger = 255;    break;
                case RIGHT_TRIGGER: rightTrigger = 255;    break;
                default: break;
            }
        }
        final int[] m = stickAxes(leftStick, leftStickDx, leftStickDy);
        final int[] c = stickAxes(cStick, cStickDx, cStickDy);
        TouchGamepad.sendState(buttons, leftTrigger, rightTrigger, m[0], m[1], c[0], c[1]);
    }

    // SDL stick Y increases downward. Apply the deadzone radially, then rescale the remaining travel.
    private int[] stickAxes(Control stick, float dx, float dy) {
        if (stick == null || stick.radius <= 0) return new int[]{0, 0};
        final float travel = stick.radius * 0.85f;
        float x = dx / travel, y = dy / travel;
        float magnitude = (float) Math.hypot(x, y);
        if (magnitude <= deadzone) return new int[]{0, 0};
        if (magnitude > 1f) { x /= magnitude; y /= magnitude; magnitude = 1f; }
        final float scaledMagnitude = (magnitude - deadzone) / (1f - deadzone);
        final float k = scaledMagnitude / magnitude;
        return new int[]{
            Math.round(Math.max(-1f, Math.min(1f, x * k)) * 32767f),
            Math.round(Math.max(-1f, Math.min(1f, y * k)) * 32767f)
        };
    }
}
