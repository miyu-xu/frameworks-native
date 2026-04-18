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
     // Tree root: directory that contains `frameworks/native/...` (AOSP workspace root).
     // From `.../binder/rust/sys`: ancestors are sys,rust,binder,libs,native,frameworks,<root>.
     let aosp_root = crate_root
         .ancestors()
         .nth(6)
         .map(|p| p.to_path_buf())
         .unwrap_or_else(|| PathBuf::from("."));
     
     // Start building bindgen command
     let mut builder = bindgen::Builder::default();
     
     // Add include paths based on user's request
     let target_os = env::var("CARGO_CFG_TARGET_OS").unwrap_or_default();

     // 1. platform/win32 first on Windows (sys/cdefs.h uid_t/pid_t before NDK headers).
     if target_os == "windows" {
         let platform_win32_path = aosp_root.join("frameworks/native/libs/binder/platform/win32");
         if platform_win32_path.exists() {
             builder = builder.clang_arg(format!("-I{}", platform_win32_path.display()));
         }
     }

     // 2. Host platform stubs (named pipes, fcntl_windows.h, etc.)
     let binder_platform_path = aosp_root.join("frameworks/native/libs/binder/platform");
     if binder_platform_path.exists() {
         builder = builder.clang_arg(format!("-I{}", binder_platform_path.display()));
     }
     
     // 3. libbinder include (public API)
     let binder_include_path = aosp_root.join("frameworks/native/libs/binder/include");
     if binder_include_path.exists() {
         builder = builder.clang_arg(format!("-I{}", binder_include_path.display()));
     }
     
     // 4. ndk/include_ndk
     let ndk_include_ndk_path = aosp_root.join("frameworks/native/libs/binder/ndk/include_ndk");
     if ndk_include_ndk_path.exists() {
         builder = builder.clang_arg(format!("-I{}", ndk_include_ndk_path.display()));
     }
     
     // 5. ndk/include_cpp
     let ndk_include_cpp_path = aosp_root.join("frameworks/native/libs/binder/ndk/include_cpp");
     if ndk_include_cpp_path.exists() {
         builder = builder.clang_arg(format!("-I{}", ndk_include_cpp_path.display()));
     }
     
     // 6. ndk/include_platform
     let ndk_include_platform_path = aosp_root.join("frameworks/native/libs/binder/ndk/include_platform");
     if ndk_include_platform_path.exists() {
         builder = builder.clang_arg(format!("-I{}", ndk_include_platform_path.display()));
     }
     
     // Generate bindings
     let bindings = builder
         // TODO figure out what the "standard" #define is and use that instead
         .header("BinderBindings.hpp")
         .parse_callbacks(Box::new(bindgen::CargoCallbacks::new()))
         // Keep in sync with libbinder_ndk_bindgen_flags.txt
         .default_enum_style(bindgen::EnumVariation::Rust { non_exhaustive: true })
         .constified_enum("android::c_interface::consts::.*")
         .allowlist_type("android::c_interface::.*")
         .allowlist_type("AStatus")
         .allowlist_type("AIBinder_Class")
         .allowlist_type("AIBinder")
         .allowlist_type("AIBinder_Weak")
         .allowlist_type("AIBinder_DeathRecipient")
         .allowlist_type("AParcel")
         .allowlist_type("binder_status_t")
         .blocklist_function("vprintf")
         .blocklist_function("strtold")
         .blocklist_function("_vtlog")
         .blocklist_function("vscanf")
         .blocklist_function("vfprintf_worker")
         .blocklist_function("vsprintf")
         .blocklist_function("vsnprintf")
         .blocklist_function("vsnprintf_filtered")
         .blocklist_function("vfscanf")
         .blocklist_function("vsscanf")
         .blocklist_function("vdprintf")
         .blocklist_function("vasprintf")
         .blocklist_function("strtold_l")
         .allowlist_function(".*")
         .generate()
         .expect("Couldn't generate bindings");
     
     let out_path = PathBuf::from(env::var("OUT_DIR").unwrap());
     bindings.write_to_file(out_path.join("bindings.rs")).expect("Couldn't write bindings.");
    let libs_dir = crate_root.join("libs");
    println!("cargo:rustc-link-search=native={}", libs_dir.display());
    println!("cargo:rustc-link-lib=dylib=binder-rpc");
 }
 
