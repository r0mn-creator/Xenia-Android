package jp.xenia.emulator;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.hardware.input.InputManager;
import android.net.Uri;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.view.InputDevice;
import android.view.View;
import android.widget.Toast;

/**
 * Launches and runs a game via the Xenia C++ core.
 *
 * External frontends can launch via ACTION_VIEW with the game file URI:
 *   am start -n jp.xenia.emulator/.EmulatorActivity \
 *             -a android.intent.action.VIEW \
 *             -d "content://..." (or file:///...)
 *
 * Or with an explicit path extra (ES-DE / custom frontends):
 *   am start -n jp.xenia.emulator/.EmulatorActivity \
 *             --es jp.xenia.emulator.EmulatorActivity.GAME_PATH "/sdcard/game.iso" \
 *             --es jp.xenia.emulator.EmulatorActivity.GAME_TITLE "Halo 3"
 *
 * When launched from an external frontend the activity finishes back to it on
 * exit. When launched from our own LauncherActivity it returns to the game
 * selection screen.
 */
public class EmulatorActivity extends WindowedAppActivity {

    /** String extra: absolute path or content URI string of the game file. */
    public static final String EXTRA_GAME_PATH =
            "jp.xenia.emulator.EmulatorActivity.GAME_PATH";

    /** String extra: display title for the game (used in the in-game menu). */
    public static final String EXTRA_GAME_TITLE =
            "jp.xenia.emulator.EmulatorActivity.GAME_TITLE";

    /**
     * Boolean extra set by LauncherActivity when it starts this activity.
     * Absent (or false) means we were launched by an external frontend.
     */
    static final String EXTRA_FROM_LAUNCHER =
            "jp.xenia.emulator.EmulatorActivity.FROM_LAUNCHER";

    // ── Factory ───────────────────────────────────────────────────────────────

    /** Builds an intent for launching EmulatorActivity from within the app. */
    static Intent createInternalIntent(Context context, String gamePath, String gameTitle) {
        return new Intent(context, EmulatorActivity.class)
                .putExtra(EXTRA_GAME_PATH, gamePath)
                .putExtra(EXTRA_GAME_TITLE, gameTitle)
                .putExtra(EXTRA_FROM_LAUNCHER, true);
    }

    // ── State ─────────────────────────────────────────────────────────────────

    private String                mGameTitle;
    private ParcelFileDescriptor  mGameFileFd;
    private OnScreenControlsView  mOnScreenControls;
    private InputManager          mInputManager;

    private final InputManager.InputDeviceListener mDeviceListener =
            new InputManager.InputDeviceListener() {
                @Override public void onInputDeviceAdded(int id)   { updateOscVisibility(); }
                @Override public void onInputDeviceRemoved(int id) { updateOscVisibility(); }
                @Override public void onInputDeviceChanged(int id) { updateOscVisibility(); }
            };

    // ── WindowedAppActivity overrides ─────────────────────────────────────────

    /** Returns the identifier string used to load the native xenia library. */
    @Override
    protected String getWindowedAppIdentifier() { return "xenia"; }

    // XENIA-ANDROID: our WindowedAppActivity (canary base) has no getGameTitle()
    // / onMenuControlsClicked() / in-game-menu hooks (those were in the old
    // hand-written-backend base), so those overrides are dropped here. mGameTitle
    // is still tracked for future use.

