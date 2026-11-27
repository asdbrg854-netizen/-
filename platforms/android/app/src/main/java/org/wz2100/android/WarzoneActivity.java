package org.wz2100.android;

import android.content.Context;
import android.content.pm.ActivityInfo;
import android.net.wifi.WifiManager;
import android.os.Build;
import android.os.Bundle;
import android.os.VibrationEffect;
import android.os.Vibrator;
import android.os.VibratorManager;
import android.util.Log;

import org.libsdl.app.SDLActivity;

public final class WarzoneActivity extends SDLActivity {
    private static final String TAG = "WarzoneActivity";
    private WifiManager.MulticastLock multicastLock;

    /**
     * Many phones filter incoming Wi-Fi broadcast packets to save power, which hides Local Player rooms.
     * Holding a multicast lock while the game is in front lets room announcements arrive.
     */
    private void acquireMulticastLock() {
        try {
            if (multicastLock == null) {
                WifiManager wifi = (WifiManager) getApplicationContext().getSystemService(Context.WIFI_SERVICE);
                if (wifi == null) return;
                multicastLock = wifi.createMulticastLock("wz2100-local-player");
                multicastLock.setReferenceCounted(false);
            }
            if (!multicastLock.isHeld()) multicastLock.acquire();
        } catch (Throwable t) {
            Log.w(TAG, "multicast lock failed", t);
        }
    }

    private void releaseMulticastLock() {
        try {
            if (multicastLock != null && multicastLock.isHeld()) multicastLock.release();
        } catch (Throwable t) {
            Log.w(TAG, "multicast unlock failed", t);
        }
    }

    @Override
    protected void onPause() {
        releaseMulticastLock();
        super.onPause();
    }

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        ImmersiveMode.apply(this);
    }

    @Override
    protected void onResume() {
        super.onResume();
        acquireMulticastLock();
        ImmersiveMode.apply(this);
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) ImmersiveMode.apply(this);
    }

    /**
     * The game is landscape-only. SDL asks for FULL_USER (every orientation) for a resizable window, which let
     * the screen rotate to portrait and override the manifest. Always keep sensor-landscape: both landscape
     * directions, and it ignores the system auto-rotate lock, so it also works when the phone is held upright.
     */
    @Override
    public void setRequestedOrientation(int requestedOrientation) {
        super.setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE);
    }

    /** Short haptic tick, called from native code (touch hold gesture feedback). */
    public void vibrateGesture(int milliseconds) {
        try {
            Vibrator vibrator;
            if (Build.VERSION.SDK_INT >= 31) {
                VibratorManager manager = (VibratorManager) getSystemService(Context.VIBRATOR_MANAGER_SERVICE);
                vibrator = manager != null ? manager.getDefaultVibrator() : null;
            } else {
                vibrator = (Vibrator) getSystemService(Context.VIBRATOR_SERVICE);
            }
            if (vibrator != null && vibrator.hasVibrator()) {
                vibrator.vibrate(VibrationEffect.createOneShot(milliseconds, VibrationEffect.DEFAULT_AMPLITUDE));
            }
        } catch (Throwable t) {
            Log.w(TAG, "vibrateGesture failed", t);
        }
    }

    @Override
    protected String[] getLibraries() {
        // The Android vcpkg triplet links SDL3 statically into libmain.so.
        Log.i(TAG, "Loading native library libmain.so");
        return new String[] { "main" };
    }

    @Override
    protected String[] getArguments() {
        String dataDirectory = getFilesDir().getAbsolutePath() + "/warzone-data";
        Log.i(TAG, "Starting game with --datadir=" + dataDirectory);
        return new String[] { "--datadir=" + dataDirectory };
    }
}
