#ifndef ANDROID_JNI
#define ANDROID_JNI

#include <jni.h>

extern JavaVM *jvm;

JNIEnv *get_jni_env();

#endif
