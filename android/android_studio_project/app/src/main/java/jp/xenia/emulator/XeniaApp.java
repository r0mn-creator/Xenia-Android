package jp.xenia.emulator;

import android.app.Application;
import android.content.SharedPreferences;

import androidx.appcompat.app.AppCompatDelegate;

public class XeniaApp extends Application {
    @Override
    public void onCreate() {
        super.onCreate();
        applyDarkMode(getSharedPreferences(SettingsActivity.PREFS, MODE_PRIVATE));
    }

    static void applyDarkMode(SharedPreferences prefs) {
        final boolean dark = prefs.getBoolean("dark_mode", false);
        AppCompatDelegate.setDefaultNightMode(
                dark ? AppCompatDelegate.MODE_NIGHT_YES : AppCompatDelegate.MODE_NIGHT_NO);
    }
}
