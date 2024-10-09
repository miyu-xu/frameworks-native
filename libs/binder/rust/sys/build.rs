use std::env;
use std::path::PathBuf;

fn main() {
    let ndk_home = PathBuf::from(env::var("ANDROID_NDK_HOME").unwrap());
    let toolchain = ndk_home.join("toolchains/llvm/prebuilt/linux-x86_64/");
    let sysroot = toolchain.join("sysroot");
    let bindings = bindgen::Builder::default()
        .clang_arg(format!("--sysroot={}", sysroot.display()))
        // TODO figure out what the "standard" #define is and use that instead
        .clang_arg("-DANDROID_NDK")
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
}
