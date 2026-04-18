/*
 * Copyright (C) 2024 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

use std::env;
use std::path::PathBuf;

fn main() {
    // Get the current directory (crate root)
    let crate_root = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    // Try to find AOSP root by going up several directories
    // From: frameworks/native/libs/binder/rust/rpcbinder
    // We want to go to: ../../../../.. (5 levels up)
    let aosp_root = crate_root
        .parent().and_then(|p| p.parent()) // rust
        .and_then(|p| p.parent()) // rpcbinder
        .and_then(|p| p.parent()) // binder
        .and_then(|p| p.parent()) // libs
        .and_then(|p| p.parent()) // native
        .and_then(|p| p.parent()) // frameworks
        .map(|p| p.to_path_buf());
    
    let aosp_root = aosp_root.unwrap_or_else(|| PathBuf::from("."));
    
    // Start building bindgen command
    let mut builder = bindgen::Builder::default();
    
    // Add include paths based on user's request
    // Note: On Windows, we avoid adding platform directory to prevent conflicts
    // with Windows SDK headers (e.g., cmsghdr redefinition)
    let target_os = env::var("CARGO_CFG_TARGET_OS").unwrap_or_default();
    
    // Only add platform directory on non-Windows targets
    if target_os != "windows" {
        // 1. platform directory
        let platform_path = aosp_root.join("platform");
        if platform_path.exists() {
            builder = builder.clang_arg(format!("-I{}", platform_path.display()));
        }
    }
    
    // 2. platform/win32 directory (only on Windows, but we're skipping it to avoid conflicts)
    // We'll rely on Windows SDK headers instead
    
    // 3. frameworks/native/libs/binder/include
    let binder_include_path = aosp_root.join("frameworks/native/libs/binder/include");
    if binder_include_path.exists() {
        builder = builder.clang_arg(format!("-I{}", binder_include_path.display()));
    }
    
    // 4. frameworks/native/libs/binder/ndk/include_ndk
    let ndk_include_ndk_path = aosp_root.join("frameworks/native/libs/binder/ndk/include_ndk");
    if ndk_include_ndk_path.exists() {
        builder = builder.clang_arg(format!("-I{}", ndk_include_ndk_path.display()));
    }
    
    // 5. frameworks/native/libs/binder/ndk/include_cpp
    let ndk_include_cpp_path = aosp_root.join("frameworks/native/libs/binder/ndk/include_cpp");
    if ndk_include_cpp_path.exists() {
        builder = builder.clang_arg(format!("-I{}", ndk_include_cpp_path.display()));
    }
    
    // 6. frameworks/native/libs/binder/ndk/include_platform
    let ndk_include_platform_path = aosp_root.join("frameworks/native/libs/binder/ndk/include_platform");
    if ndk_include_platform_path.exists() {
        builder = builder.clang_arg(format!("-I{}", ndk_include_platform_path.display()));
    }
    
    // 7. frameworks/native/libs/binder/include_rpc_unstable
    let binder_include_rpc_unstable_path = aosp_root.join("frameworks/native/libs/binder/include_rpc_unstable");
    if binder_include_rpc_unstable_path.exists() {
        builder = builder.clang_arg(format!("-I{}", binder_include_rpc_unstable_path.display()));
    }
    
    // Generate bindings
    let bindings = builder
        .header("BinderBindings.hpp")
        .parse_callbacks(Box::new(bindgen::CargoCallbacks::new()))
        // Keep in sync with Android.bp bindgen_flags
        .blocklist_type("AIBinder")
        .raw_line("use binder_ndk_sys::AIBinder;")
        .rustified_enum("ARpcSession_FileDescriptorTransportMode")
        .allowlist_type("ARpcSession")
        .allowlist_type("ARpcServer")
        .allowlist_function("ARpcSession_.*")
        .allowlist_function("ARpcServer_.*")
        .generate()
        .expect("Couldn't generate bindings");
    
    let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());
    bindings.write_to_file(out_path.join("bindings.rs")).expect("Couldn't write bindings.");
    let sys_libs_dir = aosp_root.join("frameworks/native/libs/binder/rust/sys/libs");
    println!("cargo:rustc-link-search=native={}", sys_libs_dir.display());
    println!("cargo:rustc-link-lib=dylib=binder-rpc");
}
