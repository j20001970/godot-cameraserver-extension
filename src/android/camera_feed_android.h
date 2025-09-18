#ifndef CAMERA_FEED_ANDROID_H
#define CAMERA_FEED_ANDROID_H

#include "camera_feed.h"

#include <jni.h>

#include "buffer_decoder.h"

extern "C" JNIEXPORT void JNICALL Java_io_godot_camera_GodotCameraFeed_setup(JNIEnv *env, jclass clazz);
extern "C" JNIEXPORT void JNICALL Java_io_godot_camera_GodotCameraFeed_onImageAvailable(JNIEnv *env, jclass clazz, jlong p_caller, jint p_format, jint p_width, jint p_height, jint p_rotation, jobjectArray p_buffers);

using namespace godot;

class CameraFeedAndroid : public extension::CameraFeed {
private:
	static jclass cls;

	static jmethodID _get_camera_id;
	static jmethodID _get_camera_lens_facing;
	static jmethodID _get_formats;
	static jmethodID _set_format;
	static jmethodID _activate;
	static jmethodID _deactivate;

	jobject feed = nullptr;
	BufferDecoder *decoder = nullptr;
	StreamingBuffer *buffer = nullptr;
	int rotation = 0;

	static void setup(jclass p_feed);

	void set_image(jint p_format, jint p_width, jint p_height, jint p_rotation, jobjectArray p_buffers);
	void set_jpeg_image(jobjectArray p_buffers, int p_rotation);

public:
	CameraFeedAndroid(CameraFeedExtension *p_feed);
	CameraFeedAndroid(jobject p_feed);
	~CameraFeedAndroid();

	bool activate_feed() override;
	void deactivate_feed() override;

	TypedArray<Dictionary> get_formats() const override;
	bool set_format(int p_index, const Dictionary &p_parameters) override;

	void set_this(CameraFeedExtension *p_feed) override;

	friend JNIEXPORT void JNICALL Java_io_godot_camera_GodotCameraFeed_setup(JNIEnv *env, jclass clazz);
	friend JNIEXPORT void JNICALL Java_io_godot_camera_GodotCameraFeed_onImageAvailable(JNIEnv *env, jclass clazz, jlong p_caller, jint p_format, jint p_width, jint p_height, jint p_rotation, jobjectArray p_buffers);
};

#endif
