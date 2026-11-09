// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Pro-Termux-arch
package io.github.protermuxarch.exynosprobe;

import android.app.Activity;
import android.os.Bundle;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.widget.LinearLayout;
import android.widget.TextView;
import java.io.File;
import java.io.FileWriter;
import java.util.concurrent.atomic.AtomicBoolean;

/** No network/storage/root permissions, no background service, no global driver change. */
public final class MainActivity extends Activity implements SurfaceHolder.Callback {
    private TextView status;
    private final AtomicBoolean started = new AtomicBoolean();
    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        status = new TextView(this);
        status.setText("Experimental GPU test. Stop if device becomes unstable.\n"
                + "Mode: " + getIntent().getStringExtra("mode") + "\n"
                + "Force-stop before changing driver. Stock is the default.\n");
        layout.addView(status);
        SurfaceView surface = new SurfaceView(this);
        surface.getHolder().addCallback(this);
        layout.addView(surface, new LinearLayout.LayoutParams(-1, 0, 1));
        setContentView(layout);
    }
    @Override public void surfaceCreated(final SurfaceHolder holder) {
        if (!started.compareAndSet(false, true)) return;
        final boolean custom = "panvk".equals(getIntent().getStringExtra("mode"));
        new Thread(() -> {
            String result;
            try {
                String dir = getApplicationInfo().nativeLibraryDir;
                String identity = Native.configure(dir, custom);
                if (!identity.startsWith("PASS ")) result = identity;
                else {
                    runOnUiThread(() -> status.append(identity + "\n"));
                    String queries = Native.preflight(dir, getFilesDir().getAbsolutePath());
                    runOnUiThread(() -> status.append(queries + " (queries only; not game tests)\n"));
                    String log = new File(getFilesDir(), "present.log").getAbsolutePath();
                    result = identity + "\n" + queries + "\n" + Native.swapchainTest(
                            dir + "/libexynospresent.so", holder.getSurface(), log);
                }
            } catch (Throwable error) { result = "FAIL " + error; }
            final String text = result;
            try (FileWriter w = new FileWriter(new File(getFilesDir(), "result.txt"))) { w.write(text); }
            catch (Exception error) { android.util.Log.e("ExynosProbe", "write result", error); }
            android.util.Log.i("ExynosProbe", text);
            runOnUiThread(() -> status.append(text + "\n"));
        }, "bounded-present-test").start();
    }
    @Override public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {}
    @Override public void surfaceDestroyed(SurfaceHolder holder) {}
}
