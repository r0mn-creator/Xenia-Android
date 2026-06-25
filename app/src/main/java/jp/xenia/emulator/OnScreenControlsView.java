package jp.xenia.emulator;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.SharedPreferences;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.RectF;
import android.os.Build;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.util.AttributeSet;
import android.util.SparseIntArray;
import android.view.MotionEvent;
import android.view.ScaleGestureDetector;
import android.view.View;

/**
 * Semi-transparent Xbox 360 controller overlay drawn on a Canvas.
 *
 * All button positions are defined in a 1280×720 design space.  At runtime
 * the layout is scaled uniformly to fit the view height (maintaining 16:9
 * aspect ratio) and centered.  The user-configurable pan (mCx, mCy) and
 * scale (mOscScale) are persisted in SharedPreferences.
 *
 * PLAY mode  — multi-touch drives InputListener callbacks.
 * EDIT mode  — single-finger pan + two-finger pinch resize the layout.
 */
public class OnScreenControlsView extends View {

    // ── Button bitmask constants (Xbox 360 layout) ────────────────────────────
    public static final int BTN_A           = 1;
    public static final int BTN_B           = 1 << 1;
    public static final int BTN_X           = 1 << 2;
    public static final int BTN_Y           = 1 << 3;
    public static final int BTN_LB          = 1 << 4;
    public static final int BTN_RB          = 1 << 5;
    public static final int BTN_LT          = 1 << 6;
    public static final int BTN_RT          = 1 << 7;
    public static final int BTN_START       = 1 << 8;
    public static final int BTN_BACK        = 1 << 9;
    public static final int BTN_GUIDE       = 1 << 10;
    public static final int BTN_DPAD_UP     = 1 << 11;
    public static final int BTN_DPAD_DOWN   = 1 << 12;
    public static final int BTN_DPAD_LEFT   = 1 << 13;
    public static final int BTN_DPAD_RIGHT  = 1 << 14;
    public static final int BTN_LS          = 1 << 15;
    public static final int BTN_RS          = 1 << 16;

    // ── SharedPreferences keys ────────────────────────────────────────────────
    static final String PREF_CX    = "osc_cx";
    static final String PREF_CY    = "osc_cy";
    static final String PREF_SCALE = "osc_scale";

    // ── Design-space button geometry (1280 × 720) ─────────────────────────────
    // Triggers (rectangles: left, top, right, bottom)
    private static final float LT_L = 22,  LT_T = 15,  LT_R = 182, LT_B = 78;
    private static final float RT_L = 1098,RT_T = 15,  RT_R = 1258,RT_B = 78;
    // Bumpers
    private static final float LB_CX = 115, LB_CY = 140, LB_R = 42;
    private static final float RB_CX = 1165,RB_CY = 140, RB_R = 42;
    // Left stick
    private static final float LS_CX = 185, LS_CY = 435, LS_R = 88;
    // D-pad
    private static final float DP_CX = 345, DP_CY = 578, DP_ARM = 54, DP_HALF = 27;
    // Face buttons
    private static final float A_CX = 1058, A_CY = 472, A_R = 50;
    private static final float B_CX = 1128, B_CY = 390, B_R = 50;
    private static final float X_CX =  988, X_CY = 390, X_R = 50;
    private static final float Y_CX = 1058, Y_CY = 308, Y_R = 50;
    // Right stick
    private static final float RS_CX = 875, RS_CY = 562, RS_R = 88;
    // Center cluster
    private static final float BACK_CX  = 510, BACK_CY  = 348, BACK_R  = 34;
    private static final float GUIDE_CX = 640, GUIDE_CY = 332, GUIDE_R = 42;
    private static final float START_CX = 770, START_CY = 348, START_R = 34;

    private static final float DESIGN_W = 1280f;
    private static final float DESIGN_H = 720f;

