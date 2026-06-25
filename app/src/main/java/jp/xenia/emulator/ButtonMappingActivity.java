package jp.xenia.emulator;

import android.content.SharedPreferences;
import android.os.Bundle;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.google.android.material.appbar.MaterialToolbar;
import com.google.android.material.dialog.MaterialAlertDialogBuilder;

/**
 * Lets the user remap physical controller buttons to Xbox 360 buttons.
 * Tap a row → press any button on the connected controller → mapping saved.
 * Useful when Android hasn't auto-mapped a non-standard gamepad.
 */
public class ButtonMappingActivity extends AppCompatActivity {

    // [display label, prefs key]
    private static final String[][] XBOX_BTNS = {
        {"A",                    "btn_map_a"},
        {"B",                    "btn_map_b"},
        {"X",                    "btn_map_x"},
        {"Y",                    "btn_map_y"},
        {"Left Bumper (LB)",     "btn_map_lb"},
        {"Right Bumper (RB)",    "btn_map_rb"},
        {"Left Trigger (LT)",    "btn_map_lt"},
        {"Right Trigger (RT)",   "btn_map_rt"},
        {"Start",                "btn_map_start"},
        {"Back / Select",        "btn_map_back"},
        {"Guide (Xbox button)",  "btn_map_guide"},
        {"D-Pad Up",             "btn_map_dup"},
        {"D-Pad Down",           "btn_map_ddown"},
        {"D-Pad Left",           "btn_map_dleft"},
        {"D-Pad Right",          "btn_map_dright"},
        {"Left Stick Click",     "btn_map_ls"},
        {"Right Stick Click",    "btn_map_rs"},
    };

    private SharedPreferences mPrefs;
    private MappingAdapter    mAdapter;
    private int               mCapturingIndex = -1;
    private AlertDialog       mCaptureDialog;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_button_mapping);

        mPrefs = getSharedPreferences(SettingsActivity.PREFS, MODE_PRIVATE);

        MaterialToolbar toolbar = findViewById(R.id.toolbar_button_mapping);
        setSupportActionBar(toolbar);
        if (getSupportActionBar() != null) getSupportActionBar().setDisplayHomeAsUpEnabled(true);
        toolbar.setNavigationOnClickListener(v -> finish());

        RecyclerView list = findViewById(R.id.list_button_mapping);
        list.setLayoutManager(new LinearLayoutManager(this));
        mAdapter = new MappingAdapter();
        list.setAdapter(mAdapter);
    }

    // Captures the next physical key press when a mapping row was tapped.
    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (mCapturingIndex < 0) return super.onKeyDown(keyCode, event);
        if (keyCode == KeyEvent.KEYCODE_BACK) {
            cancelCapture();
            return true;
        }
        // Ignore modifier and volume keys.
        if (keyCode == KeyEvent.KEYCODE_VOLUME_UP
                || keyCode == KeyEvent.KEYCODE_VOLUME_DOWN
                || keyCode == KeyEvent.KEYCODE_META_LEFT
                || keyCode == KeyEvent.KEYCODE_META_RIGHT
                || keyCode == KeyEvent.KEYCODE_SHIFT_LEFT
                || keyCode == KeyEvent.KEYCODE_SHIFT_RIGHT) {
            return true;
        }
        String keyName = KeyEvent.keyCodeToString(keyCode);
        mPrefs.edit().putString(XBOX_BTNS[mCapturingIndex][1], keyName).apply();
        if (mCaptureDialog != null) mCaptureDialog.dismiss();
        mCapturingIndex = -1;
        mAdapter.notifyDataSetChanged();
        return true;
    }

    private void startCapture(int index) {
        mCapturingIndex = index;
        mCaptureDialog = new MaterialAlertDialogBuilder(this)
                .setTitle("Map \"" + XBOX_BTNS[index][0] + "\"")
                .setMessage("Press any button on your controller.\nPress Back to cancel.")
                .setNegativeButton("Cancel", (d, w) -> cancelCapture())
                .setCancelable(false)
                .create();
        mCaptureDialog.show();
    }

    private void cancelCapture() {
        mCapturingIndex = -1;
        if (mCaptureDialog != null) { mCaptureDialog.dismiss(); mCaptureDialog = null; }
    }

    private String mappingLabel(String prefKey) {
        String saved = mPrefs.getString(prefKey, null);
        if (saved == null || saved.isEmpty()) return "Default";
        // "KEYCODE_BUTTON_A" → "Button A"
        return saved.replaceFirst("^KEYCODE_", "").replace('_', ' ').toLowerCase();
    }

    // ── Adapter ───────────────────────────────────────────────────────────────

    private class MappingAdapter extends RecyclerView.Adapter<MappingAdapter.VH> {

        @NonNull
        @Override
        public VH onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
            View v = LayoutInflater.from(parent.getContext())
                    .inflate(R.layout.item_button_mapping, parent, false);
            return new VH(v);
        }

        @Override
        public void onBindViewHolder(@NonNull VH holder, int position) {
            holder.name.setText(XBOX_BTNS[position][0]);
            holder.mapping.setText(mappingLabel(XBOX_BTNS[position][1]));
            holder.itemView.setOnClickListener(v -> startCapture(position));
        }

        @Override
        public int getItemCount() { return XBOX_BTNS.length; }

        class VH extends RecyclerView.ViewHolder {
            final TextView name, mapping;
            VH(View v) {
                super(v);
                name    = v.findViewById(R.id.text_btn_name);
                mapping = v.findViewById(R.id.text_btn_mapping);
            }
        }
    }
}
