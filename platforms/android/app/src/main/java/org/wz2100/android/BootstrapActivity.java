package org.wz2100.android;

import android.app.Activity;
import android.content.Intent;
import android.content.res.AssetManager;
import android.os.Bundle;
import android.os.SystemClock;
import android.util.Log;
import android.view.Gravity;
import android.view.View;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ProgressBar;
import android.widget.TextView;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.charset.StandardCharsets;

public final class BootstrapActivity extends Activity {
    private static final String TAG = "WZBootstrap";
    private static final String MANIFEST = "wz2100-assets-manifest.txt";
    private static final long UI_PROGRESS_INTERVAL_BYTES = 8L * 1024L * 1024L;
    private TextView status;
    private ProgressBar progress;
    private Button retry;
    private volatile boolean setupRunning;

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        layout.setGravity(Gravity.CENTER);
        layout.setPadding(40, 40, 40, 40);

        status = new TextView(this);
        status.setText(getString(R.string.bootstrap_preparing));
        progress = new ProgressBar(this);
        progress.setIndeterminate(true);
        retry = new Button(this);
        retry.setText(getString(R.string.bootstrap_retry));
        retry.setVisibility(View.GONE);
        retry.setOnClickListener(view -> startSetup());

        layout.addView(progress);
        layout.addView(status);
        layout.addView(retry);
        setContentView(layout);
        ImmersiveMode.apply(this);
        startSetup();
    }

    @Override
    protected void onResume() {
        super.onResume();
        ImmersiveMode.apply(this);
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) ImmersiveMode.apply(this);
    }

    private synchronized void startSetup() {
        if (setupRunning) return;
        setupRunning = true;
        progress.setVisibility(View.VISIBLE);
        retry.setVisibility(View.GONE);
        status.setText(getString(R.string.bootstrap_checking));

        new Thread(() -> {
            try {
                AssetManager assets = getAssets();
                String expectedFingerprint = readAssetText(assets, MANIFEST).trim();
                if (!expectedFingerprint.matches("[0-9a-f]{64}")) {
                    throw new IOException("Invalid or missing asset manifest in APK");
                }

                File destination = new File(getFilesDir(), "warzone-data");
                File marker = new File(destination, ".assets-ready");
                if (isCurrentData(destination, marker, expectedFingerprint)) {
                    Log.i(TAG, "Game data already matches APK fingerprint " + expectedFingerprint);
                    postStatus(getString(R.string.bootstrap_verified));
                } else {
                    long start = SystemClock.elapsedRealtime();
                    Log.i(TAG, "Refreshing game data from APK; fingerprint=" + expectedFingerprint
                            + ", destination=" + destination.getAbsolutePath());
                    postStatus(getString(R.string.bootstrap_storage));
                    deleteTree(destination);
                    if (!destination.mkdirs() && !destination.isDirectory()) {
                        throw new IOException("Could not create private game-data directory");
                    }

                    String[] rootAssets = assets.list("");
                    if (rootAssets == null || rootAssets.length == 0) {
                        throw new IOException("APK contains no game data assets");
                    }
                    Log.i(TAG, "APK asset root contains " + rootAssets.length + " entries");
                    long[] copiedBytes = {0};
                    long[] lastUiReportBytes = {0};
                    for (String asset : rootAssets) {
                        postStatus(getString(R.string.bootstrap_copying, asset, mebibytes(copiedBytes[0])));
                        copyAssetTree(assets, asset, new File(destination, asset), copiedBytes, lastUiReportBytes);
                    }

                    postStatus(getString(R.string.bootstrap_verifying));
                    File baseArchive = new File(destination, "base.wz");
                    File multiplayerArchive = new File(destination, "mp.wz");
                    if (!baseArchive.isFile() || baseArchive.length() == 0
                            || !multiplayerArchive.isFile() || multiplayerArchive.length() == 0) {
                        throw new IOException("The APK is missing a non-empty base.wz or mp.wz archive");
                    }
                    String copiedFingerprint = readFileText(new File(destination, MANIFEST)).trim();
                    if (!expectedFingerprint.equals(copiedFingerprint)) {
                        throw new IOException("Copied game-data fingerprint does not match the APK");
                    }
                    writeFileText(marker, expectedFingerprint + "\n");
                    Log.i(TAG, "Game data refresh completed in "
                            + (SystemClock.elapsedRealtime() - start) + " ms; copied="
                            + mebibytes(copiedBytes[0]) + " MiB, base.wz=" + baseArchive.length()
                            + " bytes, mp.wz=" + multiplayerArchive.length() + " bytes");
                    postStatus(getString(R.string.bootstrap_verified));
                }

                runOnUiThread(() -> {
                    setupRunning = false;
                    if (isFinishing() || isDestroyed()) return;
                    Log.i(TAG, "Starting native game activity");
                    startActivity(new Intent(this, WarzoneActivity.class));
                    finish();
                });
            } catch (Exception error) {
                Log.e(TAG, "Game data setup failed", error);
                runOnUiThread(() -> {
                    setupRunning = false;
                    if (isFinishing() || isDestroyed()) return;
                    progress.setVisibility(View.GONE);
                    status.setText(getString(R.string.bootstrap_failed, error.getMessage()));
                    retry.setVisibility(View.VISIBLE);
                });
            }
        }, "WarzoneDataSetup").start();
    }

    private void postStatus(String message) {
        runOnUiThread(() -> {
            if (status != null && !isFinishing() && !isDestroyed()) status.setText(message);
        });
    }

    private static String mebibytes(long bytes) {
        return Long.toString(bytes / (1024L * 1024L));
    }

    private static boolean isCurrentData(File destination, File marker, String expectedFingerprint)
            throws IOException {
        if (!marker.isFile() || !destination.isDirectory()) return false;
        File copiedManifest = new File(destination, MANIFEST);
        if (!copiedManifest.isFile()) return false;
        File baseArchive = new File(destination, "base.wz");
        File multiplayerArchive = new File(destination, "mp.wz");
        if (!baseArchive.isFile() || baseArchive.length() == 0
                || !multiplayerArchive.isFile() || multiplayerArchive.length() == 0) return false;
        return expectedFingerprint.equals(readFileText(marker).trim())
                && expectedFingerprint.equals(readFileText(copiedManifest).trim());
    }

    private static String readAssetText(AssetManager assets, String path) throws IOException {
        try (InputStream input = assets.open(path)) {
            return readText(input);
        }
    }

    private static String readFileText(File file) throws IOException {
        try (InputStream input = new FileInputStream(file)) {
            return readText(input);
        }
    }

    private static String readText(InputStream input) throws IOException {
        ByteArrayOutputStream output = new ByteArrayOutputStream();
        byte[] buffer = new byte[4096];
        int count;
        while ((count = input.read(buffer)) != -1) output.write(buffer, 0, count);
        return new String(output.toByteArray(), StandardCharsets.UTF_8);
    }

    private static void writeFileText(File file, String text) throws IOException {
        try (FileOutputStream output = new FileOutputStream(file, false)) {
            output.write(text.getBytes(StandardCharsets.UTF_8));
            output.getFD().sync();
        }
    }

    private void copyAssetTree(AssetManager assets, String assetPath, File target,
                               long[] copiedBytes, long[] lastUiReportBytes) throws Exception {
        String[] children = assets.list(assetPath);
        if (children != null && children.length > 0) {
            if (!target.exists() && !target.mkdirs()) {
                throw new IOException("Could not create " + target);
            }
            for (String child : children) {
                copyAssetTree(assets, assetPath + "/" + child, new File(target, child),
                        copiedBytes, lastUiReportBytes);
            }
            return;
        }

        File parent = target.getParentFile();
        if (parent != null && !parent.isDirectory() && !parent.mkdirs()) {
            throw new IOException("Could not create " + parent);
        }
        Log.i(TAG, "Copying asset: " + assetPath);
        long fileBytes = 0;
        try (InputStream input = new BufferedInputStream(
                     assets.open(assetPath, AssetManager.ACCESS_STREAMING), 1024 * 1024);
             BufferedOutputStream output = new BufferedOutputStream(
                     new FileOutputStream(target), 1024 * 1024)) {
            byte[] buffer = new byte[1024 * 1024];
            int count;
            while ((count = input.read(buffer)) != -1) {
                output.write(buffer, 0, count);
                fileBytes += count;
                copiedBytes[0] += count;
                if (copiedBytes[0] - lastUiReportBytes[0] >= UI_PROGRESS_INTERVAL_BYTES) {
                    lastUiReportBytes[0] = copiedBytes[0];
                    String message = getString(R.string.bootstrap_copying, assetPath, mebibytes(copiedBytes[0]));
                    postStatus(message);
                    Log.i(TAG, "Copy progress: " + mebibytes(copiedBytes[0])
                            + " MiB; current asset=" + assetPath);
                }
            }
        }
        Log.i(TAG, "Copied asset: " + assetPath + " (" + fileBytes + " bytes)");
    }

    private static void deleteTree(File file) throws IOException {
        if (!file.exists()) return;
        if (file.isDirectory()) {
            File[] children = file.listFiles();
            if (children == null) throw new IOException("Could not list " + file);
            for (File child : children) deleteTree(child);
        }
        if (!file.delete()) throw new IOException("Could not remove stale game data: " + file);
    }
}
