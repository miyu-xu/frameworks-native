# Android native framework subset

[简体中文](README.zh-CN.md) | English

This BSCP branch carries the Android native framework subset required by the cross-platform host,
primarily binder RPC and its portable socket/OS compatibility layer. The repository remains based
on the pinned Android platform baseline; BSCP-specific changes are kept as a short series above
that baseline.

Build it through the BSCP root `build_all.sh` or `build_all.bat` entry point. Generated Rust import
libraries and host DLLs under `libs/binder/rust/sys/libs/` are build outputs and are not committed.
Linux is the reference binder implementation; macOS and Windows adapters must preserve protocol,
ownership, error, and shutdown semantics.
