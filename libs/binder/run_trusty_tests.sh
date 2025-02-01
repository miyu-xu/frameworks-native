#!/bin/bash
#
# Copyright (C) 2025 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

if [[ -z ${ANDROID_BUILD_TOP} ]]; then
  echo "You need to source and lunch before you can use this script"
  exit 1
fi

set -e

ANDROID_TARGET=qemu_trusty_arm64-trunk_staging-userdebug
TRUSTY_TARGET=qemu-generic-arm64-gicv3-spd-noffa-test-debug

source ${ANDROID_BUILD_TOP}/build/envsetup.sh
lunch ${ANDROID_TARGET}
m droid aprotoc trusty_metrics_atoms_protoc_plugin
${ANDROID_BUILD_TOP}/trusty/vendor/google/aosp/scripts/build.py --test "binder" ${TRUSTY_TARGET}
