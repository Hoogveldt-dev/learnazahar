// Copyright 2026 Hoogveldt-dev
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string>
#include <string_view>
#include <jni.h>
#include "common/logging/log.h"
#include "core/woofer/bridge.h"
#include "core/woofer/vocab.h"
#include "jni/android_common/android_common.h"
#include "jni/id_cache.h"
#include "jni/woofer.h"

namespace {

jclass s_bridge_class;
jmethodID s_on_message;

/// On the emulation thread: hands one message to WooferBridge.onMessage.
void OnMessage(std::string_view type, std::string_view json) {
    if (!s_bridge_class || !s_on_message) {
        return;
    }
    JNIEnv* env = IDCache::GetEnvForThread();
    // The emulation thread never returns to Java, so local references must be freed here.
    jstring j_type = ToJString(env, type);
    jstring j_json = ToJString(env, json);
    env->CallStaticVoidMethod(s_bridge_class, s_on_message, j_type, j_json);
    if (env->ExceptionCheck()) {
        LOG_ERROR(Frontend, "Woofer: the frontend threw on a \"{}\" message", type);
        env->ExceptionDescribe();
        env->ExceptionClear();
    }
    env->DeleteLocalRef(j_type);
    env->DeleteLocalRef(j_json);
}

} // namespace

namespace WooferJni {

void InitJNI(JNIEnv* env) {
    jclass bridge_class = env->FindClass("org/citra/citra_emu/woofer/WooferBridge");
    if (!bridge_class) {
        env->ExceptionClear();
        LOG_ERROR(Frontend, "Woofer: WooferBridge class not found, bridge disabled");
        return;
    }
    s_bridge_class = reinterpret_cast<jclass>(env->NewGlobalRef(bridge_class));
    env->DeleteLocalRef(bridge_class);
    s_on_message = env->GetStaticMethodID(s_bridge_class, "onMessage",
                                          "(Ljava/lang/String;Ljava/lang/String;)V");
    if (!s_on_message) {
        env->ExceptionClear();
        LOG_ERROR(Frontend, "Woofer: WooferBridge.onMessage not found, bridge disabled");
    }
}

void CleanupJNI(JNIEnv* env) {
    Woofer::SetHandler({});
    if (s_bridge_class) {
        env->DeleteGlobalRef(s_bridge_class);
        s_bridge_class = nullptr;
    }
    s_on_message = nullptr;
}

void Attach() {
    Woofer::SetHandler(&OnMessage);
}

void Detach() {
    Woofer::SetHandler({});
    Woofer::SetHostFlags(0);
}

} // namespace WooferJni

extern "C" {

JNIEXPORT void JNICALL Java_org_citra_citra_1emu_woofer_WooferBridge_setHostFlags(
    JNIEnv* env, [[maybe_unused]] jobject obj, jint flags) {
    Woofer::SetHostFlags(static_cast<u32>(flags));
}

JNIEXPORT jstring JNICALL
Java_org_citra_citra_1emu_woofer_WooferBridge_lastHello(JNIEnv* env, [[maybe_unused]] jobject obj) {
    return ToJString(env, Woofer::LastHello());
}

JNIEXPORT jstring JNICALL
Java_org_citra_citra_1emu_woofer_WooferBridge_lastLine(JNIEnv* env, [[maybe_unused]] jobject obj) {
    return ToJString(env, Woofer::LastLine());
}

JNIEXPORT jboolean JNICALL Java_org_citra_citra_1emu_woofer_WooferBridge_appendVocab(
    JNIEnv* env, [[maybe_unused]] jobject obj, jstring j_json_object) {
    return static_cast<jboolean>(Woofer::AppendVocab(GetJString(env, j_json_object)));
}

JNIEXPORT jstring JNICALL
Java_org_citra_citra_1emu_woofer_WooferBridge_readVocab(JNIEnv* env, [[maybe_unused]] jobject obj) {
    return ToJString(env, Woofer::ReadVocab());
}

} // extern "C"
