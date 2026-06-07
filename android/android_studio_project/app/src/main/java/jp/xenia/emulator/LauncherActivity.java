package jp.xenia.emulator;

import android.app.Activity;
import android.content.ClipData;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.provider.DocumentsContract;
import android.provider.OpenableColumns;
import android.util.Log;
import android.view.Menu;
import android.view.MenuItem;

import org.json.JSONArray;
import org.json.JSONObject;

import java.util.Collections;
import java.util.HashSet;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;
import androidx.viewpager2.adapter.FragmentStateAdapter;
import androidx.viewpager2.widget.ViewPager2;

import com.google.android.material.appbar.MaterialToolbar;
import com.google.android.material.floatingactionbutton.ExtendedFloatingActionButton;
import com.google.android.material.snackbar.Snackbar;
import com.google.android.material.tabs.TabLayout;
import com.google.android.material.tabs.TabLayoutMediator;

import java.util.ArrayList;
import java.util.List;

public class LauncherActivity extends AppCompatActivity implements GamePropertiesDialog.Listener {

    private static final int REQUEST_OPEN_GAME      = 1;
    private static final int REQUEST_OPEN_GPU_TRACE = 2;
    private static final int REQUEST_PICK_ART       = 3;
    private static final int REQUEST_OPEN_FOLDER    = 4;

    private static final String TAG = "LauncherActivity";

    /** SharedPreferences key for the set of folder tree URIs the user has imported. */
    static final String PREF_WATCHED_FOLDERS = "watched_folders";

    /** SharedPreferences key for the serialized game library (JSON array). */
    private static final String PREF_GAME_LIBRARY = "game_library";

    private int mPendingArtIndex = -1;

    static final List<GameEntry> sGames = new ArrayList<>();

    @Override
    protected void onCreate(final Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_launcher);

        if (savedInstanceState == null) {
            // Only restore from disk on a fresh launch, not on config change.
            loadLibrary(this);
        }

        final MaterialToolbar toolbar = findViewById(R.id.toolbar);
        setSupportActionBar(toolbar);

        final ViewPager2 pager = findViewById(R.id.pager);
        final TabLayout tabs = findViewById(R.id.tabs);

        pager.setAdapter(new PagerAdapter(this));
        new TabLayoutMediator(tabs, pager, (tab, position) -> {
            switch (position) {
                case 0: tab.setText("All Games"); break;
                case 1: tab.setText("Recent");    break;
            }
        }).attach();

        final ExtendedFloatingActionButton fab = findViewById(R.id.fab_add_games);
        fab.setOnClickListener(v -> showAddDialog());

