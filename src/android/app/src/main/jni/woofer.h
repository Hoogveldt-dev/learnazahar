// Copyright 2026 Hoogveldt-dev
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// JNI side of the Woofer bridge (docs/woofer-bridge/PROTOCOL.md): messages of
// Woofer games go to org.citra.citra_emu.woofer.WooferBridge.

#pragma once

#include <jni.h>

namespace WooferJni {

/// Caches the Java class and method; called from JNI_OnLoad.
void InitJNI(JNIEnv* env);
void CleanupJNI(JNIEnv* env);

/// Starts sending messages to Java; called when a game boots.
void Attach();

/// Stops sending messages and clears the host flags; called when emulation stops.
void Detach();

} // namespace WooferJni
