# Android native framework 子集

简体中文 | [English](README.md)

本 BSCP 分支保存跨平台主机需要的 Android native framework 子集，重点是 binder RPC 及其
可移植 Socket/操作系统兼容层。仓库基于 manifest 固定的 Android 平台基线，BSCP 修改以
短提交序列维护在基线之上。

请通过 BSCP 根目录 `build_all.sh` 或 `build_all.bat` 构建。`libs/binder/rust/sys/libs/`
下生成的 Rust import library 与主机 DLL 属于构建产物，不得提交。Linux 是 binder 参考实现；
macOS 与 Windows 适配必须保持协议、所有权、错误和关闭语义一致。
