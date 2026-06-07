package jp.xenia.emulator;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.provider.DocumentsContract;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.SwitchCompat;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.google.android.material.appbar.MaterialToolbar;
import com.google.android.material.dialog.MaterialAlertDialogBuilder;
import com.google.android.material.snackbar.Snackbar;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

public class SettingsActivity extends AppCompatActivity {

    static final String PREFS = "xenia_prefs";
    private static final String TAG = "SettingsActivity";

    // Resolution options: label, description, pref value
    static final String[][] RESOLUTIONS = {
        {"360p",  "640×360",  "360p"},
        {"480p",  "854×480",  "480p"},
        {"720p",  "1280×720", "720p"},
        {"1080p", "1920×1080","1080p"},
        {"2K",    "2560×1440","2k"},
    };
    static final int RESOLUTION_DEFAULT = 2; // 720p

    @Override
    protected void onCreate(final Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_settings);

        final MaterialToolbar toolbar = findViewById(R.id.toolbar_settings);
        setSupportActionBar(toolbar);
        toolbar.setNavigationOnClickListener(v -> finish());

        final RecyclerView list = findViewById(R.id.list_settings);
        list.setLayoutManager(new LinearLayoutManager(this));
        list.setAdapter(new SettingsAdapter(this, buildSettings()));
    }

    private List<SettingItem> buildSettings() {
        final SharedPreferences prefs = getSharedPreferences(PREFS, MODE_PRIVATE);
        final List<SettingItem> items = new ArrayList<>();

        // Appearance
        items.add(new SettingItem("Appearance", null, SettingItem.TYPE_HEADER));
        items.add(new SettingItem("Dark Mode",
                "Use dark grey background instead of white",
                SettingItem.TYPE_TOGGLE, "dark_mode", false, prefs));

        // General
        items.add(new SettingItem("General", null, SettingItem.TYPE_HEADER));
        items.add(new SettingItem("Fullscreen",
                "Use fullscreen mode when launching games",
                SettingItem.TYPE_TOGGLE, "fullscreen", false, prefs));
        items.add(new SettingItem("Discord RPC",
                "Show currently played game on Discord",
                SettingItem.TYPE_TOGGLE, "discord_rpc", false, prefs));

        // Graphics
        items.add(new SettingItem("Graphics", null, SettingItem.TYPE_HEADER));
        items.add(new SettingItem("GPU Backend",
                "Vulkan", SettingItem.TYPE_VALUE, "gpu_backend", false, prefs));
        items.add(new SettingItem("Resolution",
                currentResolutionLabel(prefs),
                SettingItem.TYPE_RESOLUTION, "resolution", false, prefs));
        items.add(new SettingItem("VSync",
                "Sync framerate to display refresh rate",
                SettingItem.TYPE_TOGGLE, "vsync", true, prefs));
        items.add(new SettingItem("Gamma",
                "2.2", SettingItem.TYPE_VALUE, "gamma", false, prefs));

        // Audio
        items.add(new SettingItem("Audio", null, SettingItem.TYPE_HEADER));
        items.add(new SettingItem("Audio Backend",
                "AAudio", SettingItem.TYPE_VALUE, "audio_backend", false, prefs));
        items.add(new SettingItem("Mute Audio",
                null, SettingItem.TYPE_TOGGLE, "mute", false, prefs));

        // Input
        items.add(new SettingItem("Input", null, SettingItem.TYPE_HEADER));
        items.add(new SettingItem("Controller Overlay",
                "Show on-screen controller",
                SettingItem.TYPE_TOGGLE, "controller_overlay", true, prefs));
        items.add(new SettingItem("Haptic Feedback",
                "Vibrate on button press",
                SettingItem.TYPE_TOGGLE, "haptics", true, prefs));
        items.add(new SettingItem("Edit On-Screen Controls",
                "Move and resize the controller overlay",
                () -> startActivity(new Intent(this, OnScreenControlsEditorActivity.class))));
        items.add(new SettingItem("Button Mapping",
                "Remap physical controller buttons to Xbox 360 layout",
                () -> startActivity(new Intent(this, ButtonMappingActivity.class))));

        // Box Art
        items.add(new SettingItem("Box Art", null, SettingItem.TYPE_HEADER));
        items.add(new SettingItem("Auto-scrape box art",
                "Fetch from embedded game data and Libretro database automatically",
                SettingItem.TYPE_TOGGLE, "boxart_auto", true, prefs));
        items.add(new SettingItem("Libretro thumbnails",
                "Free — no account needed",
                SettingItem.TYPE_TOGGLE, "boxart_libretro", true, prefs));
        items.add(new SettingItem("TheGamesDB API Key",
                "Optional — enhances results. Get a free key at thegamesdb.net",
                SettingItem.TYPE_VALUE, "thegamesdb_api_key", false, prefs));

        // Library
        items.add(new SettingItem("Library", null, SettingItem.TYPE_HEADER));
        items.add(new SettingItem("Refresh Library",
                "Scan your added folders for new game files",
                this::refreshLibrary));

        // Advanced
        items.add(new SettingItem("Advanced", null, SettingItem.TYPE_HEADER));
        items.add(new SettingItem("GPU Trace Viewer",
                "Open trace capture tool",
                SettingItem.TYPE_VALUE, null, false, prefs));
        items.add(new SettingItem("Log Level",
                "Warning", SettingItem.TYPE_VALUE, "log_level", false, prefs));

        return items;
    }

    // -------------------------------------------------------------------------
    // Library refresh
    // -------------------------------------------------------------------------

    private void refreshLibrary() {
        final SharedPreferences prefs = getSharedPreferences(PREFS, MODE_PRIVATE);
        final Set<String> folders = new HashSet<>(prefs.getStringSet(
                LauncherActivity.PREF_WATCHED_FOLDERS, Collections.emptySet()));

        if (folders.isEmpty()) {
            Snackbar.make(findViewById(android.R.id.content),
                    "No watched folders — add games via \"Select a folder\" first",
                    Snackbar.LENGTH_LONG).show();
            return;
        }

        Snackbar.make(findViewById(android.R.id.content),
                "Refreshing library…", Snackbar.LENGTH_SHORT).show();

        new Thread(() -> {
            // Snapshot of already-known URIs to skip duplicates.
            final Set<String> existing = new HashSet<>();
            for (LauncherActivity.GameEntry g : LauncherActivity.sGames) {
                existing.add(g.uri);
            }

            int found = 0;
            for (String uriStr : folders) {
                try {
                    final Uri treeUri = Uri.parse(uriStr);
                    final Uri childrenUri = DocumentsContract.buildChildDocumentsUriUsingTree(
                            treeUri, DocumentsContract.getTreeDocumentId(treeUri));
                    try (Cursor c = getContentResolver().query(childrenUri,
                            new String[]{
                                    DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                                    DocumentsContract.Document.COLUMN_DISPLAY_NAME
                            }, null, null, null)) {
                        while (c != null && c.moveToNext()) {
                            final String docId = c.getString(0);
                            final String name  = c.getString(1);
                            if (!isGameFileName(name)) continue;
                            final Uri fileUri =
                                    DocumentsContract.buildDocumentUriUsingTree(treeUri, docId);
                            if (existing.add(fileUri.toString())) {
                                runOnUiThread(() -> addRefreshedGame(fileUri, name));
                                found++;
                            }
                        }
                    }
                } catch (Exception e) {
                    Log.w(TAG, "Refresh failed for " + uriStr + ": " + e.getMessage());
                }
            }

            final String msg = found == 0
                    ? "Library is up to date"
                    : "Added " + found + " new game" + (found == 1 ? "" : "s");
            runOnUiThread(() -> Snackbar.make(
                    findViewById(android.R.id.content), msg, Snackbar.LENGTH_LONG).show());
        }).start();
    }

    private void addRefreshedGame(Uri uri, String displayName) {
        try {
            getContentResolver().takePersistableUriPermission(
                    uri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
        } catch (SecurityException ignored) {}
        final String title = displayName.replaceFirst("(?i)\\.(iso|xex|zar|xbla)$", "");
        final LauncherActivity.GameEntry entry =
                new LauncherActivity.GameEntry(title, uri.toString(), "Xbox 360");
        LauncherActivity.sGames.add(entry);
        // onResume() in LauncherActivity refreshes the grid when the user navigates back.
        // Pass empty onComplete — art will be visible once the user returns.
        GameScanner.scan(this, entry, () -> {});
    }

    private static boolean isGameFileName(String name) {
        if (name == null) return false;
        final String lower = name.toLowerCase();
        return lower.endsWith(".iso") || lower.endsWith(".xex")
                || lower.endsWith(".zar") || lower.endsWith(".xbla");
    }

    static String currentResolutionLabel(SharedPreferences prefs) {
        final String saved = prefs.getString("resolution", RESOLUTIONS[RESOLUTION_DEFAULT][2]);
        for (String[] r : RESOLUTIONS) {
            if (r[2].equals(saved)) return r[0] + " (" + r[1] + ")";
        }
        return RESOLUTIONS[RESOLUTION_DEFAULT][0] + " (" + RESOLUTIONS[RESOLUTION_DEFAULT][1] + ")";
    }

    // -------------------------------------------------------------------------

    static class SettingItem {
        static final int TYPE_HEADER     = 0;
        static final int TYPE_TOGGLE     = 1;
        static final int TYPE_VALUE      = 2;
        static final int TYPE_RESOLUTION = 3;
        static final int TYPE_ACTION     = 4;

        final String name;
        String description;
        final int type;
        final String prefKey;
        boolean enabled;
        final Runnable action;

        SettingItem(String name, String description, int type,
                    String prefKey, boolean defaultOn, SharedPreferences prefs) {
            this.name = name;
            this.description = description;
            this.type = type;
            this.prefKey = prefKey;
            this.action = null;
            this.enabled = (type == TYPE_TOGGLE && prefKey != null)
                    ? prefs.getBoolean(prefKey, defaultOn) : defaultOn;
        }

        // Header constructor
        SettingItem(String name, String description, int type) {
            this(name, description, type, null, false, null);
        }

        // Action button constructor
        SettingItem(String name, String description, Runnable action) {
            this.name = name;
            this.description = description;
            this.type = TYPE_ACTION;
            this.prefKey = null;
            this.action = action;
            this.enabled = false;
        }
    }

    // -------------------------------------------------------------------------

    private static class SettingsAdapter extends RecyclerView.Adapter<RecyclerView.ViewHolder> {

        private final Context mContext;
        private final List<SettingItem> mItems;
        private final SharedPreferences mPrefs;

        SettingsAdapter(Context ctx, List<SettingItem> items) {
            mContext = ctx;
            mItems = items;
            mPrefs = ctx.getSharedPreferences(PREFS, MODE_PRIVATE);
        }

        @Override
        public int getItemViewType(int pos) {
            final int t = mItems.get(pos).type;
            // Both RESOLUTION and ACTION reuse the TYPE_VALUE view holder.
            return (t == SettingItem.TYPE_RESOLUTION || t == SettingItem.TYPE_ACTION)
                    ? SettingItem.TYPE_VALUE : t;
        }

        @NonNull
        @Override
        public RecyclerView.ViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
            final LayoutInflater inf = LayoutInflater.from(parent.getContext());
            if (viewType == SettingItem.TYPE_HEADER)
                return new HeaderVH(inf.inflate(R.layout.item_setting_header, parent, false));
            return new SettingVH(inf.inflate(R.layout.item_setting, parent, false));
        }

        @Override
        public void onBindViewHolder(@NonNull RecyclerView.ViewHolder holder, int pos) {
            final SettingItem item = mItems.get(pos);

            if (holder instanceof HeaderVH) {
                ((TextView) holder.itemView).setText(item.name);
                return;
            }

            final SettingVH vh = (SettingVH) holder;
            vh.name.setText(item.name);

            if (item.type == SettingItem.TYPE_TOGGLE) {
                vh.toggle.setVisibility(View.VISIBLE);
                vh.value.setVisibility(View.GONE);
                if (item.description != null) {
                    vh.description.setText(item.description);
                    vh.description.setVisibility(View.VISIBLE);
                }
                vh.toggle.setChecked(item.enabled);
                vh.toggle.setOnCheckedChangeListener(null);
                vh.toggle.setOnCheckedChangeListener((v, checked) -> {
                    item.enabled = checked;
                    if (item.prefKey != null) {
                        mPrefs.edit().putBoolean(item.prefKey, checked).apply();
                        if ("dark_mode".equals(item.prefKey)) {
                            XeniaApp.applyDarkMode(mPrefs);
                            ((Activity) mContext).recreate();
                        }
                    }
                });
                holder.itemView.setOnClickListener(v -> vh.toggle.toggle());

            } else if (item.type == SettingItem.TYPE_RESOLUTION) {
                vh.toggle.setVisibility(View.GONE);
                vh.value.setVisibility(View.VISIBLE);
                vh.description.setVisibility(View.GONE);
                vh.value.setText(item.description);
                holder.itemView.setOnClickListener(v -> showResolutionPicker(pos, item, vh.value));

            } else if (item.type == SettingItem.TYPE_ACTION) {
                vh.toggle.setVisibility(View.GONE);
                vh.value.setVisibility(View.GONE);
                vh.description.setVisibility(View.VISIBLE);
                vh.description.setText(item.description);
                holder.itemView.setOnClickListener(v -> {
                    if (item.action != null) item.action.run();
                });

            } else {
                vh.toggle.setVisibility(View.GONE);
                vh.value.setVisibility(View.VISIBLE);
                vh.description.setVisibility(View.GONE);
                vh.value.setText(item.description);
                holder.itemView.setOnClickListener(null);
            }
        }

        private void showResolutionPicker(int adapterPos, SettingItem item, TextView valueView) {
            final String saved = mPrefs.getString("resolution",
                    RESOLUTIONS[RESOLUTION_DEFAULT][2]);

            // Find current selection index
            int current = RESOLUTION_DEFAULT;
            for (int i = 0; i < RESOLUTIONS.length; i++) {
                if (RESOLUTIONS[i][2].equals(saved)) { current = i; break; }
            }

            final String[] labels = new String[RESOLUTIONS.length];
            for (int i = 0; i < RESOLUTIONS.length; i++) labels[i] = RESOLUTIONS[i][0];

            new MaterialAlertDialogBuilder(mContext)
                    .setTitle("Resolution")
                    .setSingleChoiceItems(labels, current, (dialog, which) -> {
                        mPrefs.edit().putString("resolution", RESOLUTIONS[which][2]).apply();
                        final String newLabel = RESOLUTIONS[which][0] + " (" + RESOLUTIONS[which][1] + ")";
                        item.description = newLabel;
                        valueView.setText(newLabel);
                        dialog.dismiss();
                    })
                    .setNegativeButton("Cancel", null)
                    .show();
        }

        @Override
        public int getItemCount() { return mItems.size(); }

        static class HeaderVH extends RecyclerView.ViewHolder {
            HeaderVH(View v) { super(v); }
        }

        static class SettingVH extends RecyclerView.ViewHolder {
            final TextView name, description, value;
            final SwitchCompat toggle;
            SettingVH(View v) {
                super(v);
                name = v.findViewById(R.id.text_setting_name);
                description = v.findViewById(R.id.text_setting_description);
                value = v.findViewById(R.id.text_setting_value);
                toggle = v.findViewById(R.id.switch_setting);
            }
        }
    }
}
