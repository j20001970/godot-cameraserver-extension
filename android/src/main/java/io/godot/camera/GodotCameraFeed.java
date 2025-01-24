package io.godot.camera;

import android.graphics.ImageFormat;
import android.hardware.camera2.CameraAccessException;
import android.hardware.camera2.CameraCaptureSession;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraDevice;
import android.hardware.camera2.CaptureRequest;
import android.hardware.camera2.params.StreamConfigurationMap;
import android.media.Image;
import android.media.ImageReader;
import android.util.Log;
import android.util.Size;

import androidx.annotation.NonNull;

import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.List;

public class GodotCameraFeed {
    static class FeedFormat {
        int format;
        int width;
        int height;
        long minFrameDuration;

        protected FeedFormat(int format, int width, int height, long frameDuration) {
            this.format = format;
            this.width = width;
            this.height = height;
            this.minFrameDuration = frameDuration;
        }
    }

    private static final String TAG = GodotCameraFeed.class.getName();

    private final String mCameraId;
    private final CameraCharacteristics mCameraCharacteristics;

    private long mNativeCaller = 0;

    private int mOutputFormat;
    private int mOutputWidth;
    private int mOutputHeight;

    private int mFrameRotation = 0;
    private boolean mActivated = false;
    private CameraDevice mCameraDevice = null;
    private ImageReader mImageReader = null;

    private final CameraDevice.StateCallback mStateCallback = new CameraDevice.StateCallback() {
        @Override
        public void onOpened(@NonNull CameraDevice cameraDevice) {
            mCameraDevice = cameraDevice;
            mImageReader = ImageReader.newInstance(mOutputWidth, mOutputHeight, mOutputFormat, 2);
            mImageReader.setOnImageAvailableListener(mImageAvailableListener, null);
            try {
                mCameraDevice.createCaptureSession(List.of(mImageReader.getSurface()), mSessionCallback, null);
            } catch (CameraAccessException e) {
                Log.e(TAG, String.format("Failed to create capture session: %s", e.getMessage()));
            }
        }

        @Override
        public void onDisconnected(@NonNull CameraDevice cameraDevice) {
            Log.e(TAG, String.format("Camera id %s disconnected", mCameraId));
        }

        @Override
        public void onError(@NonNull CameraDevice cameraDevice, int i) {
            Log.e(TAG, String.format("Camera id %s error: %d", mCameraId, i));
        }
    };

    private final CameraCaptureSession.StateCallback mSessionCallback = new CameraCaptureSession.StateCallback() {
        @Override
        public void onConfigured(@NonNull CameraCaptureSession cameraCaptureSession) {
            if (mCameraDevice == null) {
                return;
            }
            try {
                CaptureRequest.Builder builder = mCameraDevice.createCaptureRequest(CameraDevice.TEMPLATE_PREVIEW);
                builder.addTarget(mImageReader.getSurface());
                cameraCaptureSession.setRepeatingRequest(builder.build(), null, null);
            } catch (CameraAccessException e) {
                Log.e(TAG, String.format("Failed to set capture session: %s", e.getMessage()));
            }
        }

        @Override
        public void onConfigureFailed(@NonNull CameraCaptureSession cameraCaptureSession) {
            Log.e(TAG, String.format("Failed to configure capture session for id %s", mCameraId));
        }
    };

    private final ImageReader.OnImageAvailableListener mImageAvailableListener = imageReader -> {
        Image image = imageReader.acquireLatestImage();
        if (image == null) {
            return;
        }
        int format = image.getFormat();
        int width = image.getWidth();
        int height = image.getHeight();
        Image.Plane[] planes = image.getPlanes();
        byte[][] buffers = new byte[planes.length][];
        for (int i = 0; i < planes.length; i++) {
            ByteBuffer buffer = planes[i].getBuffer();
            buffers[i] = new byte[buffer.remaining()];
            buffer.get(buffers[i]);
        }
        if (mNativeCaller != 0) {
            onImageAvailable(mNativeCaller, format, width, height, mFrameRotation, buffers);
        }
        image.close();
    };

    public GodotCameraFeed(String id, CameraCharacteristics cameraCharacteristics) {
        mCameraId = id;
        mCameraCharacteristics = cameraCharacteristics;
    }

    public String getCameraId() {
        return mCameraId;
    }

    public int getCameraLensFacing() {
        Integer lensFacing = mCameraCharacteristics.get(CameraCharacteristics.LENS_FACING);
        if (lensFacing == null) {
            return CameraCharacteristics.LENS_FACING_EXTERNAL;
        }
        return lensFacing;
    }

    public Object[] getFormats() {
        StreamConfigurationMap map = mCameraCharacteristics.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
        assert map != null;
        List<FeedFormat> result = new ArrayList<>();
        int[] formats = map.getOutputFormats();
        for (int format : formats) {
            if (!isSupportedFormat(format)) {
                continue;
            }
            Size[] sizes = map.getOutputSizes(format);
            for (Size size : sizes) {
                long duration = map.getOutputMinFrameDuration(format, size);
                result.add(new FeedFormat(format, size.getWidth(), size.getHeight(), duration));
            }
        }
        return result.toArray();
    }

    public boolean setFormat(int index) {
        Object[] formats = getFormats();
        if (index < 0) {
            return false;
        }
        if (index < formats.length) {
            FeedFormat format = (FeedFormat) formats[index];
            mOutputFormat = format.format;
            mOutputWidth = format.width;
            mOutputHeight = format.height;
            return true;
        }
        return false;
    }

    public boolean activate(long nativeCaller) {
        mNativeCaller = nativeCaller;
        mActivated = startCamera();
        return mActivated;
    }

    public void deactivate() {
        stopCamera();
        mActivated = false;
        mNativeCaller = 0;
    }

    private boolean isSupportedFormat(int format) {
        return format == ImageFormat.JPEG;
    }

    private int getFrameRotation(int deviceOrientation) {
        Integer sensorOrientation = mCameraCharacteristics.get(CameraCharacteristics.SENSOR_ORIENTATION);
        if (sensorOrientation == null) {
            sensorOrientation = 0;
        }
        int lensFacing = getCameraLensFacing();
        // https://developer.android.com/media/camera/camera2/camera-preview#orientation_calculation
        int sign = lensFacing == CameraCharacteristics.LENS_FACING_FRONT ? 1 : -1;
        return (sensorOrientation - deviceOrientation * sign + 360) % 360;
    }

    private boolean startCamera() {
        if (mNativeCaller == 0) {
            return false;
        }
        if (mCameraDevice != null) {
            return true;
        }
        int deviceRotation = GodotCameraServer.getSingleton().getDeviceRotation();
        mFrameRotation = getFrameRotation(deviceRotation);
        return GodotCameraServer.getSingleton().openCamera(mCameraId, mStateCallback);
    }

    private void stopCamera() {
        if (mCameraDevice != null) {
            mCameraDevice.close();
            mCameraDevice = null;
        }
        if (mImageReader != null) {
            mImageReader.close();
            mImageReader = null;
        }
    }

    protected void onPermissionResult(boolean granted) {
        if (granted && mActivated) {
            startCamera();
        }
    }

    protected void onActivityPaused() {
        stopCamera();
    }

    protected void onActivityResumed() {
        if (mActivated) {
            startCamera();
        }
    }

    protected static native void setup();

    private native void onImageAvailable(long nativeCaller, int format, int width, int height, int rotation, byte[][] buffers);
}