    /**
     * Resolves the game path and cvars from the launch intent, then initializes
     * the native emulator core, Vulkan surface, and on-screen controls.
     */
    @Override
    protected void onCreate(final Bundle savedInstanceState) {
        final Intent intent = getIntent();
        mGameTitle = resolveGameTitle(intent);

        final String gamePath = resolveGamePathForCore(intent);
        if (gamePath == null) {
            super.onCreate(savedInstanceState);
            Toast.makeText(this, "No game path provided.", Toast.LENGTH_SHORT).show();
            finish();
            return;
        }

        Bundle cvars = intent.getBundleExtra(EXTRA_CVARS);
        if (cvars == null) cvars = new Bundle();
        cvars.putString("target", gamePath);

        // DIAGNOSTIC: allow overriding ANY cvar from an intent string extra
        // named "xe_<cvar>" (e.g. --es xe_apu nop / --es xe_gpu null /
        // --es xe_diag_skip_draw true) so subsystems can be bisected without
        // rebuilding. The native bundle parser keys off the exact cvar name and
        // type-dispatches (getBoolean/getInt/getString), so infer the type:
        //   "true"/"false" -> boolean,  all-digits -> int,  else -> string.
        final Bundle extras = intent.getExtras();
        if (extras != null) {
            for (final String extraKey : extras.keySet()) {
                if (extraKey == null || !extraKey.startsWith("xe_")) continue;
                final String cvarName = extraKey.substring(3);
                final String v = intent.getStringExtra(extraKey);
                if (v == null) continue;
                if (v.equalsIgnoreCase("true") || v.equalsIgnoreCase("false")) {
                    cvars.putBoolean(cvarName, v.equalsIgnoreCase("true"));
                } else {
                    try {
                        cvars.putInt(cvarName, Integer.parseInt(v));
                    } catch (NumberFormatException nfe) {
                        cvars.putString(cvarName, v);
                    }
                }
            }
        }

        // XENIA-ANDROID: canary backend defaults (the old hand-written build set
        // these in native). Vulkan GPU, our AAudio APU, our Android HID driver.
        if (!cvars.containsKey("gpu")) cvars.putString("gpu", "vulkan");
        if (!cvars.containsKey("apu")) cvars.putString("apu", "aaudio");
        if (!cvars.containsKey("hid")) cvars.putString("hid", "android");

        final java.io.File extFiles = getExternalFilesDir(null);
        if (extFiles != null) {
            final String storageRoot = extFiles.getAbsolutePath();
            cvars.putString("storage_root", storageRoot);
            cvars.putString("content_root", storageRoot + "/content");
            cvars.putString("cache_root",   storageRoot + "/cache");
        }

        // XENIA-ANDROID: apply the custom GPU driver selected in Settings (if any). The adrenotools
        // loader needs the driver dir (internal/exec), the hook libs (our native lib dir), and a
        // writable+exec temp dir. Empty = use the system libvulkan.so.
        final SharedPreferences sprefs = getSharedPreferences(SettingsActivity.PREFS, MODE_PRIVATE);
        final String drvDir  = sprefs.getString(DriverManager.PREF_DRIVER_DIR, "");
        final String drvName = sprefs.getString(DriverManager.PREF_DRIVER_NAME, "");
        if (!drvDir.isEmpty() && !drvName.isEmpty()) {
            cvars.putString("custom_gpu_driver_dir", drvDir);
            cvars.putString("custom_gpu_driver_name", drvName);
            cvars.putString("gpu_driver_hook_dir", getApplicationInfo().nativeLibraryDir);
            final java.io.File tmp = new java.io.File(getCodeCacheDir(), "gpu_driver");
            //noinspection ResultOfMethodCallIgnored
            tmp.mkdirs();
            cvars.putString("gpu_driver_tmp_dir", tmp.getAbsolutePath());
        }
        intent.putExtra(EXTRA_CVARS, cvars);

        super.onCreate(savedInstanceState);

        setContentView(R.layout.activity_emulator);
        enableImmersiveFullscreen();
        setWindowSurfaceView(findViewById(R.id.emulator_surface_view));

        mOnScreenControls = findViewById(R.id.on_screen_controls);
        mOnScreenControls.setInputListener((buttons, lx, ly, rx, ry, lt, rt) ->
                injectControllerInput(buttons, lx, ly, rx, ry, lt, rt));

        mInputManager = (InputManager) getSystemService(INPUT_SERVICE);
    }

    // Run the emulator in immersive sticky fullscreen: hide the status/nav bars
    // so the framework's WindowInsets/vsync animation path has nothing to drive.
    // (Besides being the correct presentation for a game, this avoids the
    // InsetsAnimation thread running during the heavy native launch.)
    private void enableImmersiveFullscreen() {
        getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_LAYOUT_STABLE
              | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
              | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
              | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
              | View.SYSTEM_UI_FLAG_FULLSCREEN
              | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
    }

