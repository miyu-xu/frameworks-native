#!/bin/bash

set -e

# cc_cmake_snapshot with include_sources pulls all source files, including *.zip and build/ in this directory
git clean -fdx

${ANDROID_BUILD_TOP}/build/soong/soong_ui.bash --make-mode binder_sdk
cp ${ANDROID_BUILD_TOP}/out/soong/.intermediates/frameworks/native/libs/binder/binder_sdk/*/*/binder_sdk.zip ./binder_sdk.zip
unzip -q -o binder_sdk.zip -d build

cmake \
  -G Ninja \
  -DANDROID_BUILD_TOP=$ANDROID_BUILD_TOP \
  -B build \
  build
cmake --build build

if [ "$DO_TEST" == "1" ]; then
  if ! lsmod | grep vsock_loopback &> /dev/null ; then
    sudo modprobe vsock_loopback
  fi

  cd build/frameworks/native/libs/binder/tests/
  ./binderRpcTestNoKernel
fi