        pager.registerOnPageChangeCallback(new ViewPager2.OnPageChangeCallback() {
            @Override
            public void onPageScrolled(int pos, float offset, int offsetPixels) {
                if (offsetPixels == 0) fab.extend(); else fab.shrink();
            }
        });
    }

    @Override
    protected void onResume() {
        super.onResume();
        // Refresh grid so new games added while in Settings (or scanner completions) appear.
        refreshAllGridFragments();
    }

    /** Persists a folder tree URI so "Refresh Library" can re-scan it later. */
    static void saveWatchedFolder(Context context, Uri treeUri) {
        final SharedPreferences prefs =
                context.getSharedPreferences(SettingsActivity.PREFS, Context.MODE_PRIVATE);
        final HashSet<String> folders = new HashSet<>(
                prefs.getStringSet(PREF_WATCHED_FOLDERS, Collections.emptySet()));
        folders.add(treeUri.toString());
        prefs.edit().putStringSet(PREF_WATCHED_FOLDERS, folders).apply();
    }

    /** Serializes sGames to SharedPreferences as a JSON array. */
    static void saveLibrary(Context context) {
        try {
            final JSONArray array = new JSONArray();
            for (final GameEntry e : sGames) {
                final JSONObject obj = new JSONObject();
                obj.put("title", e.title);
                obj.put("uri", e.uri);
                obj.put("region", e.region);
                if (e.titleId != null)     obj.put("titleId", e.titleId);
                if (e.customArtUri != null) obj.put("customArtUri", e.customArtUri);
                array.put(obj);
            }
            context.getSharedPreferences(SettingsActivity.PREFS, Context.MODE_PRIVATE)
                    .edit().putString(PREF_GAME_LIBRARY, array.toString()).apply();
        } catch (Exception e) {
            Log.w(TAG, "saveLibrary failed: " + e.getMessage());
        }
    }

    /** Restores sGames from SharedPreferences. Call once on a fresh launch. */
    private static void loadLibrary(Context context) {
        final String json = context.getSharedPreferences(
                SettingsActivity.PREFS, Context.MODE_PRIVATE)
                .getString(PREF_GAME_LIBRARY, null);
        if (json == null) return;
        try {
            final JSONArray array = new JSONArray(json);
            sGames.clear();
            for (int i = 0; i < array.length(); i++) {
                final JSONObject obj = array.getJSONObject(i);
                final GameEntry e = new GameEntry(
                        obj.getString("title"),
                        obj.getString("uri"),
                        obj.optString("region", "Xbox 360"));
                e.titleId      = obj.has("titleId")     ? obj.getString("titleId")     : null;
                e.customArtUri = obj.has("customArtUri") ? obj.getString("customArtUri") : null;
                sGames.add(e);
            }
        } catch (Exception e) {
            Log.w(TAG, "loadLibrary failed: " + e.getMessage());
        }
    }

    @Override
    public boolean onCreateOptionsMenu(final Menu menu) {
        getMenuInflater().inflate(R.menu.menu_launcher, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(final MenuItem item) {
        final int id = item.getItemId();
        if (id == R.id.menu_gpu_trace) {
            openGpuTracePicker();
            return true;
        }
        if (id == R.id.menu_settings) {
            startActivity(new Intent(this, SettingsActivity.class));
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    @Override
    public void onSetCustomArt(int gameIndex) {
        mPendingArtIndex = gameIndex;
        final Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("image/*");
        startActivityForResult(intent, REQUEST_PICK_ART);
    }

    private void showAddDialog() {
        new AlertDialog.Builder(this)
                .setTitle("Add games")
                .setItems(new String[]{"Select game files", "Select a folder"},
                        (d, which) -> { if (which == 0) openFilePicker(); else openFolderPicker(); })
                .show();
    }

    /** Opens the file picker with multi-select so several games can be picked at once. */
    private void openFilePicker() {
        final Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("*/*");
        intent.putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true);
        startActivityForResult(intent, REQUEST_OPEN_GAME);
    }

    /** Opens the folder/tree picker; every game file found inside is added at once. */
    private void openFolderPicker() {
        startActivityForResult(new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE), REQUEST_OPEN_FOLDER);
    }

    private void openGpuTracePicker() {
        final Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("application/octet-stream");
        startActivityForResult(intent, REQUEST_OPEN_GPU_TRACE);
    }

    @Override
    protected void onActivityResult(final int requestCode, final int resultCode, final Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (resultCode != Activity.RESULT_OK || data == null) return;

        if (requestCode == REQUEST_OPEN_GAME) {
            // With EXTRA_ALLOW_MULTIPLE the system puts everything into ClipData.
            final ClipData clip = data.getClipData();
            if (clip != null && clip.getItemCount() > 0) {
                final int count = clip.getItemCount();
                for (int i = 0; i < count; i++) {
                    addGameFromUri(clip.getItemAt(i).getUri(), null);
                }
                showScanningSnackbar(count);
            } else if (data.getData() != null) {
                // Single file (some pickers skip ClipData for a single selection)
                addGameFromUri(data.getData(), null);
            }

        } else if (requestCode == REQUEST_OPEN_FOLDER) {
            final Uri treeUri = data.getData();
            if (treeUri == null) return;
            try {
                getContentResolver().takePersistableUriPermission(
                        treeUri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
            } catch (SecurityException ignored) {}
            saveWatchedFolder(this, treeUri);
            addGamesFromTree(treeUri);

        } else if (requestCode == REQUEST_PICK_ART
                && mPendingArtIndex >= 0 && mPendingArtIndex < sGames.size()) {
            final Uri uri = data.getData();
            if (uri == null) return;
            getContentResolver().takePersistableUriPermission(
                    uri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
            sGames.get(mPendingArtIndex).customArtUri = uri.toString();
            BoxArtManager.clearCache(this, sGames.get(mPendingArtIndex));
            saveLibrary(this);
            refreshAllGridFragments();
            mPendingArtIndex = -1;

        } else if (requestCode == REQUEST_OPEN_GPU_TRACE) {
            final Uri uri = data.getData();
            if (uri == null) return;
            final Intent gpuIntent = new Intent(this, GpuTraceViewerActivity.class);
            final Bundle args = new Bundle();
            args.putString("target_trace_file", uri.toString());
            gpuIntent.putExtra(WindowedAppActivity.EXTRA_CVARS, args);
            startActivity(gpuIntent);
        }
    }

    /** Persists read permission, creates a GameEntry, shows its card, and queues a background scan. */
    private void addGameFromUri(Uri uri, String displayName) {
        try {
            getContentResolver().takePersistableUriPermission(
                    uri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
        } catch (SecurityException ignored) {}

        final String title = (displayName != null)
                ? displayName.replaceFirst("(?i)\\.(iso|xex|zar|xbla)$", "")
                : resolveTitle(uri);

        final GameEntry entry = new GameEntry(title, uri.toString(), "Xbox 360");
        sGames.add(entry);
        saveLibrary(this);
        refreshAllGridFragments();
        GameScanner.scan(this, entry, () -> {
            saveLibrary(this);  // capture updated title / titleId from scanner
            refreshAllGridFragments();
        });
    }

    /**
     * Lists all game files directly inside a folder tree and adds each one.
     *
     * For 6 games, 6 tasks are submitted to GameScanner's single-thread queue.
     * They run one after another and the queue goes idle when the last one finishes —
     * no loops, no timers, no re-scheduling.
     */
    private void addGamesFromTree(Uri treeUri) {
        final Uri childrenUri = DocumentsContract.buildChildDocumentsUriUsingTree(
                treeUri, DocumentsContract.getTreeDocumentId(treeUri));

        int added = 0;
        try (Cursor c = getContentResolver().query(childrenUri,
                new String[]{
                        DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                        DocumentsContract.Document.COLUMN_DISPLAY_NAME
                }, null, null, null)) {
            while (c != null && c.moveToNext()) {
                final String docId = c.getString(0);
                final String name  = c.getString(1);
                if (!isGameFileName(name)) continue;
                final Uri fileUri = DocumentsContract.buildDocumentUriUsingTree(treeUri, docId);
                addGameFromUri(fileUri, name);
                added++;
            }
        } catch (Exception e) {
            Log.w(TAG, "Folder scan failed: " + e.getMessage());
        }

        if (added == 0) {
            Snackbar.make(findViewById(android.R.id.content),
                    "No game files found in that folder", Snackbar.LENGTH_LONG).show();
        } else {
            showScanningSnackbar(added);
        }
    }

    private static boolean isGameFileName(String name) {
        if (name == null) return false;
        final String lower = name.toLowerCase();
        return lower.endsWith(".iso") || lower.endsWith(".xex")
                || lower.endsWith(".zar") || lower.endsWith(".xbla");
    }

    private void showScanningSnackbar(int count) {
        final String msg = count == 1
                ? "Scanning 1 game for art and info…"
                : "Scanning " + count + " games for art and info…";
        Snackbar.make(findViewById(android.R.id.content), msg, Snackbar.LENGTH_LONG).show();
    }

    /** Returns a clean display title for a picked game URI, using the file's actual name. */
    private String resolveTitle(Uri uri) {
        try (Cursor c = getContentResolver().query(
                uri, new String[]{OpenableColumns.DISPLAY_NAME}, null, null, null)) {
            if (c != null && c.moveToFirst()) {
                final String name = c.getString(0);
                if (name != null && !name.isEmpty())
                    return name.replaceFirst("(?i)\\.(iso|xex|zar|xbla)$", "");
            }
        } catch (Exception ignored) {}
        final String seg = uri.getLastPathSegment();
        return seg != null ? seg.replaceFirst("(?i)\\.(iso|xex|zar|xbla)$", "") : "Unknown Game";
    }

    void refreshAllGridFragments() {
        getSupportFragmentManager().getFragments().forEach(f -> {
            if (f instanceof GameGridFragment) ((GameGridFragment) f).refresh();
        });
    }

    static class GameEntry {
        String title;   // may be updated by GameScanner to the canonical Wikipedia title
        final String uri;
        final String region;
        String customArtUri = null;
        String titleId = null;  // 8-char hex e.g. "454107EC", filled in by GameScanner

        GameEntry(String title, String uri, String region) {
            this.title = title;
            this.uri = uri;
            this.region = region;
        }
    }

    private static class PagerAdapter extends FragmentStateAdapter {
        PagerAdapter(FragmentActivity fa) { super(fa); }

        @NonNull
        @Override
        public Fragment createFragment(int position) {
            return GameGridFragment.newInstance(position);
        }

        @Override
        public int getItemCount() { return 2; }
    }
}
