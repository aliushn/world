/* DO NOT EDIT THIS FILE - it is machine generated */
#include <jni.h>
/* Header for class com_example_alexnet_netlib */

#ifndef _Included_com_example_alexnet_netlib
#define _Included_com_example_alexnet_netlib
#ifdef __cplusplus
extern "C" {
#endif
/*
 * Class:     com_example_alexnet_netlib
 * Method:    stringFromJNI
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_com_example_alexnet_netlib_stringFromJNI
  (JNIEnv *, jobject);

/*
 * Class:     com_example_alexnet_netlib
 * Method:    initEnv
 * Signature: (Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;[FII)J
 */
JNIEXPORT jlong JNICALL Java_com_example_alexnet_netlib_initEnv
  (JNIEnv *, jobject, jstring, jstring, jstring, jfloatArray, jint, jint);

/*
 * Class:     com_example_alexnet_netlib
 * Method:    inference
 * Signature: (JLjava/lang/String;I)Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL Java_com_example_alexnet_netlib_inference
  (JNIEnv *, jobject, jlong, jstring, jint);

#ifdef __cplusplus
}
#endif
#endif