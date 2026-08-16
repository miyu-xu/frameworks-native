# Cross-platform binder-rpc host build (CMake). Android uses Soong (Android.bp).

set(BINDER_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# 平台检测
if(WIN32)
    set(PLATFORM_WINDOWS 1)
    set(PLATFORM_NAME "Windows")
    add_definitions(-D_WIN32_WINNT=0x0601)  # Windows 7+
    add_definitions(-DBINDER_NO_LIBBASE=1)
elseif(APPLE)
    set(PLATFORM_MACOS 1)
    set(PLATFORM_NAME "macOS")
else()
    set(PLATFORM_LINUX 1)
    set(PLATFORM_NAME "Linux")
endif()

# 编译器特定设置
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wno-pedantic -Wno-error=pedantic -Wno-error=format -Wno-error=sign-compare -Wno-error=missing-field-initializers -Wno-error=return-type)
endif()

# 配置输出目录
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 查找依赖
find_package(Threads REQUIRED)

# Windows特定依赖
if(PLATFORM_WINDOWS)
    set(WINDOWS_LIBS ws2_32 advapi32)
endif()

# 包含目录
include_directories(
    ${CMAKE_SOURCE_DIR}
    ${BINDER_DIR}
    ${BINDER_DIR}/platform
    ${BINDER_DIR}/platform/host_stubs
    ${BINDER_DIR}/include
    ${BINDER_DIR}/include_rpc_unstable
    ${BINDER_DIR}/liblog_stub/include
    ${CMAKE_SOURCE_DIR}/system/core/libutils/binder/include
    ${CMAKE_SOURCE_DIR}/system/core/libutils/include
    ${CMAKE_SOURCE_DIR}/system/core/libcutils/include
    ${CMAKE_SOURCE_DIR}/system/core/libsystem/include
)

# 为Android日志系统创建兼容性定义
add_definitions(-DLOG_TAG="binder-rpc")

# 核心RPC源文件
set(RPC_CORE_SOURCES
    ${BINDER_DIR}/RpcServer.cpp
    ${BINDER_DIR}/RpcSession.cpp
    ${BINDER_DIR}/RpcState.cpp
    ${BINDER_DIR}/RpcTransportRaw.cpp
    ${BINDER_DIR}/libbinder_rpc_unstable.cpp
)

set(IPC_THREAD_STATE_SOURCE ${BINDER_DIR}/IPCThreadState.cpp)
if(PLATFORM_MACOS)
    set(IPC_THREAD_STATE_SOURCE ${BINDER_DIR}/platform/IPCThreadState_host.cpp)
endif()

set(IRESULT_RECEIVER_SOURCE ${BINDER_DIR}/IResultReceiver.cpp)
if(PLATFORM_WINDOWS)
    set(IRESULT_RECEIVER_SOURCE ${BINDER_DIR}/platform/IResultReceiver_windows.cpp)
endif()

# Android Binder核心组件源文件
set(BINDER_CORE_SOURCES
    ${CMAKE_SOURCE_DIR}/system/core/libutils/binder/RefBase.cpp
    ${CMAKE_SOURCE_DIR}/system/core/libutils/binder/String8.cpp
    ${CMAKE_SOURCE_DIR}/system/core/libutils/binder/StrongPointer.cpp
    ${CMAKE_SOURCE_DIR}/system/core/libutils/binder/Errors.cpp
    ${CMAKE_SOURCE_DIR}/system/core/libutils/binder/VectorImpl.cpp
    ${CMAKE_SOURCE_DIR}/system/core/libutils/binder/String16.cpp
    ${CMAKE_SOURCE_DIR}/system/core/libutils/binder/Unicode.cpp
    ${CMAKE_SOURCE_DIR}/system/core/libutils/binder/SharedBuffer.cpp
    ${CMAKE_SOURCE_DIR}/system/core/libutils/Threads.cpp
    ${BINDER_DIR}/Parcel.cpp
    ${BINDER_DIR}/BpBinder.cpp
    ${BINDER_DIR}/BufferedTextOutput.cpp
    ${IPC_THREAD_STATE_SOURCE}
    ${BINDER_DIR}/FdTrigger.cpp
    ${BINDER_DIR}/IInterface.cpp
    ${IRESULT_RECEIVER_SOURCE}
    ${BINDER_DIR}/Binder.cpp
    ${BINDER_DIR}/Stability.cpp
    ${BINDER_DIR}/Static.cpp
    ${BINDER_DIR}/Status.cpp
    ${BINDER_DIR}/TextOutput.cpp
    ${BINDER_DIR}/Utils.cpp
    ${BINDER_DIR}/Debug.cpp
    ${BINDER_DIR}/file.cpp
    ${BINDER_DIR}/ProcessState.cpp
)

