/*
 * Copyright (C) 2026 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 */

#include "macos_uds_vsock_path.h"

#include <cstdio>

namespace android {

std::string binderRpcVsockHostPath(unsigned int bind_cid, unsigned int port) {
    char buf[256];
    // Sun path limit is ~104 bytes; this pattern stays well below.
    std::snprintf(buf, sizeof(buf), "/tmp/binder_rpc_vsock_%u_%u.sock", bind_cid, port);
    return std::string(buf);
}

} // namespace android
