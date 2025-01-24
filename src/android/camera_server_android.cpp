#include "android/camera_server_android.h"

#include <memory>

#include "godot_cpp/classes/camera_server.hpp"
#include "godot_cpp/classes/engine.hpp"

#include "camera_feed_android.h"
#include "jni.h"

jclass CameraServerAndroid::cls = nullptr;
jobject CameraServerAndroid::camera_server = nullptr;
jmethodID CameraServerAndroid::_get_feeds = nullptr;
jmethodID CameraServerAndroid::_permission_granted = nullptr;
jmethodID CameraServerAndroid::_request_permission = nullptr;

void CameraServerAndroid::setup(jobject p_object) {
	JNIEnv *env = get_jni_env();
	camera_server = env->NewGlobalRef(p_object);
	jclass c = env->GetObjectClass(p_object);
	cls = (jclass)env->NewGlobalRef(c);
	_get_feeds = env->GetMethodID(cls, "getFeeds", "()[Ljava/lang/Object;");
	_permission_granted = env->GetMethodID(cls, "permissionGranted", "()Z");
	_request_permission = env->GetMethodID(cls, "requestPermission", "()V");
}

CameraServerAndroid::CameraServerAndroid(CameraServerExtension *p_server) :
		extension::CameraServer(p_server) {
	Object *android_server = Engine::get_singleton()->get_singleton("GodotCameraServer");
	ERR_FAIL_COND(android_server == nullptr);
	Signal(android_server, "camera_permission_denied").connect(Callable((Object *)this_, "emit_signal").bindv(Array::make("permission_result", false)));
	Signal(android_server, "camera_permission_granted").connect(Callable((Object *)this_, "emit_signal").bindv(Array::make("permission_result", true)));
	ERR_FAIL_NULL_MSG(camera_server, "Camera server not intialized, is GodotCameraServer loaded?");
	JNIEnv *env = get_jni_env();
	jobjectArray jfeeds = (jobjectArray)env->CallObjectMethod(camera_server, _get_feeds);
	jint feeds_size = env->GetArrayLength(jfeeds);
	for (int i = 0; i < feeds_size; i++) {
		jobject jfeed = env->GetObjectArrayElement(jfeeds, i);
		std::unique_ptr<CameraFeedAndroid> feed_impl = std::make_unique<CameraFeedAndroid>(jfeed);
		Ref<CameraFeedExtension> feed = memnew(CameraFeedExtension(std::move(feed_impl)));
		this_->get_server()->add_feed(feed);
	}
}

CameraServerAndroid::~CameraServerAndroid() = default;

bool CameraServerAndroid::permission_granted() {
	ERR_FAIL_COND_V(camera_server == nullptr, false);
	JNIEnv *env = get_jni_env();
	return env->CallBooleanMethod(camera_server, _permission_granted);
}

bool CameraServerAndroid::request_permission() {
	ERR_FAIL_COND_V(camera_server == nullptr, false);
	JNIEnv *env = get_jni_env();
	if (permission_granted()) {
		return true;
	}
	env->CallVoidMethod(camera_server, _request_permission);
	return false;
}

void CameraServerExtension::set_impl() {
	impl = std::make_unique<CameraServerAndroid>(this);
}

extern "C" JNIEXPORT void JNICALL Java_io_godot_camera_GodotCameraServer_setup(JNIEnv *env, jobject p_camera_manager) {
	CameraServerAndroid::setup(p_camera_manager);
}
