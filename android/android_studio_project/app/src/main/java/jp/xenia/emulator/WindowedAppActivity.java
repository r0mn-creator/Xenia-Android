package jp.xenia.emulator;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Intent;
import android.content.res.AssetManager;
import android.os.Bundle;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.TextView;

import org.jetbrains.annotations.Nullable;

import jp.xenia.XeniaRuntimeException;

public abstract class WindowedAppActivity extends Activity {
    // The EXTRA_CVARS value literal is also used in the native code.

    /**
     * Name of the Bundle intent extra containing Xenia config variable launch arguments.
     */
    public static final String EXTRA_CVARS = "jp.xenia.emulator.WindowedAppActivity.EXTRA_CVARS";

    static {
        System.loadLibrary("xenia-app");
    }

    private final WindowSurfaceListener mWindowSurfaceListener = new WindowSurfaceListener();

    // May be 0 while destroying (mainly while the superclass is).
    private long mAppContext = 0;

    @Nullable
    private WindowSurfaceView mWindowSurfaceView = null;

    // In-game menu
    private View mScrim;
    private View mMenuPanel;
    private boolean mMenuVisible = false;

    // Emulation suspend state — two independent triggers.
    // Emulation is suspended when either is true; resumes only when both are false.
    private boolean mSuspendedForMenu = false;
    private boolean mSuspendedForBackground = false;

    private native long initializeWindowedAppOnCreate(
            String windowedAppIdentifier, AssetManager assetManager);

    private native void onDestroyNative(long appContext);

    private native void onAndroidSuspend(long appContext);

    private native void onAndroidResume(long appContext);

    private native void onWindowSurfaceLayoutChange(
            long appContext, int left, int top, int right, int bottom);

    private native boolean onWindowSurfaceMotionEvent(long appContext, MotionEvent event);

    private native void onWindowSurfaceChanged(long appContext, Surface windowSurface);

    private native void paintWindow(long appContext, boolean forcePaint);

    private native void injectControllerInputNative(
            int buttonMask, float leftX, float leftY,
            float rightX, float rightY,
            float leftTrigger, float rightTrigger);

    protected abstract String getWindowedAppIdentifier();

    /** Override in subclasses to show the game title in the in-game menu. */
    protected String getGameTitle() {
        return String.valueOf(getTitle());
    }

    protected void setWindowSurfaceView(@Nullable final WindowSurfaceView windowSurfaceView) {
        if (mWindowSurfaceView == windowSurfaceView) {
            return;
        }

        // Detach from the old surface.
        if (mWindowSurfaceView != null) {
            mWindowSurfaceView.getHolder().removeCallback(mWindowSurfaceListener);
            mWindowSurfaceView.setOnTouchListener(null);
            mWindowSurfaceView.setOnGenericMotionListener(null);
            mWindowSurfaceView.removeOnLayoutChangeListener(mWindowSurfaceListener);
            mWindowSurfaceView = null;
            if (mAppContext != 0) {
                onWindowSurfaceChanged(mAppContext, null);
            }
        }

        if (windowSurfaceView == null) {
            return;
        }

        mWindowSurfaceView = windowSurfaceView;
        // FIXME(Triang3l): This doesn't work if the layout has already been performed.
        mWindowSurfaceView.addOnLayoutChangeListener(mWindowSurfaceListener);
        mWindowSurfaceView.setOnGenericMotionListener(mWindowSurfaceListener);
        mWindowSurfaceView.setOnTouchListener(mWindowSurfaceListener);
        final SurfaceHolder windowSurfaceHolder = mWindowSurfaceView.getHolder();
        windowSurfaceHolder.addCallback(mWindowSurfaceListener);
        // If setting after the creation of the surface.
        if (mAppContext != 0) {
            final Surface windowSurface = windowSurfaceHolder.getSurface();
            if (windowSurface != null) {
                onWindowSurfaceChanged(mAppContext, windowSurface);
            }
        }
    }

    public void onWindowSurfaceDraw(final boolean forcePaint) {
        if (mAppContext == 0) {
            return;
        }
        paintWindow(mAppContext, forcePaint);
    }

