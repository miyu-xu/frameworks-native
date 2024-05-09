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
BINDER_DIR=${BUILD_DIR}/frameworks/native/libs/binder
RED='\033[1;31m'
GREEN='\033[1;32m'
NO_COLOR='\033[0m'

if [ ! -d "${BINDER_DIR}" ]; then
    echo -e "${RED}Invalid build dir. Please provide it as the first argument.${NO_COLOR}"
    exit -1
fi

${BINDER_DIR}/tests/binderRpcWireProtocolTest

# TODO: causes test timeout (runs for 2 minutes)
#${BINDER_DIR}/tests/binderRpcTestNoKernel
#${BINDER_DIR}/tests/binderRpcTestSingleThreadedNoKernel

echo -e "${GREEN}All tests succeeded${NO_COLOR}"
