#!/bin/bash

set -e

# cc_cmake_snapshot with include_sources pulls all source files, including *.zip and build/ in this directory
git clean -fdx

${ANDROID_BUILD_TOP}/build/soong/soong_ui.bash --make-mode binder_cmake_tstlib
cp ${ANDROID_BUILD_TOP}/out/soong/.intermediates/frameworks/native/libs/binder/binder_cmake_tstlib/*/*/binder_cmake_tstlib.zip ./binder_cmake_tstlib.zip
unzip -q -o binder_cmake_tstlib.zip -d build

export CC=gcc
export CXX=g++
#export CC=clang
#export CXX=clang++

# you can skip "-G Ninja" and use "make" instead of "cmake --build build" for GNU experience
cmake \
  -G Ninja \
  -DANDROID_BUILD_TOP=$ANDROID_BUILD_TOP \
  -B build \
  build
cmake --build build

if [ "$DO_TEST" == "1" ]; then
  cd build/frameworks/native/libs/binder/tests/
  ./binderRpcTestNoKernel
fi