    // ── Colors ────────────────────────────────────────────────────────────────
    private static final int CLR_FACE    = 0xBB444444;
    private static final int CLR_PRESSED = 0xEE888888;
    private static final int CLR_A_N     = 0xBB2E7D32;
    private static final int CLR_A_P     = 0xFF4CAF50;
    private static final int CLR_B_N     = 0xBBC62828;
    private static final int CLR_B_P     = 0xFFF44336;
    private static final int CLR_X_N     = 0xBB1565C0;
    private static final int CLR_X_P     = 0xFF2196F3;
    private static final int CLR_Y_N     = 0xBBF57F17;
    private static final int CLR_Y_P     = 0xFFFFD740;
    private static final int CLR_GUIDE_N = 0xBB1B5E20;
    private static final int CLR_GUIDE_P = 0xFF388E3C;
    private static final int CLR_TEXT    = 0xFFFFFFFF;
    private static final int CLR_STICK   = 0xBB333333;
    private static final int CLR_THUMB   = 0xDDCCCCCC;

    // ── Input listener ────────────────────────────────────────────────────────
    public interface InputListener {
        void onInputChanged(int buttons, float leftX, float leftY,
                float rightX, float rightY, float leftTrigger, float rightTrigger);
    }

    private InputListener mInputListener;
    private int   mButtons = 0;
    private float mLeftX = 0f, mLeftY = 0f;
    private float mRightX = 0f, mRightY = 0f;

    // ── Pointer tracking ──────────────────────────────────────────────────────
    private static final int STICK_NONE  = 0;
    private static final int STICK_LEFT  = 1;
    private static final int STICK_RIGHT = 2;

    // pointer id → button bitmask it's pressing (0 = stick or nothing)
    private final SparseIntArray mPointerButtons  = new SparseIntArray();
    // pointer id → STICK_LEFT or STICK_RIGHT (0 = not a stick)
    private final SparseIntArray mPointerStick    = new SparseIntArray();
    // screen-space origin of each active stick pointer
    private float mLsOriginX, mLsOriginY;
    private float mRsOriginX, mRsOriginY;

    // ── Edit-mode state ───────────────────────────────────────────────────────
    private boolean mEditMode = false;
    private float mDragStartX, mDragStartY;
    private float mDragStartCx, mDragStartCy;
    private boolean mIsDragging = false;
    private ScaleGestureDetector mScaleDetector;

    // ── Layout prefs ──────────────────────────────────────────────────────────
    private float mCx = 0.5f;
    private float mCy = 0.5f;
    private float mOscScale = 1.0f;
    private SharedPreferences mPrefs;

    // ── Drawing helpers ───────────────────────────────────────────────────────
    private final Paint mPaint  = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final RectF mRectF  = new RectF();
    private Vibrator mVibrator;
    private boolean  mHapticsEnabled = true;

    // ──────────────────────────────────────────────────────────────────────────

    public OnScreenControlsView(Context context) {
        super(context);
        init(context);
    }

    public OnScreenControlsView(Context context, AttributeSet attrs) {
        super(context, attrs);
        init(context);
    }

    private void init(Context context) {
        mPrefs = context.getSharedPreferences(SettingsActivity.PREFS, Context.MODE_PRIVATE);
        mCx       = mPrefs.getFloat(PREF_CX, 0.5f);
        mCy       = mPrefs.getFloat(PREF_CY, 0.5f);
        mOscScale = mPrefs.getFloat(PREF_SCALE, 1.0f);
        mHapticsEnabled = mPrefs.getBoolean("haptics", true);
        mVibrator = (Vibrator) context.getSystemService(Context.VIBRATOR_SERVICE);

        mScaleDetector = new ScaleGestureDetector(context,
                new ScaleGestureDetector.SimpleOnScaleGestureListener() {
                    @Override
                    public boolean onScale(ScaleGestureDetector d) {
                        mOscScale = Math.max(0.3f, Math.min(2.5f, mOscScale * d.getScaleFactor()));
                        invalidate();
                        return true;
                    }
                });
    }

    public void setInputListener(InputListener l) { mInputListener = l; }

    public void setEditMode(boolean edit) {
        mEditMode = edit;
        clearAll();
        invalidate();
    }

