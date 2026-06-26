package jp.xenia.emulator;

import android.content.Context;
import android.content.SharedPreferences;
import android.database.Cursor;
import android.net.Uri;
import android.provider.OpenableColumns;

import org.json.JSONObject;

import java.io.BufferedInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.List;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

/**
 * Manages custom Adreno (Mesa Turnip) Vulkan drivers in the "adpkg" format: a .zip containing a
 * meta.json (with "libraryName" = the driver .so) plus the .so itself.
 *
 * Imported drivers are extracted into the app's INTERNAL files dir (executable — required so the
 * loader can dlopen the .so; /sdcard external storage is mounted noexec). The selected driver is
 * stored in SharedPreferences and applied as engine cvars by EmulatorActivity.
 */
public final class DriverManager {

    public static final String PREF_DRIVER_DIR   = "gpu_driver_dir";
    public static final String PREF_DRIVER_NAME  = "gpu_driver_name";
    public static final String PREF_DRIVER_LABEL = "gpu_driver_label";

    public static final String SYSTEM_LABEL = "System default";

    public static final class Driver {
        public final String label;        // human-readable name (meta.json "name")
        public final String dirPath;      // absolute path to the extracted driver directory
        public final String libraryName;  // the driver .so filename (meta.json "libraryName")

        Driver(String label, String dirPath, String libraryName) {
            this.label = label;
            this.dirPath = dirPath;
            this.libraryName = libraryName;
        }
    }

    private DriverManager() {}

    /** Internal (executable) directory holding extracted drivers. */
    static File driversDir(Context ctx) {
        final File d = new File(ctx.getFilesDir(), "gpu_drivers");
        if (!d.exists()) d.mkdirs();
        return d;
    }

    /** All installed drivers (directories with a valid meta.json + existing .so). */
    static List<Driver> list(Context ctx) {
        final List<Driver> out = new ArrayList<>();
        final File[] dirs = driversDir(ctx).listFiles(File::isDirectory);
        if (dirs == null) return out;
        for (final File dir : dirs) {
            final Driver d = read(dir);
            if (d != null) out.add(d);
        }
        return out;
    }

    private static Driver read(File dir) {
        final File meta = new File(dir, "meta.json");
        if (!meta.exists()) return null;
        try {
            final JSONObject j = new JSONObject(readText(meta));
            final String lib = j.optString("libraryName", "");
            final String name = j.optString("name", dir.getName());
            if (lib.isEmpty() || !new File(dir, lib).exists()) return null;
            return new Driver(name, dir.getAbsolutePath(), lib);
        } catch (Exception e) {
            return null;
        }
    }

    /** Extract a driver .zip (adpkg) into internal storage and return it. Throws on bad packages. */
    static Driver importZip(Context ctx, Uri zipUri) throws IOException {
        String base = sanitize(stripZip(queryDisplayName(ctx, zipUri)));
        if (base.isEmpty()) base = "driver_" + System.currentTimeMillis();
        final File outDir = new File(driversDir(ctx), base);
        if (outDir.exists()) deleteRecursive(outDir);
        if (!outDir.mkdirs()) throw new IOException("Could not create driver directory");

        final InputStream raw = ctx.getContentResolver().openInputStream(zipUri);
        if (raw == null) throw new IOException("Could not open the selected file");
        try (ZipInputStream zis = new ZipInputStream(new BufferedInputStream(raw))) {
            final byte[] buf = new byte[1 << 16];
            ZipEntry e;
            while ((e = zis.getNextEntry()) != null) {
                if (e.isDirectory()) continue;
                final String name = new File(e.getName()).getName(); // flatten any nested dirs
                if (name.isEmpty()) continue;
                final File f = new File(outDir, name);
                try (OutputStream os = new FileOutputStream(f)) {
                    int r;
                    while ((r = zis.read(buf)) > 0) os.write(buf, 0, r);
                }
            }
        }

        final Driver d = read(outDir);
        if (d == null) {
            deleteRecursive(outDir);
            throw new IOException("Not a valid driver package (needs meta.json + its libraryName .so)");
        }
        return d;
    }

    static void select(SharedPreferences p, Driver d) {
        p.edit().putString(PREF_DRIVER_DIR, d.dirPath)
                .putString(PREF_DRIVER_NAME, d.libraryName)
                .putString(PREF_DRIVER_LABEL, d.label)
                .apply();
    }

    static void selectSystem(SharedPreferences p) {
        p.edit().remove(PREF_DRIVER_DIR).remove(PREF_DRIVER_NAME).remove(PREF_DRIVER_LABEL).apply();
    }

    static String currentLabel(SharedPreferences p) {
        return p.getString(PREF_DRIVER_LABEL, SYSTEM_LABEL);
    }

    // ---- helpers ----------------------------------------------------------

    private static String readText(File f) throws IOException {
        try (InputStream in = new java.io.FileInputStream(f)) {
            final ByteArrayOutputStream bos = new ByteArrayOutputStream();
            final byte[] buf = new byte[8192];
            int r;
            while ((r = in.read(buf)) > 0) bos.write(buf, 0, r);
            return bos.toString("UTF-8");
        }
    }

    private static String queryDisplayName(Context ctx, Uri uri) {
        try (Cursor c = ctx.getContentResolver().query(uri, null, null, null, null)) {
            if (c != null && c.moveToFirst()) {
                final int idx = c.getColumnIndex(OpenableColumns.DISPLAY_NAME);
                if (idx >= 0) {
                    final String n = c.getString(idx);
                    if (n != null) return n;
                }
            }
        } catch (Exception ignored) {}
        final String last = uri.getLastPathSegment();
        return last != null ? last : "driver";
    }

    private static String stripZip(String s) {
        return s.replaceFirst("(?i)\\.zip$", "");
    }

    private static String sanitize(String s) {
        return s.replaceAll("[^A-Za-z0-9._-]", "_");
    }

    private static void deleteRecursive(File f) {
        if (f.isDirectory()) {
            final File[] kids = f.listFiles();
            if (kids != null) for (File k : kids) deleteRecursive(k);
        }
        //noinspection ResultOfMethodCallIgnored
        f.delete();
    }
}