    /** Re-applies immersive fullscreen flags whenever the window regains focus. */
    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) enableImmersiveFullscreen();
    }

    /** Registers the input-device listener and refreshes on-screen controls visibility. */
    @Override
    protected void onResume() {
        super.onResume();
        mInputManager.registerInputDeviceListener(mDeviceListener, null);
        updateOscVisibility();
    }

    /** Unregisters the input-device listener when the activity is paused. */
    @Override
    protected void onPause() {
        super.onPause();
        mInputManager.unregisterInputDeviceListener(mDeviceListener);
    }

    /** Closes the game file descriptor (used for content:// URIs) on teardown. */
    @Override
    protected void onDestroy() {
        super.onDestroy();
        closeGameFileFd();
    }

    // XENIA-ANDROID: onMenuControlsClicked() override removed — our canary
    // WindowedAppActivity has no in-game menu hook to call it.

    // ── On-screen controls visibility ─────────────────────────────────────────

    /** Shows or hides the on-screen gamepad overlay based on user prefs and connected hardware. */
    private void updateOscVisibility() {
        if (mOnScreenControls == null) return;
        SharedPreferences prefs = getSharedPreferences(SettingsActivity.PREFS, MODE_PRIVATE);
        boolean prefEnabled    = prefs.getBoolean("controller_overlay", true);
        boolean gamepadPresent = isGamepadConnected();
        boolean show = prefEnabled && !gamepadPresent;
        mOnScreenControls.setVisibility(show ? View.VISIBLE : View.GONE);
    }

    /** Returns true if at least one physical gamepad (controller number > 0) is connected. */
    private boolean isGamepadConnected() {
        for (int id : InputDevice.getDeviceIds()) {
            InputDevice dev = InputDevice.getDevice(id);
            // getControllerNumber() returns non-zero only for real physical controllers
            // (API 19+). Source-flag checks alone match virtual/sensor devices and
            // incorrectly suppress the on-screen overlay on phones with no gamepad.
            if (dev != null && dev.getControllerNumber() > 0) return true;
        }
        return false;
    }

    // ── Intent parsing helpers ────────────────────────────────────────────────

    /**
     * Converts the raw game path from the intent into a form the C++ core can open:
     * absolute paths pass through; file:// URIs are decoded; content:// URIs are
     * opened as a file descriptor and returned as /proc/self/fd/<n>.
     */
    private String resolveGamePathForCore(Intent intent) {
        final String raw = resolveGamePath(intent);
        if (raw == null) return null;

        if (raw.startsWith("/"))         return raw;
        if (raw.startsWith("file://"))   return Uri.parse(raw).getPath();

        if (raw.startsWith("content://")) {
            try {
                try {
                    getContentResolver().takePersistableUriPermission(
                            Uri.parse(raw), Intent.FLAG_GRANT_READ_URI_PERMISSION);
                } catch (SecurityException ignored) {}

                mGameFileFd = getContentResolver().openFileDescriptor(Uri.parse(raw), "r");
                if (mGameFileFd != null) {
                    return "/proc/self/fd/" + mGameFileFd.getFd();
                }
            } catch (Exception e) {
                Toast.makeText(this, "Cannot open game file: " + e.getMessage(),
                        Toast.LENGTH_LONG).show();
            }
            return null;
        }
        return raw;
    }

    /** Closes and clears the content:// game file descriptor if one was opened. */
    private void closeGameFileFd() {
        if (mGameFileFd != null) {
            try { mGameFileFd.close(); } catch (Exception ignored) {}
            mGameFileFd = null;
        }
    }

    /**
     * Extracts the raw game path string from the intent: checks EXTRA_GAME_PATH,
     * then the "xe_game_path" shorthand (used by adb test launches), then getData().
     */
    static String resolveGamePath(Intent intent) {
        if (intent == null) return null;
        final String extra = intent.getStringExtra(EXTRA_GAME_PATH);
        if (extra != null && !extra.isEmpty()) return extra;
        // Short-form "xe_game_path" accepted for adb test launches.
        final String xeExtra = intent.getStringExtra("xe_game_path");
        if (xeExtra != null && !xeExtra.isEmpty()) return xeExtra;
        final Uri data = intent.getData();
        return data != null ? data.toString() : null;
    }

    /**
     * Extracts the display title from the intent: checks EXTRA_GAME_TITLE,
     * then strips the extension from the URI's last path segment, then defaults
     * to "Unknown Game".
     */
    static String resolveGameTitle(Intent intent) {
        if (intent == null) return "Unknown Game";
        final String extra = intent.getStringExtra(EXTRA_GAME_TITLE);
        if (extra != null && !extra.isEmpty()) return extra;
        final Uri data = intent.getData();
        if (data != null) {
            final String last = data.getLastPathSegment();
            if (last != null)
                return last.replaceFirst("(?i)\\.(iso|xex|zar|xbla)$", "");
        }
        return "Unknown Game";
    }
}