    public void saveLayout() {
        mPrefs.edit()
                .putFloat(PREF_CX, mCx)
                .putFloat(PREF_CY, mCy)
                .putFloat(PREF_SCALE, mOscScale)
                .apply();
    }

    public void resetLayout() {
        mCx = 0.5f; mCy = 0.5f; mOscScale = 1.0f;
        invalidate();
    }

    // ── Coordinate mapping ────────────────────────────────────────────────────

    private float uniformScale() {
        int w = getWidth(), h = getHeight();
        if (w == 0 || h == 0) return 1f;
        return Math.min(w / DESIGN_W, h / DESIGN_H) * mOscScale;
    }

    private float sx(float dx) {
        float s = uniformScale();
        float baseX = (getWidth()  - DESIGN_W * s) / 2f;
        float panX  = (mCx - 0.5f) * getWidth();
        return dx * s + baseX + panX;
    }

    private float sy(float dy) {
        float s = uniformScale();
        float baseY = (getHeight() - DESIGN_H * s) / 2f;
        float panY  = (mCy - 0.5f) * getHeight();
        return dy * s + baseY + panY;
    }

    private float sr(float dr) { return dr * uniformScale(); }

    private float toDesignX(float vx) {
        float s = uniformScale();
        float baseX = (getWidth()  - DESIGN_W * s) / 2f;
        float panX  = (mCx - 0.5f) * getWidth();
        return (vx - baseX - panX) / s;
    }

    private float toDesignY(float vy) {
        float s = uniformScale();
        float baseY = (getHeight() - DESIGN_H * s) / 2f;
        float panY  = (mCy - 0.5f) * getHeight();
        return (vy - baseY - panY) / s;
    }

