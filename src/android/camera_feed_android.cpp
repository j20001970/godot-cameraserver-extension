#include "camera_feed_android.h"

#include "camera/NdkCameraMetadataTags.h"
#include "media/NdkImage.h"

#include "jni.h"

jclass CameraFeedAndroid::cls = nullptr;

jmethodID CameraFeedAndroid::_get_camera_id = nullptr;
jmethodID CameraFeedAndroid::_get_camera_lens_facing = nullptr;
jmethodID CameraFeedAndroid::_get_formats = nullptr;
jmethodID CameraFeedAndroid::_set_format = nullptr;
jmethodID CameraFeedAndroid::_activate = nullptr;
jmethodID CameraFeedAndroid::_deactivate = nullptr;

void CameraFeedAndroid::setup(jclass p_cls) {
	JNIEnv *env = get_jni_env();
	if (env->IsSameObject(cls, nullptr)) {
		cls = (jclass)env->NewGlobalRef(p_cls);
		_get_camera_id = env->GetMethodID(cls, "getCameraId", "()Ljava/lang/String;");
		_get_camera_lens_facing = env->GetMethodID(cls, "getCameraLensFacing", "()I");
		_get_formats = env->GetMethodID(cls, "getFormats", "()[Ljava/lang/Object;");
		_set_format = env->GetMethodID(cls, "setFormat", "(I)Z");
		_activate = env->GetMethodID(cls, "activate", "(J)Z");
		_deactivate = env->GetMethodID(cls, "deactivate", "()V");
	}
}

CameraFeedAndroid::CameraFeedAndroid(CameraFeedExtension *p_feed) :
		extension::CameraFeed(p_feed) {}

CameraFeedAndroid::CameraFeedAndroid(jobject p_feed) {
	buffer = new StreamingBuffer();
	JNIEnv *env = get_jni_env();
	feed = env->NewGlobalRef(p_feed);
}

CameraFeedAndroid::~CameraFeedAndroid() {
	JNIEnv *env = get_jni_env();
	env->DeleteGlobalRef(feed);
	feed = nullptr;
	delete buffer;
}

void CameraFeedAndroid::set_image(jint p_format, jint p_width, jint p_height, jint p_rotation, jobjectArray p_buffers) {
	if (p_format == AIMAGE_FORMAT_JPEG) {
		set_jpeg_image(p_buffers, p_rotation);
	} else {
		return;
	}
}

void CameraFeedAndroid::set_jpeg_image(jobjectArray p_buffers, int p_rotation) {
	JNIEnv *env = get_jni_env();
	jint buffers_size = env->GetArrayLength(p_buffers);
	ERR_FAIL_COND(buffers_size != 1);
	jbyteArray buffer = (jbyteArray)env->GetObjectArrayElement(p_buffers, 0);
	jbyte *bytes = env->GetByteArrayElements(buffer, nullptr);
	this->buffer->start = bytes;
	this->buffer->length = env->GetArrayLength(buffer);
	decoder->decode(*this->buffer, p_rotation);
	env->ReleaseByteArrayElements(buffer, bytes, 0);
}

bool CameraFeedAndroid::activate_feed() {
	JNIEnv *env = get_jni_env();
	jfieldID _format = env->GetFieldID(cls, "mOutputFormat", "I");
	jint format = env->GetIntField(feed, _format);
	if (format == AIMAGE_FORMAT_JPEG) {
		decoder = memnew(JpegBufferDecoder(this_));
	}
	else {
		return false;
	}
	return env->CallBooleanMethod(feed, _activate, this);
}

void CameraFeedAndroid::deactivate_feed() {
	JNIEnv *env = get_jni_env();
	env->CallVoidMethod(feed, _deactivate);
	memdelete(decoder);
}

TypedArray<Dictionary> CameraFeedAndroid::get_formats() const {
	TypedArray<Dictionary> formats;
	JNIEnv *env = get_jni_env();
	jobjectArray output_formats = (jobjectArray)env->CallObjectMethod(feed, _get_formats);
	jint formats_size = env->GetArrayLength(output_formats);
	jclass format_cls = nullptr;
	jfieldID _format = nullptr;
	jfieldID _width = nullptr;
	jfieldID _height = nullptr;
	jfieldID _min_frame_duration = nullptr;
	for (int i = 0; i < formats_size; i++) {
		jobject jformat = env->GetObjectArrayElement(output_formats, i);
		if (format_cls == nullptr) {
			format_cls = env->GetObjectClass(jformat);
			_format = env->GetFieldID(format_cls, "format", "I");
			_width = env->GetFieldID(format_cls, "width", "I");
			_height = env->GetFieldID(format_cls, "height", "I");
			_min_frame_duration = env->GetFieldID(format_cls, "minFrameDuration", "J");
		}
		Dictionary format;
		if (env->GetIntField(jformat, _format) == AIMAGE_FORMAT_JPEG) {
			format["format"] = "JPEG";
		}
		format["width"] = env->GetIntField(jformat, _width);
		format["height"] = env->GetIntField(jformat, _height);
		format["min_frame_duration"] = env->GetLongField(jformat, _min_frame_duration);
		formats.append(format);
	}
	return formats;
}

bool CameraFeedAndroid::set_format(int p_index, const Dictionary &p_parameters) {
	ERR_FAIL_COND_V(this_->is_active(), false);

	JNIEnv *env = get_jni_env();
	if (env->CallBooleanMethod(feed, _set_format, p_index)) {
		return true;
	}
	return false;
}

void CameraFeedAndroid::set_this(CameraFeedExtension *p_feed) {
	this_ = p_feed;
	JNIEnv *env = get_jni_env();
	jstring camera_id = (jstring)env->CallObjectMethod(feed, _get_camera_id);
	const char *id = env->GetStringUTFChars(camera_id, nullptr);
	this_->set_name(id);
	env->ReleaseStringUTFChars(camera_id, id);
	jint lens_facing = env->CallIntMethod(feed, _get_camera_lens_facing);
	godot::CameraFeed::FeedPosition position = godot::CameraFeed::FEED_UNSPECIFIED;
	if (lens_facing == ACAMERA_LENS_FACING_FRONT) {
		position = godot::CameraFeed::FEED_FRONT;
	} else if (lens_facing == ACAMERA_LENS_FACING_BACK) {
		position = godot::CameraFeed::FEED_BACK;
	}
	this_->set_position(position);
}

CameraFeedExtension::CameraFeedExtension() {
	impl = std::make_unique<CameraFeedAndroid>(this);
}

extern "C" JNIEXPORT void JNICALL Java_io_godot_camera_GodotCameraFeed_setup(JNIEnv *env, jclass clazz) {
	CameraFeedAndroid::setup(clazz);
}

extern "C" JNIEXPORT void JNICALL Java_io_godot_camera_GodotCameraFeed_onImageAvailable(JNIEnv *env, jclass clazz, jlong p_caller, jint p_format, jint p_width, jint p_height, jint p_rotation, jobjectArray p_buffers) {
	CameraFeedAndroid *feed = (CameraFeedAndroid *)p_caller;
	feed->set_image(p_format, p_width, p_height, p_rotation, p_buffers);
}
