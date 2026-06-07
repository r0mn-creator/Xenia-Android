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

    @Override
    protected String getWindowedAppIdentifier() { return "xenia"; }

    @Override
    protected String getGameTitle() {
        return mGameTitle != null ? mGameTitle : super.getGameTitle();
    }

    @Override
    protected void onCreate(final Bundle savedInstanceState) {
        final Intent intent = getIntent();
        mGameTitle = resolveGameTitle(intent);

        final String gamePath = resolveGamePathForCore(intent);
        if (gamePath == null) {
            Toast.makeText(this, "No game path provided.", Toast.LENGTH_SHORT).show();
            finish();
            return;
        }

        Bundle cvars = intent.getBundleExtra(EXTRA_CVARS);
        if (cvars == null) cvars = new Bundle();
        cvars.putString("target", gamePath);

        final java.io.File extFiles = getExternalFilesDir(null);
        if (extFiles != null) {
            final String storageRoot = extFiles.getAbsolutePath();
            cvars.putString("storage_root", storageRoot);
            cvars.putString("content_root", storageRoot + "/content");
            cvars.putString("cache_root",   storageRoot + "/cache");
        }
        intent.putExtra(EXTRA_CVARS, cvars);

        super.onCreate(savedInstanceState);

        setContentView(R.layout.activity_emulator);
        setWindowSurfaceView(findViewById(R.id.emulator_surface_view));

        mOnScreenControls = findViewById(R.id.on_screen_controls);
        mOnScreenControls.setInputListener((buttons, lx, ly, rx, ry, lt, rt) ->
                injectControllerInput(buttons, lx, ly, rx, ry, lt, rt));

        mInputManager = (InputManager) getSystemService(INPUT_SERVICE);
    }

    @Override
    protected void onResume() {
        super.onResume();
        mInputManager.registerInputDeviceListener(mDeviceListener, null);
        updateOscVisibility();
    }

    @Override
    protected void onPause() {
        super.onPause();
        mInputManager.unregisterInputDeviceListener(mDeviceListener);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        closeGameFileFd();
    }

    // Toggle the overlay from the in-game menu "Controller Overlay" item.
    @Override
    protected void onMenuControlsClicked() {
        SharedPreferences prefs = getSharedPreferences(SettingsActivity.PREFS, MODE_PRIVATE);
        boolean current = prefs.getBoolean("controller_overlay", true);
        prefs.edit().putBoolean("controller_overlay", !current).apply();
        updateOscVisibility();
    }

    // ── On-screen controls visibility ─────────────────────────────────────────

    private void updateOscVisibility() {
        if (mOnScreenControls == null) return;
        SharedPreferences prefs = getSharedPreferences(SettingsActivity.PREFS, MODE_PRIVATE);
        boolean prefEnabled    = prefs.getBoolean("controller_overlay", true);
        boolean gamepadPresent = isGamepadConnected();
        boolean show = prefEnabled && !gamepadPresent;
        mOnScreenControls.setVisibility(show ? View.VISIBLE : View.GONE);
    }

    private boolean isGamepadConnected() {
        for (int id : InputDevice.getDeviceIds()) {
            InputDevice dev = InputDevice.getDevice(id);
            if (dev == null) continue;
            int src = dev.getSources();
            if ((src & InputDevice.SOURCE_GAMEPAD)  == InputDevice.SOURCE_GAMEPAD
             || (src & InputDevice.SOURCE_JOYSTICK) == InputDevice.SOURCE_JOYSTICK) {
                return true;
            }
        }
        return false;
    }

    // ── Intent parsing helpers ────────────────────────────────────────────────

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

    private void closeGameFileFd() {
        if (mGameFileFd != null) {
            try { mGameFileFd.close(); } catch (Exception ignored) {}
            mGameFileFd = null;
        }
    }

    static String resolveGamePath(Intent intent) {
        if (intent == null) return null;
        final String extra = intent.getStringExtra(EXTRA_GAME_PATH);
        if (extra != null && !extra.isEmpty()) return extra;
        final Uri data = intent.getData();
        return data != null ? data.toString() : null;
    }

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
