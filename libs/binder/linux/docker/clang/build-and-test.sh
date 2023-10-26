#!/bin/bash

set -e

#${ANDROID_BUILD_TOP}/build/soong/soong_ui.bash --make-mode binder_cmake_tstlib
cp ${ANDROID_BUILD_TOP}/out/soong/.intermediates/frameworks/native/libs/binder/binder_cmake_tstlib/*/*/binder_cmake_tstlib.zip ./linux-binder.zip

docker build -t linux-binder .
#docker run -it linux-binder
