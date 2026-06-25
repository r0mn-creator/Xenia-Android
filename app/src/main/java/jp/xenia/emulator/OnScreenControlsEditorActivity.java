package jp.xenia.emulator;

import android.os.Bundle;
import android.view.Gravity;
import android.view.View;
import android.view.WindowManager;
import android.widget.FrameLayout;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;

/**
 * Full-screen editor for repositioning and rescaling the on-screen controller
 * overlay.  Black background; "Done" saves and exits, "Reset" restores defaults.
 * Landscape-only (matches the emulator orientation).
 */
public class OnScreenControlsEditorActivity extends AppCompatActivity {

    private OnScreenControlsView mControlsView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // Full-screen immersive so the layout fills the same area as the emulator.
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
        getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_FULLSCREEN
                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION);

        FrameLayout root = new FrameLayout(this);
        setContentView(root);

        // Controller overlay in edit mode.
        mControlsView = new OnScreenControlsView(this);
        mControlsView.setEditMode(true);
        root.addView(mControlsView, new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT));

        int pad = dp(16);

        // "Done" — top-right corner.
        TextView done = makeCornerButton("Done", 0xFFFFFFFF, 0x99000000);
        done.setOnClickListener(v -> { mControlsView.saveLayout(); finish(); });
        FrameLayout.LayoutParams lpDone = new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.WRAP_CONTENT,
                FrameLayout.LayoutParams.WRAP_CONTENT,
                Gravity.TOP | Gravity.END);
        lpDone.topMargin = pad;
        lpDone.rightMargin = pad;
        root.addView(done, lpDone);

        // "Reset" — top-left corner.
        TextView reset = makeCornerButton("Reset", 0xFFFF6E6E, 0x99000000);
        reset.setOnClickListener(v -> mControlsView.resetLayout());
        FrameLayout.LayoutParams lpReset = new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.WRAP_CONTENT,
                FrameLayout.LayoutParams.WRAP_CONTENT,
                Gravity.TOP | Gravity.START);
        lpReset.topMargin = pad;
        lpReset.leftMargin = pad;
        root.addView(reset, lpReset);
    }

    private TextView makeCornerButton(String text, int textColor, int bgColor) {
        TextView tv = new TextView(this);
        tv.setText(text);
        tv.setTextColor(textColor);
        tv.setTextSize(15f);
        tv.setBackgroundColor(bgColor);
        tv.setPadding(dp(18), dp(9), dp(18), dp(9));
        return tv;
    }

    @Override
    public void onBackPressed() {
        mControlsView.saveLayout();
        super.onBackPressed();
    }

    private int dp(int dp) {
        return Math.round(dp * getResources().getDisplayMetrics().density);
    }
}
