#include <jni.h>

JavaVM *jvm = nullptr;

extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
	JNIEnv *env;
	if (vm->GetEnv((void **)&env, JNI_VERSION_1_6) != JNI_OK) {
		return JNI_ERR;
	}
	jvm = vm;
	return JNI_VERSION_1_6;
}

JNIEnv *get_jni_env() {
	JNIEnv *env;
	jvm->GetEnv((void **)&env, JNI_VERSION_1_6);
	return env;
}
