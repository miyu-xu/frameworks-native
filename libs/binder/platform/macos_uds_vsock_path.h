/*
 * Copyright (C) 2026 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */

#pragma once

#include <string>

namespace android {

// Maps (bind_cid, port) to a Unix domain socket path. Same logical key as Windows
// \\.\pipe\binder_rpc_vsock_{cid}_{port} and virtmgr Rust vsock_transport (macOS).
// Keep path format in sync with:
//   packages/modules/Virtualization/android/virtmgr/src/vsock_transport.rs
std::string binderRpcVsockHostPath(unsigned int bind_cid, unsigned int port);

} // namespace android
