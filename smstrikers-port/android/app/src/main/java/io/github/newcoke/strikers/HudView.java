package io.github.newcoke.strikers;

import android.app.ActivityManager;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import android.graphics.Typeface;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.view.View;
import android.view.WindowInsets;

import java.io.RandomAccessFile;

/** Displays game frame timings and system metrics when the device exposes them. */
public class HudView extends View {

    private static final int GREEN   = 0xFF3DDC84;
    private static final int CYAN   = 0xFF3FC7E8;
    private static final int YELLOW = 0xFFF2C037;
    private static final int ORANGE = 0xFFFF8A3D;
    private static final int MUTED = 0xFF9AA3AD;

    private final Paint panelPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint labelPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint valuePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint unitPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final RectF bounds = new RectF();

    private final String[] labels;
    private final String soc;
    private String fps = "--", frameTime = "--", cpu = "--";
    private String ram = "--", temp = "--", gpu = "--";

    private final long[] metrics = new long[FrameMetrics.METRIC_COUNT];
    private final Handler handler = new Handler(Looper.getMainLooper());
    private int topInset;

    public HudView(Context ctx) {
        super(ctx);
        labels = new String[]{"FPS", ctx.getString(R.string.hud_frame), "CPU", "GPU", "RAM", "TEMP"};
        labelPaint.setTypeface(Typeface.create("sans-serif-medium", Typeface.BOLD));
        labelPaint.setTextAlign(Paint.Align.CENTER);
        valuePaint.setTypeface(Typeface.create("sans-serif", Typeface.BOLD));
        valuePaint.setTextAlign(Paint.Align.CENTER);
        valuePaint.setColor(Color.WHITE);
        unitPaint.setTypeface(Typeface.create("sans-serif-medium", Typeface.BOLD));
        unitPaint.setTextAlign(Paint.Align.CENTER);
        unitPaint.setColor(MUTED);

        soc = Build.VERSION.SDK_INT >= 31 && Build.SOC_MODEL != null
                && !"unknown".equals(Build.SOC_MODEL) ? Build.SOC_MODEL : Build.HARDWARE;
    }

    // Sample game timings at 4 Hz and system metrics once per second while visible.
    private final Runnable sample = new Runnable() {
        private int tick;

        @Override public void run() {
            readFrameMetrics();
            if (tick++ % 4 == 0) readSystemMetrics();
            invalidate();
            handler.postDelayed(this, 250);
        }
    };

    @Override
    public void onVisibilityAggregated(boolean visible) {
        super.onVisibilityAggregated(visible);
        handler.removeCallbacks(sample);
        if (visible) handler.post(sample);
    }

