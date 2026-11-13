package org.ankool29.openttdmobile;

import android.app.AlertDialog;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.os.Bundle;
import android.text.method.ScrollingMovementMethod;
import android.util.Log;
import android.widget.ScrollView;
import android.widget.TextView;

import org.libsdl.app.SDLActivity;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.PrintWriter;
import java.util.Date;

/**
 * SDL entry activity for OpenTTD Mobile.
 *
 * SDLActivity loads libSDL2.so first, then libmain.so which contains the
 * OpenTTD entry point (SDL_main). Before that thread starts, the runtime data
 * (base set metadata, fonts and language files) is copied from the APK assets
 * into the app-private home directory.
 */
public class OpenTTDActivity extends SDLActivity {
    private static final String TAG = "OpenTTD";
    private static final String STARTUP_COMPLETE = "STARTUP_COMPLETE";

    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL2", "main" };
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        Log.i(TAG, "OpenTTD Mobile " + getVersionName() + " (" + getVersionCode() + ")");
        prepareDiagnostics();
        extractRuntimeAssets();
    }

    private String getVersionName() {
        try {
            return getPackageManager().getPackageInfo(getPackageName(), 0).versionName;
        } catch (Exception e) {
            return "unknown";
        }
    }

    private long getVersionCode() {
        try {
            PackageInfo info = getPackageManager().getPackageInfo(getPackageName(), 0);
            return info.getLongVersionCode();
        } catch (Exception e) {
            return 0;
        }
    }

    /**
     * Copy the base set metadata, fonts and language files into the directory
     * OpenTTD uses as HOME, but only once per installed version.
     */
    private void extractRuntimeAssets() {
        File root = new File(getFilesDir(), ".openttd");
        File stamp = new File(root, "assets.version");
        String version = Long.toString(getVersionCode());

        if (stamp.isFile() && version.equals(readText(stamp).trim())) {
            Log.i(TAG, "Runtime assets already extracted for version " + version);
            return;
        }

        long start = System.currentTimeMillis();
        try {
            copyAssetTree("baseset", new File(root, "baseset"));
            copyAssetTree("lang", new File(root, "lang"));
            writeText(stamp, version + "\n");
            Log.i(TAG, "Extracted runtime assets in " + (System.currentTimeMillis() - start) + " ms");
        } catch (IOException e) {
            Log.e(TAG, "Failed to extract runtime assets", e);
            writeJavaCrash(e);
        }
    }

    private void copyAssetTree(String assetPath, File destination) throws IOException {
        String[] children = getAssets().list(assetPath);
        if (children == null || children.length == 0) {
            copyAssetFile(assetPath, destination);
            return;
        }

        if (!destination.isDirectory() && !destination.mkdirs()) {
            throw new IOException("Cannot create directory " + destination);
        }
        for (String child : children) {
            copyAssetTree(assetPath + "/" + child, new File(destination, child));
        }
    }

    private void copyAssetFile(String assetPath, File destination) throws IOException {
        File parent = destination.getParentFile();
        if (parent != null && !parent.isDirectory() && !parent.mkdirs()) {
            throw new IOException("Cannot create directory " + parent);
        }
        try (InputStream input = getAssets().open(assetPath);
             OutputStream output = new FileOutputStream(destination)) {
            byte[] buffer = new byte[8192];
            int length;
            while ((length = input.read(buffer)) != -1) {
                output.write(buffer, 0, length);
            }
        }
    }

    /**
     * Rotate the previous startup log and show it if OpenTTD did not reach its
     * main loop. This makes early exits and native crashes visible on devices
     * where logcat is not accessible.
     */
    private void prepareDiagnostics() {
        File startup = new File(getFilesDir(), "startup.log");
        File previous = new File(getFilesDir(), "startup.prev.log");
        File nativeCrash = new File(getFilesDir(), "crash.log");
        File javaCrash = new File(getFilesDir(), "crash-java.log");

        if (startup.isFile()) {
            if (previous.exists() && !previous.delete()) {
                Log.w(TAG, "Cannot delete old startup.prev.log");
            }
            if (!startup.renameTo(previous)) {
                Log.w(TAG, "Cannot rotate startup.log");
            }
        }

        StringBuilder report = new StringBuilder();
        if (javaCrash.isFile()) {
            report.append("Java exception:\n").append(readText(javaCrash));
            if (!javaCrash.delete()) Log.w(TAG, "Cannot delete crash-java.log");
        }
        if (nativeCrash.isFile()) {
            report.append("Native crash:\n").append(readText(nativeCrash));
            if (!nativeCrash.delete()) Log.w(TAG, "Cannot delete crash.log");
        }
        if (previous.isFile()) {
            String log = readText(previous);
            if (!log.contains(STARTUP_COMPLETE)) {
                report.append("Previous startup did not complete:\n").append(log);
            }
            if (!previous.delete()) Log.w(TAG, "Cannot delete startup.prev.log");
        }

        if (report.length() > 0) {
            showDiagnostics(report.toString());
        }
    }

    private void showDiagnostics(String text) {
        if (text.length() > 65536) {
            text = text.substring(0, 65536) + "\n[truncated]\n";
        }
        final String report = text;

        TextView view = new TextView(this);
        view.setText(report);
        view.setTextIsSelectable(true);
        view.setMovementMethod(new ScrollingMovementMethod());
        int padding = (int) (16 * getResources().getDisplayMetrics().density);
        view.setPadding(padding, padding, padding, padding);

        ScrollView scroll = new ScrollView(this);
        scroll.addView(view);

        new AlertDialog.Builder(this)
                .setTitle("OpenTTD startup diagnostics")
                .setView(scroll)
                .setPositiveButton("Copy", (dialog, which) -> copyToClipboard(report))
                .setNeutralButton("Share", (dialog, which) -> shareReport(report))
                .setNegativeButton("Continue", null)
                .show();
    }

    private void copyToClipboard(String text) {
        ClipboardManager clipboard = (ClipboardManager) getSystemService(Context.CLIPBOARD_SERVICE);
        if (clipboard != null) {
            clipboard.setPrimaryClip(ClipData.newPlainText("OpenTTD log", text));
        }
    }

    private void shareReport(String text) {
        Intent intent = new Intent(Intent.ACTION_SEND);
        intent.setType("text/plain");
        intent.putExtra(Intent.EXTRA_TEXT, text);
        startActivity(Intent.createChooser(intent, "Share OpenTTD log"));
    }

    private void writeJavaCrash(Throwable throwable) {
        File file = new File(getFilesDir(), "crash-java.log");
        try (PrintWriter writer = new PrintWriter(new FileWriter(file, true))) {
            writer.println(new Date().toString());
            throwable.printStackTrace(writer);
        } catch (IOException e) {
            Log.e(TAG, "Cannot write crash-java.log", e);
        }
    }

    private static String readText(File file) {
        StringBuilder text = new StringBuilder();
        try (BufferedReader reader = new BufferedReader(new FileReader(file))) {
            String line;
            while ((line = reader.readLine()) != null) {
                text.append(line).append('\n');
            }
        } catch (IOException e) {
            text.append("Cannot read ").append(file).append(": ").append(e).append('\n');
        }
        return text.toString();
    }

    private static void writeText(File file, String text) throws IOException {
        File parent = file.getParentFile();
        if (parent != null && !parent.isDirectory() && !parent.mkdirs()) {
            throw new IOException("Cannot create directory " + parent);
        }
        try (OutputStream output = new FileOutputStream(file)) {
            output.write(text.getBytes("UTF-8"));
        }
    }
}
