package io.godot.camera;

import android.Manifest;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.Context;
import android.content.pm.PackageManager;
import android.hardware.camera2.CameraAccessException;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraDevice;
import android.hardware.camera2.CameraManager;
import android.os.Handler;
import android.os.HandlerThread;
import android.util.Log;
import android.view.Display;
import android.view.Surface;
import android.view.WindowManager;

import androidx.annotation.NonNull;
import androidx.collection.ArraySet;

import org.godotengine.godot.Godot;
import org.godotengine.godot.plugin.GodotPlugin;
import org.godotengine.godot.plugin.SignalInfo;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;

public class GodotCameraServer extends GodotPlugin {
    static {
        System.loadLibrary("cameraserver-extension.android");
    }

    private static final String TAG = GodotCameraServer.class.getSimpleName();

    private static GodotCameraServer singleton;

    private final CameraManager mCameraManager;

    private final List<GodotCameraFeed> mFeeds;

    private HandlerThread mHandlerThread = null;
    private Handler mHandler = null;

    public static GodotCameraServer getSingleton() {
        return singleton;
    }

    public GodotCameraServer(Godot godot) {
        super(godot);
        setup();
        GodotCameraFeed.setup();
        singleton = this;
        Activity activity = godot.getActivity();
        assert activity != null;
        mCameraManager = (CameraManager) activity.getSystemService(Activity.CAMERA_SERVICE);
        mFeeds = new ArrayList<>();
        String[] idList = new String[0];
        try {
            idList = mCameraManager.getCameraIdList();
        } catch (CameraAccessException e) {
            Log.e(TAG, String.format("Failed to get camera id list: %s", e.getMessage()));
        }
        for (String id : idList) {
            CameraCharacteristics cameraCharacteristics;
            try {
                cameraCharacteristics = mCameraManager.getCameraCharacteristics(id);
            } catch (CameraAccessException e) {
                Log.e(TAG, String.format("Failed to get camera characteristics: %s", e.getMessage()));
                continue;
            }
            GodotCameraFeed feed = new GodotCameraFeed(id, cameraCharacteristics);
            mFeeds.add(feed);
        }
    }

    public Object[] getFeeds() {
        return mFeeds.toArray();
    }

    public boolean permissionGranted() {
        String[] grantedPermissions = getGodot().getGrantedPermissions();
        if (grantedPermissions == null) {
            return false;
        }
        for (String permission : grantedPermissions) {
            if (permission.equals(Manifest.permission.CAMERA)) {
                return true;
            }
        }
        return false;
    }

    public void requestPermission() {
        getGodot().requestPermission("CAMERA");
    }

    public int getDeviceRotation() {
        Activity activity = getGodot().getActivity();
        assert activity != null;
        WindowManager windowManager = (WindowManager) activity.getSystemService(Context.WINDOW_SERVICE);
        Display display = windowManager.getDefaultDisplay();
        int rotation = display.getRotation();
        if (rotation == Surface.ROTATION_90) {
            return 270;
        } else if (rotation == Surface.ROTATION_180) {
            return 180;
        } else if (rotation == Surface.ROTATION_270) {
            return 90;
        }
        return 0;
    }

    @SuppressLint("MissingPermission")
    public boolean openCamera(String id, CameraDevice.StateCallback callback) {
        if (!permissionGranted()) {
            requestPermission();
            return true;
        }
        startHandlerThread();
        try {
            mCameraManager.openCamera(id, callback, mHandler);
        } catch (CameraAccessException e) {
            Log.e(TAG, String.format("Failed to open camera: %s", e.getMessage()));
            return false;
        }
        return true;
    }

    private void startHandlerThread() {
        if (mHandlerThread != null) {
            return;
        }
        mHandlerThread = new HandlerThread("GodotCameraThread");
        mHandlerThread.start();
        mHandler = new Handler(mHandlerThread.getLooper());
    }

    private void stopHandlerThread() {
        if (mHandlerThread == null) {
            return;
        }
        mHandlerThread.quitSafely();
        try {
            mHandlerThread.join();
            mHandlerThread = null;
            mHandler = null;
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
    }

    @Override
    public void onMainRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        for (int i = 0; i < permissions.length; i++) {
            if (permissions[i].equals(Manifest.permission.CAMERA)) {
                boolean granted = grantResults[i] == PackageManager.PERMISSION_GRANTED;
                if (granted) {
                    emitSignal("camera_permission_granted");
                } else {
                    emitSignal("camera_permission_denied");
                }
                for (GodotCameraFeed feed : mFeeds) {
                    feed.onPermissionResult(granted);
                }
            }
        }
    }

    @Override
    public void onMainPause() {
        for (GodotCameraFeed feed : mFeeds) {
            feed.onActivityPaused();
        }
        stopHandlerThread();
    }

    @Override
    public void onMainResume() {
        for (GodotCameraFeed feed : mFeeds) {
            feed.onActivityResumed();
        }
    }

    @NonNull
    @Override
    public Set<SignalInfo> getPluginSignals() {
        Set<SignalInfo> signals = new ArraySet<>();
        signals.add(new SignalInfo("camera_permission_denied"));
        signals.add(new SignalInfo("camera_permission_granted"));
        return signals;
    }

    @NonNull
    @Override
    public String getPluginName() {
        return getClass().getSimpleName();
    }

    private native void setup();
}
