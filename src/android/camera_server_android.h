#ifndef CAMERA_SERVER_ANDROID_H
#define CAMERA_SERVER_ANDROID_H

#include <jni.h>

#include "camera_server.h"

extern "C" JNIEXPORT void JNICALL Java_io_godot_camera_GodotCameraServer_setup(JNIEnv *env, jobject p_camera_manager);

using namespace godot;

class CameraServerAndroid : public extension::CameraServer {
private:
	static jclass cls;
	static jobject camera_server;
	static jmethodID _get_feeds;
	static jmethodID _permission_granted;
	static jmethodID _request_permission;

	static void setup(jobject p_object);

public:
	CameraServerAndroid(CameraServerExtension *p_server);
	~CameraServerAndroid();

	bool permission_granted() override;
	bool request_permission() override;

	friend JNIEXPORT void JNICALL Java_io_godot_camera_GodotCameraServer_setup(JNIEnv *env, jobject p_camera_manager);
};

#endif
