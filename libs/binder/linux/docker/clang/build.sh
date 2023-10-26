#!/bin/bash

set -e

#${ANDROID_BUILD_TOP}/build/soong/soong_ui.bash --make-mode binder_sdk
cp ${ANDROID_BUILD_TOP}/out/soong/.intermediates/frameworks/native/libs/binder/binder_sdk/*/*/binder_sdk.zip ./binder_sdk.zip

docker build -t binder_sdk .
if [ "$DO_TEST" == "1" ]; then
  docker run -it binder_sdk
fi