# Unix socket + RPC helpers (Linux and macOS host builds)
set(OS_UNIX_COMMON_SOURCES ${BINDER_DIR}/OS_unix_base.cpp)

# 平台特定源文件
if(PLATFORM_WINDOWS)
    set(PLATFORM_SOURCES
        ${BINDER_DIR}/platform/OS_windows.cpp
        ${BINDER_DIR}/platform/windows_stubs.cpp
        ${BINDER_DIR}/platform/IResultReceiver_windows.cpp
        ${BINDER_DIR}/platform/Pipe_windows.cpp
        ${BINDER_DIR}/platform/namedpipe_vsock.cpp
        ${BINDER_DIR}/platform/namedpipe_rpc_transport.cpp
    )
elseif(PLATFORM_MACOS)
    set(PLATFORM_SOURCES
        ${BINDER_DIR}/platform/OS_macos.cpp
        ${BINDER_DIR}/platform/macos_stubs.cpp
        ${BINDER_DIR}/platform/macos_uds_vsock_path.cpp
        ${OS_UNIX_COMMON_SOURCES}
    )
else()
    set(PLATFORM_SOURCES
        ${BINDER_DIR}/OS_non_android_linux.cpp
        ${OS_UNIX_COMMON_SOURCES}
    )
endif()

# libbinder_ndk RPC相关接口库
set(BINDER_NDK_RPC_SOURCES
    ${BINDER_DIR}/ndk/ibinder.cpp
    ${BINDER_DIR}/ndk/libbinder.cpp
    ${BINDER_DIR}/ndk/parcel.cpp
    ${BINDER_DIR}/ndk/process.cpp
    ${BINDER_DIR}/ndk/status.cpp
    ${BINDER_DIR}/ndk/stability.cpp
)

# 创建binder-rpc库
add_library(binder-rpc SHARED
    ${RPC_CORE_SOURCES}
    ${BINDER_CORE_SOURCES}
    ${PLATFORM_SOURCES}
    ${STRING16_SOURCES}
    ${BINDER_NDK_RPC_SOURCES}
)

# 设置库属性
set_target_properties(binder-rpc PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    OUTPUT_NAME "binder-rpc"
)

# 链接库
target_link_libraries(binder-rpc
    PRIVATE Threads::Threads
)

if(PLATFORM_WINDOWS)
    target_link_libraries(binder-rpc PRIVATE ${WINDOWS_LIBS})
    target_compile_definitions(binder-rpc PRIVATE _WIN32_WINNT=0x0601)
endif()

