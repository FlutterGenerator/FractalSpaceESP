#include <list>
#include <vector>
#include <string.h>
#include <pthread.h>
#include <thread>
#include <cstring>
#include <jni.h>
#include <unistd.h>
#include <fstream>
#include <iostream>
#include <dlfcn.h>

#include "Includes/Logger.h"
#include "Includes/obfuscate.h"
#include "Includes/Utils.h"
#include "Menu/Register.h"

#include "Hooks.h"

ESP espOverlay;

jobjectArray GetFeatureList(JNIEnv *env, jobject context) {
    jobjectArray ret;

    const char *features[] = {
            OBFUSCATE("Category_ESP"),//Not counted
            OBFUSCATE("Toggle_Esp"),//0                 
            OBFUSCATE("Toggle_Esp Line"),//1 
            OBFUSCATE("Toggle_Esp Box"),//2
            OBFUSCATE("Toggle_Esp Object"),//3         
    };

    int Total_Feature = (sizeof features / sizeof features[0]);
    ret = (jobjectArray)
            env->NewObjectArray(Total_Feature, env->FindClass(OBFUSCATE("java/lang/String")),
                                env->NewStringUTF(""));

    for (int i = 0; i < Total_Feature; i++)
        env->SetObjectArrayElement(ret, i, env->NewStringUTF(features[i]));

    return (ret);
}

void Changes(JNIEnv *env, jclass clazz, jobject obj,
                                        jint featNum, jstring featName, jint value,
                                        jboolean boolean, jstring str) {
    switch (featNum) {
        case 0:
            Esp = boolean;
            break;
        case 1:
            EspLine = boolean;
            break;    
        case 2:
            EspBox = boolean;
            break;
        case 3:
            EspObject = boolean;
            break;
    }
}

[[maybe_unused]] __attribute__((constructor))
void lib_main() {
    std::thread(hack_thread).detach();
}

extern "C"
JNIEXPORT void JNICALL
Java_com_android_support_Menu_DrawOn(JNIEnv *env, jclass type, jobject espView, jobject canvas) {
    espOverlay = ESP(env, espView, canvas);
    if (espOverlay.isValid()) {
        DrawESP(espOverlay, espOverlay.getWidth(), espOverlay.getHeight());
    }
}

extern "C"
JNIEXPORT jint JNICALL
JNI_OnLoad(JavaVM *vm, void *reserved) {
    JNIEnv *env;

    vm->GetEnv((void **) &env, JNI_VERSION_1_6);

    static const JNINativeMethod menuMethods[] = {
            {OBFUSCATE("Icon"), OBFUSCATE("()Ljava/lang/String;"), reinterpret_cast<void *>(Icon)},
            {OBFUSCATE("IconWebViewData"),  OBFUSCATE("()Ljava/lang/String;"), reinterpret_cast<void *>(IconWebViewData)},
            {OBFUSCATE("IsGameLibLoaded"),  OBFUSCATE("()Z"), reinterpret_cast<void *>(isGameLibLoaded)},
            {OBFUSCATE("Init"),  OBFUSCATE("(Landroid/content/Context;Landroid/widget/TextView;Landroid/widget/TextView;)V"), reinterpret_cast<void *>(Init)},
            {OBFUSCATE("GetFeatureList"),  OBFUSCATE("()[Ljava/lang/String;"), reinterpret_cast<void *>(GetFeatureList)},
    };

    if (Register(env, "com/android/support/Menu", menuMethods, sizeof(menuMethods) / sizeof(JNINativeMethod)) != 0)
        return JNI_ERR;

    static const JNINativeMethod prefMethods[] = {
            { OBFUSCATE("Changes"), OBFUSCATE("(Landroid/content/Context;ILjava/lang/String;IZLjava/lang/String;)V"), reinterpret_cast<void *>(Changes)},
    };

    if (Register(env, "com/android/support/Preferences",
                 prefMethods, sizeof(prefMethods) / sizeof(JNINativeMethod)) != 0)
        return JNI_ERR;

    static const JNINativeMethod mainMethods[] = {
            { OBFUSCATE("CheckOverlayPermission"), OBFUSCATE("(Landroid/content/Context;)V"), reinterpret_cast<void *>(CheckOverlayPermission)},
    };

    if (Register(env, "com/android/support/Main", mainMethods, sizeof(mainMethods) / sizeof(JNINativeMethod)) != 0)
        return JNI_ERR;

    return JNI_VERSION_1_6;
}