    // Used from the native WindowedAppContext. May be called from non-UI threads.
    @SuppressWarnings("UnusedDeclaration")
    protected void postInvalidateWindowSurface() {
        if (mWindowSurfaceView == null) {
            return;
        }
        mWindowSurfaceView.postInvalidate();
    }

    @Override
    protected void onCreate(final Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        final String windowedAppIdentifier = getWindowedAppIdentifier();
        mAppContext = initializeWindowedAppOnCreate(windowedAppIdentifier, getAssets());
        if (mAppContext == 0) {
            finish();
            throw new XeniaRuntimeException(
                    "Error initializing the windowed app " + windowedAppIdentifier);
        }
    }

    @Override
    protected void onPostCreate(@Nullable Bundle savedInstanceState) {
        super.onPostCreate(savedInstanceState);
        setupInGameMenu();
    }

    /**
     * Called when the user taps "Controller Overlay" in the in-game menu.
     * Override in subclasses to toggle the on-screen controller visibility.
     */
    protected void onMenuControlsClicked() {}

    /**
     * Inject virtual gamepad state produced by the on-screen controls.
     * Override or extend in a subclass to forward the state to the C++ HID layer.
     * buttonMask uses {@link OnScreenControlsView}.BTN_* constants.
     * Axis values are in [-1, 1]; trigger values in [0, 1].
     */
    protected void injectControllerInput(int buttonMask,
            float leftX, float leftY, float rightX, float rightY,
            float leftTrigger, float rightTrigger) {
        injectControllerInputNative(buttonMask, leftX, leftY, rightX, rightY,
                leftTrigger, rightTrigger);
    }

    private void setupInGameMenu() {
        final int panelWidthPx = (int) (260 * getResources().getDisplayMetrics().density);

        // Semi-transparent scrim behind the panel — tapping it closes the menu.
        mScrim = new View(this);
        mScrim.setBackgroundColor(0x80000000);
        mScrim.setVisibility(View.GONE);
        mScrim.setOnClickListener(v -> hideInGameMenu());
        addContentView(mScrim, new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT));

        // Panel slides in from the right.
        mMenuPanel = getLayoutInflater().inflate(R.layout.fragment_ingame_menu, null);
        mMenuPanel.setVisibility(View.GONE);
        addContentView(mMenuPanel, new FrameLayout.LayoutParams(
                panelWidthPx, FrameLayout.LayoutParams.MATCH_PARENT, Gravity.END));

        ((TextView) mMenuPanel.findViewById(R.id.text_game_title)).setText(getGameTitle());