    @Override
    public WindowInsets onApplyWindowInsets(WindowInsets insets) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            topInset = insets.getInsets(
                    WindowInsets.Type.systemBars() | WindowInsets.Type.displayCutout()).top;
        } else {
            topInset = insets.getSystemWindowInsetTop();
        }
        requestLayout();
        return insets;
    }

    @Override
    protected void onDetachedFromWindow() {
        handler.removeCallbacks(sample);
        super.onDetachedFromWindow();
    }

    private void readFrameMetrics() {
        if (!FrameMetrics.read(metrics)) {
            fps = frameTime = cpu = "--";
            return;
        }
        final long us = metrics[FrameMetrics.FRAME_US];
        // Keep three-digit FPS values within the fixed-width column.
        final float f = metrics[FrameMetrics.FPS_TIMES_100] / 100f;
        fps = f >= 100f ? String.format("%.0f", f) : String.format("%.1f", f);
        frameTime = us > 0 ? String.format("%.1f", us / 1000f) : "--";
        cpu = String.format("%.2f", metrics[FrameMetrics.BUSY_US] / 1000f);
    }

    private void readSystemMetrics() {
        final ActivityManager am =
                (ActivityManager) getContext().getSystemService(Context.ACTIVITY_SERVICE);
        if (am != null) {
            final ActivityManager.MemoryInfo mi = new ActivityManager.MemoryInfo();
            am.getMemoryInfo(mi);
            if (mi.totalMem > 0) {
                ram = Long.toString(Math.round(100f * (mi.totalMem - mi.availMem) / mi.totalMem));
            }
        }
        temp = readTemperature();
        gpu = readGpuUsage();
    }

    private String readTemperature() {
        for (int i = 0; i < 16; i++) {
            final String type = readFirstLine("/sys/class/thermal/thermal_zone" + i + "/type");
            if (type == null) continue;
            final String t = type.toLowerCase();
            if (!(t.contains("cpu") || t.contains("soc") || t.contains("tsens")
                  || t.contains("ap"))) continue;
            final String raw = readFirstLine("/sys/class/thermal/thermal_zone" + i + "/temp");
            if (raw == null) continue;
            try {
                float v = Float.parseFloat(raw.trim());
                // Thermal zones can report degrees or millidegrees Celsius.
                if (v > 1000f) v /= 1000f;
                if (v > 10f && v < 150f) return String.format("%.0f", v);
            } catch (NumberFormatException ignored) { }
        }
        return "--";
    }

    private String readGpuUsage() {
        final String raw = readFirstLine("/sys/class/kgsl/kgsl-3d0/gpubusy");
        if (raw == null) return "--";
        final String[] fields = raw.trim().split("\\s+");
        if (fields.length < 2) return "--";
        try {
            final long busy = Long.parseLong(fields[0]);
            final long total = Long.parseLong(fields[1]);
            if (total <= 0) return "--";
            return Long.toString(Math.round(100f * busy / total));
        } catch (NumberFormatException e) {
            return "--";
        }
    }

    private static String readFirstLine(String path) {
        try (RandomAccessFile f = new RandomAccessFile(path, "r")) {
            return f.readLine();
        } catch (Exception e) {
            return null;
        }
    }

    private static final String[] UNITS = {"", "MS", "MS", "%", "%", "°C"};
    private static final int[] COLORS = {GREEN, CYAN, CYAN, YELLOW, YELLOW, ORANGE};

    private String valueAt(int i) {
        switch (i) {
            case 0: return fps;
            case 1: return frameTime;
            case 2: return cpu;
            case 3: return gpu;
            case 4: return ram;
            default: return temp;
        }
    }

    @Override
    protected void onMeasure(int widthSpec, int heightSpec) {
        final float d = getResources().getDisplayMetrics().density;
        // Fixed column widths keep the HUD stable as values change.
        setMeasuredDimension(Math.round(labels.length * 76 * d + 24 * d),
                             Math.round(88 * d));
    }

    @Override
    protected void onDraw(Canvas canvas) {
        final float d = getResources().getDisplayMetrics().density;
        final float w = getWidth(), h = getHeight();

        bounds.set(0, 0, w, h);
        final float r = 17 * d;
        panelPaint.setColor(0xD9000000);
        canvas.drawRoundRect(bounds, r, r, panelPaint);
        panelPaint.setStyle(Paint.Style.STROKE);
        panelPaint.setStrokeWidth(Math.max(1f, d));
        panelPaint.setColor(0x33FFFFFF);
        canvas.drawRoundRect(bounds, r, r, panelPaint);
        panelPaint.setStyle(Paint.Style.FILL);

        labelPaint.setTextSize(12 * d);
        valuePaint.setTextSize(29 * d);
        unitPaint.setTextSize(10 * d);

        final float columnWidth = (w - 24 * d) / labels.length;
        for (int i = 0; i < labels.length; i++) {
            final float cx = 12 * d + columnWidth * (i + 0.5f);

            if (i > 0) {
                panelPaint.setColor(0x26FFFFFF);
                canvas.drawRect(12 * d + columnWidth * i - 0.5f * d, 16 * d,
                                12 * d + columnWidth * i + 0.5f * d, h - 16 * d, panelPaint);
            }

            labelPaint.setColor(COLORS[i]);
            canvas.drawText(labels[i], cx, 24 * d, labelPaint);
            canvas.drawText(valueAt(i), cx, 57 * d, valuePaint);
            if (!UNITS[i].isEmpty()) {
                canvas.drawText(UNITS[i], cx, 75 * d, unitPaint);
            }
        }

        unitPaint.setTextAlign(Paint.Align.RIGHT);
        canvas.drawText(soc, w - 12 * d, h - 4 * d, unitPaint);
        unitPaint.setTextAlign(Paint.Align.CENTER);
    }

    int topInset() { return topInset; }
}