    // ── Drawing ───────────────────────────────────────────────────────────────

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);

        if (mEditMode) {
            canvas.drawColor(Color.BLACK);
            // Boundary hint
            mPaint.setStyle(Paint.Style.STROKE);
            mPaint.setColor(0x33FFFFFF);
            mPaint.setStrokeWidth(2f);
            mRectF.set(sx(0), sy(0), sx(DESIGN_W), sy(DESIGN_H));
            canvas.drawRect(mRectF, mPaint);
            // Hint text
            mPaint.setStyle(Paint.Style.FILL);
            mPaint.setColor(0xAAFFFFFF);
            mPaint.setTextSize(26f);
            mPaint.setTextAlign(Paint.Align.CENTER);
            canvas.drawText("Drag to move  •  Pinch to resize",
                    getWidth() / 2f, getHeight() - 24f, mPaint);
        }

        drawTrigger(canvas, LT_L, LT_T, LT_R, LT_B, (mButtons & BTN_LT) != 0, "LT");
        drawTrigger(canvas, RT_L, RT_T, RT_R, RT_B, (mButtons & BTN_RT) != 0, "RT");
        drawRoundBtn(canvas, LB_CX, LB_CY, LB_R, (mButtons & BTN_LB) != 0, "LB",
                CLR_FACE, CLR_PRESSED);
        drawRoundBtn(canvas, RB_CX, RB_CY, RB_R, (mButtons & BTN_RB) != 0, "RB",
                CLR_FACE, CLR_PRESSED);

        drawStick(canvas, LS_CX, LS_CY, LS_R, mLeftX,  mLeftY,  (mButtons & BTN_LS) != 0);
        drawDpad(canvas);
        drawStick(canvas, RS_CX, RS_CY, RS_R, mRightX, mRightY, (mButtons & BTN_RS) != 0);

        drawRoundBtn(canvas, A_CX, A_CY, A_R, (mButtons & BTN_A) != 0, "A", CLR_A_N, CLR_A_P);
        drawRoundBtn(canvas, B_CX, B_CY, B_R, (mButtons & BTN_B) != 0, "B", CLR_B_N, CLR_B_P);
        drawRoundBtn(canvas, X_CX, X_CY, X_R, (mButtons & BTN_X) != 0, "X", CLR_X_N, CLR_X_P);
        drawRoundBtn(canvas, Y_CX, Y_CY, Y_R, (mButtons & BTN_Y) != 0, "Y", CLR_Y_N, CLR_Y_P);

        drawPillBtn(canvas, BACK_CX,  BACK_CY,  BACK_R,  (mButtons & BTN_BACK)  != 0, "◄►");
        drawGuide(canvas);
        drawPillBtn(canvas, START_CX, START_CY, START_R, (mButtons & BTN_START) != 0, "☰");
    }

    private void drawTrigger(Canvas c, float dl, float dt, float dr, float db,
            boolean pressed, String label) {
        float l = sx(dl), t = sy(dt), r = sx(dr), b = sy(db);
        mRectF.set(l, t, r, b);
        mPaint.setStyle(Paint.Style.FILL);
        mPaint.setColor(pressed ? CLR_PRESSED : CLR_FACE);
        c.drawRoundRect(mRectF, sr(8), sr(8), mPaint);
        mPaint.setStyle(Paint.Style.STROKE);
        mPaint.setColor(0x66FFFFFF);
        mPaint.setStrokeWidth(sr(2));
        c.drawRoundRect(mRectF, sr(8), sr(8), mPaint);
        mPaint.setStyle(Paint.Style.FILL);
        mPaint.setColor(CLR_TEXT);
        mPaint.setTextSize(sr(20));
        mPaint.setTextAlign(Paint.Align.CENTER);
        c.drawText(label, (l + r) / 2f, (t + b) / 2f + sr(7), mPaint);
    }

    private void drawRoundBtn(Canvas c, float dcx, float dcy, float dr,
            boolean pressed, String label, int normalClr, int pressClr) {
        float cx = sx(dcx), cy = sy(dcy), r = sr(dr);
        mPaint.setStyle(Paint.Style.FILL);
        mPaint.setColor(pressed ? pressClr : normalClr);
        c.drawCircle(cx, cy, r, mPaint);
        mPaint.setStyle(Paint.Style.STROKE);
        mPaint.setColor(0x66FFFFFF);
        mPaint.setStrokeWidth(sr(2));
        c.drawCircle(cx, cy, r, mPaint);
        mPaint.setStyle(Paint.Style.FILL);
        mPaint.setColor(CLR_TEXT);
        mPaint.setTextSize(sr(24));
        mPaint.setTextAlign(Paint.Align.CENTER);
        mPaint.setFakeBoldText(true);
        c.drawText(label, cx, cy + sr(9), mPaint);
        mPaint.setFakeBoldText(false);
    }

    private void drawPillBtn(Canvas c, float dcx, float dcy, float dr,
            boolean pressed, String label) {
        float cx = sx(dcx), cy = sy(dcy);
        float rw = sr(dr) * 1.7f, rh = sr(dr) * 0.85f;
        mRectF.set(cx - rw, cy - rh, cx + rw, cy + rh);
        mPaint.setStyle(Paint.Style.FILL);
        mPaint.setColor(pressed ? CLR_PRESSED : CLR_FACE);
        c.drawRoundRect(mRectF, rh, rh, mPaint);
        mPaint.setStyle(Paint.Style.STROKE);
        mPaint.setColor(0x66FFFFFF);
        mPaint.setStrokeWidth(sr(2));
        c.drawRoundRect(mRectF, rh, rh, mPaint);
        mPaint.setStyle(Paint.Style.FILL);
        mPaint.setColor(CLR_TEXT);
        mPaint.setTextSize(sr(15));
        mPaint.setTextAlign(Paint.Align.CENTER);
        c.drawText(label, cx, cy + sr(5), mPaint);
    }

    private void drawGuide(Canvas c) {
        float cx = sx(GUIDE_CX), cy = sy(GUIDE_CY), r = sr(GUIDE_R);
        boolean pressed = (mButtons & BTN_GUIDE) != 0;
        mPaint.setStyle(Paint.Style.FILL);
        mPaint.setColor(pressed ? CLR_GUIDE_P : CLR_GUIDE_N);
        c.drawCircle(cx, cy, r, mPaint);
        mPaint.setStyle(Paint.Style.STROKE);
        mPaint.setColor(0x88FFFFFF);
        mPaint.setStrokeWidth(sr(2));
        c.drawCircle(cx, cy, r, mPaint);
        // Xbox X logo
        mPaint.setStyle(Paint.Style.FILL);
        mPaint.setColor(CLR_TEXT);
        mPaint.setTextSize(sr(26));
        mPaint.setTextAlign(Paint.Align.CENTER);
        mPaint.setFakeBoldText(true);
        c.drawText("Ⓧ", cx, cy + sr(10), mPaint);  // circled X
        mPaint.setFakeBoldText(false);
    }

    private void drawStick(Canvas c, float dcx, float dcy, float dr,
            float axisX, float axisY, boolean clicked) {
        float cx = sx(dcx), cy = sy(dcy), r = sr(dr), innerR = sr(dr * 0.40f);
        mPaint.setStyle(Paint.Style.FILL);
        mPaint.setColor(CLR_STICK);
        c.drawCircle(cx, cy, r, mPaint);
        mPaint.setStyle(Paint.Style.STROKE);
        mPaint.setColor(0x55FFFFFF);
        mPaint.setStrokeWidth(sr(2));
        c.drawCircle(cx, cy, r, mPaint);
        float dotX = cx + axisX * (r - innerR);
        float dotY = cy + axisY * (r - innerR);
        mPaint.setStyle(Paint.Style.FILL);
        mPaint.setColor(clicked ? 0xFFFFFFFF : CLR_THUMB);
        c.drawCircle(dotX, dotY, innerR, mPaint);
    }

    private void drawDpad(Canvas c) {
        float cx = sx(DP_CX), cy = sy(DP_CY);
        float arm = sr(DP_ARM), half = sr(DP_HALF);
        boolean up    = (mButtons & BTN_DPAD_UP)    != 0;
        boolean down  = (mButtons & BTN_DPAD_DOWN)  != 0;
        boolean left  = (mButtons & BTN_DPAD_LEFT)  != 0;
        boolean right = (mButtons & BTN_DPAD_RIGHT) != 0;

        // Draw each arm as a rounded-cap rectangle
        mPaint.setStyle(Paint.Style.FILL);
        float rad = sr(6);
        mRectF.set(cx - half, cy - arm - half, cx + half, cy + half);
        mPaint.setColor(up ? CLR_PRESSED : CLR_FACE);
        c.drawRoundRect(mRectF, rad, rad, mPaint);

        mRectF.set(cx - half, cy - half, cx + half, cy + arm + half);
        mPaint.setColor(down ? CLR_PRESSED : CLR_FACE);
        c.drawRoundRect(mRectF, rad, rad, mPaint);

        mRectF.set(cx - arm - half, cy - half, cx + half, cy + half);
        mPaint.setColor(left ? CLR_PRESSED : CLR_FACE);
        c.drawRoundRect(mRectF, rad, rad, mPaint);

        mRectF.set(cx - half, cy - half, cx + arm + half, cy + half);
        mPaint.setColor(right ? CLR_PRESSED : CLR_FACE);
        c.drawRoundRect(mRectF, rad, rad, mPaint);

        // Outlines
        mPaint.setStyle(Paint.Style.STROKE);
        mPaint.setColor(0x66FFFFFF);
        mPaint.setStrokeWidth(sr(2));
        // Top and bottom arm outlines
        mRectF.set(cx - half, cy - arm - half, cx + half, cy - half);
        c.drawRoundRect(mRectF, rad, rad, mPaint);
        mRectF.set(cx - half, cy + half, cx + half, cy + arm + half);
        c.drawRoundRect(mRectF, rad, rad, mPaint);
        // Left and right arm outlines
        mRectF.set(cx - arm - half, cy - half, cx - half, cy + half);
        c.drawRoundRect(mRectF, rad, rad, mPaint);
        mRectF.set(cx + half, cy - half, cx + arm + half, cy + half);
        c.drawRoundRect(mRectF, rad, rad, mPaint);
    }

    // ── Touch handling ────────────────────────────────────────────────────────

    @Override
    @SuppressLint("ClickableViewAccessibility")
    public boolean onTouchEvent(MotionEvent event) {
        if (mEditMode) {
            mScaleDetector.onTouchEvent(event);
            return handleEditDrag(event);
        }
        return handlePlayTouch(event);
    }

    private boolean handleEditDrag(MotionEvent event) {
        if (mScaleDetector.isInProgress() || event.getPointerCount() > 1) {
            mIsDragging = false;
            return true;
        }
        switch (event.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
                mDragStartX  = event.getX();
                mDragStartY  = event.getY();
                mDragStartCx = mCx;
                mDragStartCy = mCy;
                mIsDragging  = true;
                break;
            case MotionEvent.ACTION_MOVE:
                if (mIsDragging) {
                    float dx = (event.getX() - mDragStartX) / getWidth();
                    float dy = (event.getY() - mDragStartY) / getHeight();
                    mCx = Math.max(0f, Math.min(1f, mDragStartCx + dx));
                    mCy = Math.max(0f, Math.min(1f, mDragStartCy + dy));
                    invalidate();
                }
                break;
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL:
                mIsDragging = false;
                break;
        }
        return true;
    }

    private boolean handlePlayTouch(MotionEvent event) {
        final int action = event.getActionMasked();
        final int pIdx   = event.getActionIndex();
        final int pId    = event.getPointerId(pIdx);

        switch (action) {
            case MotionEvent.ACTION_DOWN:
            case MotionEvent.ACTION_POINTER_DOWN: {
                float vx = event.getX(pIdx), vy = event.getY(pIdx);
                pointerDown(pId, toDesignX(vx), toDesignY(vy), vx, vy);
                break;
            }
            case MotionEvent.ACTION_MOVE: {
                for (int i = 0; i < event.getPointerCount(); i++) {
                    int id = event.getPointerId(i);
                    int stick = mPointerStick.get(id, STICK_NONE);
                    if (stick != STICK_NONE) {
                        updateStick(stick, event.getX(i), event.getY(i));
                    }
                }
                break;
            }
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_POINTER_UP:
                pointerUp(pId);
                break;
            case MotionEvent.ACTION_CANCEL:
                clearAll();
                break;
        }
        notifyListener();
        invalidate();
        return true;
    }

    private void pointerDown(int pId, float dx, float dy, float vx, float vy) {
        // Sticks get priority (larger effective hit area).
        if (hitCircle(dx, dy, LS_CX, LS_CY, LS_R + 12)) {
            mPointerStick.put(pId, STICK_LEFT);
            mLsOriginX = vx; mLsOriginY = vy;
            hapticTick();
            return;
        }
        if (hitCircle(dx, dy, RS_CX, RS_CY, RS_R + 12)) {
            mPointerStick.put(pId, STICK_RIGHT);
            mRsOriginX = vx; mRsOriginY = vy;
            hapticTick();
            return;
        }
        int btns = hitButton(dx, dy);
        if (btns != 0) {
            mPointerButtons.put(pId, btns);
            mButtons |= btns;
            hapticTick();
        }
    }

    private void pointerUp(int pId) {
        int stick = mPointerStick.get(pId, STICK_NONE);
        if (stick == STICK_LEFT) {
            mLeftX = 0; mLeftY = 0;
            mPointerStick.delete(pId);
        } else if (stick == STICK_RIGHT) {
            mRightX = 0; mRightY = 0;
            mPointerStick.delete(pId);
        } else {
            int btns = mPointerButtons.get(pId, 0);
            mButtons &= ~btns;
            mPointerButtons.delete(pId);
        }
    }

    private void updateStick(int which, float vx, float vy) {
        float maxR = sr(LS_R) * 0.82f;  // LS_R == RS_R
        float odx, ody;
        if (which == STICK_LEFT) {
            odx = vx - mLsOriginX; ody = vy - mLsOriginY;
        } else {
            odx = vx - mRsOriginX; ody = vy - mRsOriginY;
        }
        float len = (float) Math.sqrt(odx * odx + ody * ody);
        if (len > maxR) { odx = odx / len * maxR; ody = ody / len * maxR; }
        float ax = odx / maxR, ay = ody / maxR;
        if (which == STICK_LEFT) { mLeftX  = ax; mLeftY  = ay; }
        else                     { mRightX = ax; mRightY = ay; }
    }

    private int hitButton(float dx, float dy) {
        if (hitRect(dx, dy, LT_L, LT_T, LT_R, LT_B))   return BTN_LT;
        if (hitRect(dx, dy, RT_L, RT_T, RT_R, RT_B))   return BTN_RT;
        if (hitCircle(dx, dy, LB_CX, LB_CY, LB_R))     return BTN_LB;
        if (hitCircle(dx, dy, RB_CX, RB_CY, RB_R))     return BTN_RB;
        if (hitCircle(dx, dy, A_CX, A_CY, A_R))         return BTN_A;
        if (hitCircle(dx, dy, B_CX, B_CY, B_R))         return BTN_B;
        if (hitCircle(dx, dy, X_CX, X_CY, X_R))         return BTN_X;
        if (hitCircle(dx, dy, Y_CX, Y_CY, Y_R))         return BTN_Y;
        if (hitCircle(dx, dy, BACK_CX,  BACK_CY,  BACK_R  + 12)) return BTN_BACK;
        if (hitCircle(dx, dy, START_CX, START_CY, START_R + 12)) return BTN_START;
        if (hitCircle(dx, dy, GUIDE_CX, GUIDE_CY, GUIDE_R))      return BTN_GUIDE;
        return dpadHit(dx, dy);
    }

    private int dpadHit(float dx, float dy) {
        float rx = dx - DP_CX, ry = dy - DP_CY;
        boolean inVert = Math.abs(rx) <= DP_HALF && Math.abs(ry) <= DP_ARM + DP_HALF;
        boolean inHorz = Math.abs(ry) <= DP_HALF && Math.abs(rx) <= DP_ARM + DP_HALF;
        if (!inVert && !inHorz) return 0;
        // Skip the dead centre
        if (Math.abs(rx) < DP_HALF * 0.4f && Math.abs(ry) < DP_HALF * 0.4f) return 0;
        if (Math.abs(ry) >= Math.abs(rx))
            return ry < 0 ? BTN_DPAD_UP : BTN_DPAD_DOWN;
        else
            return rx < 0 ? BTN_DPAD_LEFT : BTN_DPAD_RIGHT;
    }

    private static boolean hitCircle(float dx, float dy, float cx, float cy, float r) {
        float ox = dx - cx, oy = dy - cy;
        return ox * ox + oy * oy <= r * r;
    }

    private static boolean hitRect(float dx, float dy, float l, float t, float r, float b) {
        return dx >= l && dx <= r && dy >= t && dy <= b;
    }

    private void clearAll() {
        mButtons = 0;
        mLeftX = mLeftY = mRightX = mRightY = 0;
        mPointerButtons.clear();
        mPointerStick.clear();
    }

    private void notifyListener() {
        if (mInputListener == null) return;
        float lt = (mButtons & BTN_LT) != 0 ? 1f : 0f;
        float rt = (mButtons & BTN_RT) != 0 ? 1f : 0f;
        mInputListener.onInputChanged(mButtons,
                mLeftX, mLeftY, mRightX, mRightY, lt, rt);
    }

    @SuppressWarnings("deprecation")
    private void hapticTick() {
        if (!mHapticsEnabled || mVibrator == null || !mVibrator.hasVibrator()) return;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            mVibrator.vibrate(VibrationEffect.createOneShot(
                    18, VibrationEffect.DEFAULT_AMPLITUDE));
        } else {
            mVibrator.vibrate(18);
        }
    }
}
