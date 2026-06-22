package jp.xenia.emulator;

import android.content.Intent;
import android.os.Bundle;
import android.view.MotionEvent;

/**
 * Xenia Android — boots a game by running the native "xenia" windowed app.
 *
 * The game path and cvars are handed to the engine via the EXTRA_CVARS intent bundle, which
 * Canary's native windowed_app_context_android.cc already reads (key = WindowedAppActivity
 * .EXTRA_CVARS) and applies as cvars before the emulator starts. We just need to populate it
 * before super.onCreate() runs the native init.
 *
 * Minimal first version: game path comes from the "xe_game_path" string extra (e.g. an adb
 * launch). Our richer launcher UI (game grid, box art, content-URI handling) is grafted later.
 */
public class EmulatorActivity extends WindowedAppActivity {
    @Override
    protected String getWindowedAppIdentifier() {
        // The emulator registers itself under this name (xenia_main.cc:303).
        return "xenia";
    }

    @Override
    protected void onCreate(final Bundle savedInstanceState) {
        final Intent intent = getIntent();

        // Build the cvar bundle the native side reads. Keys are cvar names.
        Bundle cvars = intent.getBundleExtra(EXTRA_CVARS);
        if (cvars == null) {
            cvars = new Bundle();
        }

        final String gamePath = intent.getStringExtra("xe_game_path");
        if (gamePath != null && !gamePath.isEmpty()) {
            cvars.putString("target", gamePath);  // cvars::target = the .iso/.xex to run
        }

        // Generic cvar passthrough: any string intent extra named "xe_<cvar>" becomes the cvar
        // <cvar> (e.g. `--es xe_log_level 2`, `--es xe_gpu d3d12`). Handy for debugging/bisection
        // from adb without rebuilding. (xe_game_path above is the one special-cased shorthand.)
        final Bundle allExtras = intent.getExtras();
        if (allExtras != null) {
            for (final String key : allExtras.keySet()) {
                if (key == null || !key.startsWith("xe_") || key.equals("xe_game_path")) continue;
                final Object v = allExtras.get(key);
                if (v instanceof String) cvars.putString(key.substring(3), (String) v);
            }
        }

        // GPU/APU explicit for Android (Vulkan; no-op audio for now — AAudio later).
        if (!cvars.containsKey("gpu")) cvars.putString("gpu", "vulkan");
        if (!cvars.containsKey("apu")) cvars.putString("apu", "aaudio");
        if (!cvars.containsKey("hid")) cvars.putString("hid", "android");

        // App-private storage for Xenia's roots so we don't need broad storage permission.
        final String storageRoot = getExternalFilesDir(null).getAbsolutePath();
        if (!cvars.containsKey("storage_root")) cvars.putString("storage_root", storageRoot);
        if (!cvars.containsKey("content_root")) cvars.putString("content_root", storageRoot + "/content");
        if (!cvars.containsKey("cache_root")) cvars.putString("cache_root", storageRoot + "/cache");

        intent.putExtra(EXTRA_CVARS, cvars);

        // super.onCreate() calls the native init, which reads EXTRA_CVARS we just set.
        super.onCreate(savedInstanceState);

        setContentView(R.layout.activity_window_demo);
        setWindowSurfaceView(findViewById(R.id.window_demo_surface_view));
    }

    // XENIA-ANDROID: temporary input — tap anywhere to press Start (release on
    // lift). Enough to leave the attract loop / navigate menus until a proper
    // on-screen control pad is grafted from the X360 UI.
    private static final int BTN_START = 1 << 8;  // kJavaBtnStart

    @Override
    public boolean dispatchTouchEvent(final MotionEvent ev) {
        switch (ev.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
                injectControllerInput(BTN_START, 0, 0, 0, 0, 0, 0);
                break;
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL:
                injectControllerInput(0, 0, 0, 0, 0, 0, 0);
                break;
            default:
                break;
        }
        return super.dispatchTouchEvent(ev);
    }
}
