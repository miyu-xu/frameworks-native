#!/bin/bash

#
# Copyright (C) 2024 The Android Open Source Project
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

set -ex

BUILD_DIR=$1
SERVICEMANAGER_DIR=${BUILD_DIR}/frameworks/native/cmds/servicemanager

if [ ! -d "${SERVICEMANAGER_DIR}" ]; then
    RED='\033[1;31m'
    NO_COLOR='\033[0m'
    echo -e "${RED}Invalid build dir. Please provide it as the first argument.${NO_COLOR}"
    exit -1
fi

${SERVICEMANAGER_DIR}/servicemanager &
SERVICEMANAGER_PID=$!
function cleanup {
    kill $SERVICEMANAGER_PID
}
trap cleanup EXIT

cd $BUILD_DIR
ctest --parallel 32 --output-on-failure