        mMenuPanel.findViewById(R.id.menu_resume).setOnClickListener(v -> hideInGameMenu());
        mMenuPanel.findViewById(R.id.menu_pause).setOnClickListener(v -> hideInGameMenu());
        mMenuPanel.findViewById(R.id.menu_controls).setOnClickListener(v -> {
            hideInGameMenu();
            onMenuControlsClicked();
        });
        mMenuPanel.findViewById(R.id.menu_settings).setOnClickListener(v ->
                startActivity(new Intent(this, SettingsActivity.class)));
        mMenuPanel.findViewById(R.id.menu_exit).setOnClickListener(v -> exitToLauncher());
    }

    private void showInGameMenu() {
        if (mMenuVisible) return;
        mMenuVisible = true;
        mSuspendedForMenu = true;
        doSuspend();

        // Show Resume, hide Pause (opening the menu IS the pause action).
        mMenuPanel.findViewById(R.id.menu_pause).setVisibility(View.GONE);
        mMenuPanel.findViewById(R.id.menu_resume).setVisibility(View.VISIBLE);

        final float panelW = mMenuPanel.getLayoutParams() != null
                ? ((FrameLayout.LayoutParams) mMenuPanel.getLayoutParams()).width
                : 260 * getResources().getDisplayMetrics().density;

        mScrim.setAlpha(0f);
        mScrim.setVisibility(View.VISIBLE);
        mMenuPanel.setTranslationX(panelW);
        mMenuPanel.setVisibility(View.VISIBLE);

        mMenuPanel.animate().translationX(0).setDuration(250).start();
        mScrim.animate().alpha(1f).setDuration(250).start();
    }

    private void hideInGameMenu() {
        if (!mMenuVisible) return;
        mMenuVisible = false;
        mSuspendedForMenu = false;
        doResumeIfClear();

        final float panelW = mMenuPanel.getLayoutParams() != null
                ? ((FrameLayout.LayoutParams) mMenuPanel.getLayoutParams()).width
                : 260 * getResources().getDisplayMetrics().density;

        mMenuPanel.animate().translationX(panelW).setDuration(200)
                .withEndAction(() -> mMenuPanel.setVisibility(View.GONE)).start();
        mScrim.animate().alpha(0f).setDuration(200)
                .withEndAction(() -> mScrim.setVisibility(View.GONE)).start();
    }

    @Override
    public void onBackPressed() {
        if (mMenuVisible) {
            hideInGameMenu();
        } else {
            showInGameMenu();
        }
    }

    private void exitToLauncher() {
        // If launched from an external frontend (no FROM_LAUNCHER flag), just
        // finish() — Android's back stack returns us to the frontend naturally.
        final boolean fromLauncher = getIntent()
                .getBooleanExtra(EmulatorActivity.EXTRA_FROM_LAUNCHER, false);
        if (fromLauncher) {
            final Intent intent = new Intent(this, LauncherActivity.class);
            intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_SINGLE_TOP);
            startActivity(intent);
        }
        finish();
    }

    private void doSuspend() {
        if (mAppContext != 0 && !mSuspendedForMenu && !mSuspendedForBackground) {
            onAndroidSuspend(mAppContext);
        }
    }

    private void doResumeIfClear() {
        if (mAppContext != 0 && !mSuspendedForMenu && !mSuspendedForBackground) {
            onAndroidResume(mAppContext);
        }
    }

    @Override
    protected void onStop() {
        super.onStop();
        mSuspendedForBackground = true;
        doSuspend();
    }

    @Override
    protected void onStart() {
        super.onStart();
        mSuspendedForBackground = false;
        doResumeIfClear();
    }

    @Override
    protected void onDestroy() {
        setWindowSurfaceView(null);
        if (mAppContext != 0) {
            onDestroyNative(mAppContext);
        }
        mAppContext = 0;
        super.onDestroy();
    }

    private class WindowSurfaceListener implements
            View.OnGenericMotionListener,
            View.OnLayoutChangeListener,
            View.OnTouchListener,
            SurfaceHolder.Callback2 {
        @Override
        public void onLayoutChange(
                final View v, final int left, final int top, final int right, final int bottom,
                final int oldLeft, final int oldTop, final int oldRight, final int oldBottom) {
            if (mAppContext != 0) {
                onWindowSurfaceLayoutChange(mAppContext, left, top, right, bottom);
            }
        }

        @Override
        public boolean onGenericMotion(final View view, final MotionEvent event) {
            if (mAppContext == 0) {
                return false;
            }
            return onWindowSurfaceMotionEvent(mAppContext, event);
        }

        @SuppressLint("ClickableViewAccessibility")
        @Override
        public boolean onTouch(final View view, final MotionEvent event) {
            if (mAppContext == 0) {
                return false;
            }
            return onWindowSurfaceMotionEvent(mAppContext, event);
        }

        @Override
        public void surfaceCreated(final SurfaceHolder holder) {
            if (mAppContext == 0) {
                return;
            }
            onWindowSurfaceChanged(mAppContext, holder.getSurface());
        }

        @Override
        public void surfaceChanged(
                final SurfaceHolder holder, final int format, final int width, final int height) {
            if (mAppContext == 0) {
                return;
            }
            onWindowSurfaceChanged(mAppContext, holder.getSurface());
        }

        @Override
        public void surfaceDestroyed(final SurfaceHolder holder) {
            if (mAppContext == 0) {
                return;
            }
            onWindowSurfaceChanged(mAppContext, null);
        }

        @Override
        public void surfaceRedrawNeeded(final SurfaceHolder holder) {
            onWindowSurfaceDraw(true);
        }
    }
}