# 包含目录
target_include_directories(binder-rpc
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
        $<BUILD_INTERFACE:${BINDER_DIR}>
        $<BUILD_INTERFACE:${BINDER_DIR}/platform>
        $<$<BOOL:${PLATFORM_WINDOWS}>:$<BUILD_INTERFACE:${BINDER_DIR}/platform/win32>>
        $<BUILD_INTERFACE:${BINDER_DIR}/include>
        $<BUILD_INTERFACE:${BINDER_DIR}/ndk/include_ndk>
        $<BUILD_INTERFACE:${BINDER_DIR}/ndk/include_cpp>
        $<BUILD_INTERFACE:${BINDER_DIR}/ndk/include_platform>
    PRIVATE
        ${BINDER_DIR}
        ${BINDER_DIR}/platform/host_stubs
        ${CMAKE_SOURCE_DIR}/system/core/libutils/binder/include
        ${CMAKE_SOURCE_DIR}/system/core/libutils/include
        ${CMAKE_SOURCE_DIR}/system/core/libcutils/include
        ${CMAKE_SOURCE_DIR}/system/core/libsystem/include
        ${BINDER_DIR}/liblog_stub/include
)

# 编译定义
target_compile_definitions(binder-rpc
    PRIVATE
        $<$<BOOL:${PLATFORM_WINDOWS}>:PLATFORM_WINDOWS=1>
        $<$<BOOL:${PLATFORM_LINUX}>:PLATFORM_LINUX=1>
        $<$<BOOL:${PLATFORM_MACOS}>:PLATFORM_MACOS=1>
        BINDER_NO_LIBBASE=1
        LOG_NDEBUG=0
        __LP64__
        $<$<BOOL:${PLATFORM_WINDOWS}>:BYTE_ORDER_BIG_ENDIAN=1>
        $<$<BOOL:${PLATFORM_WINDOWS}>:BYTE_ORDER_LITTLE_ENDIAN=0>
)

# 安装目标
install(TARGETS binder-rpc
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

# 安装头文件
install(DIRECTORY ${BINDER_DIR}/platform/
    DESTINATION include/platform
    FILES_MATCHING PATTERN "*.h"
)

install(DIRECTORY ${BINDER_DIR}/include/
    DESTINATION include/binder
    FILES_MATCHING PATTERN "*.h"
)

# 包配置
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/binder-rpc-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
    ${BINDER_DIR}/binder-rpc-config.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/binder-rpc-config.cmake"
    INSTALL_DESTINATION lib/cmake/binder-rpc
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/binder-rpc-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/binder-rpc-config-version.cmake"
    DESTINATION lib/cmake/binder-rpc
)

# 测试目标（可选）
if(BUILD_TESTING)
    # --- RPC 最小 smoke test ---
    # 利用 Unix socket pair 在同一个进程中建立 RPC server+client 并发送 ping 事务。
    add_executable(rpc_smoke_test tests/host/rpc_smoke_test.cpp)
    target_link_libraries(rpc_smoke_test PRIVATE binder-rpc)
    target_include_directories(rpc_smoke_test PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${BINDER_DIR}
        ${BINDER_DIR}/include
        ${CMAKE_SOURCE_DIR}/system/core/libutils/include
        ${CMAKE_SOURCE_DIR}/system/core/libutils/binder/include
        ${CMAKE_SOURCE_DIR}/system/core/libcutils/include
    )
    target_compile_definitions(rpc_smoke_test PRIVATE -DBUILD_TESTING -DBINDER_NO_LIBBASE=1)
    add_test(NAME rpc_smoke_test COMMAND rpc_smoke_test)

    # --- 符号导出验证测试 ---
    # 使用 nm (或 dumpbin) 检查 libbinder-rpc 是否导出了所有预期符号。
    # 仅在 Unix 平台上通过 shell 实现；Windows 上可使用 dumpbin 或跳过。
    if(NOT PLATFORM_WINDOWS)
        set(SYMBOL_CHECK_SCRIPT ${BINDER_DIR}/tests/host/check_symbols.sh)
        add_test(
            NAME symbol_check
            COMMAND ${CMAKE_COMMAND} -E env
                bash ${SYMBOL_CHECK_SCRIPT}
                "$<TARGET_FILE:binder-rpc>"
        )
        set_tests_properties(symbol_check PROPERTIES
            DEPENDS binder-rpc
        )
    endif()
endif()

# 输出配置信息
message(STATUS "Building for platform: ${PLATFORM_NAME}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
