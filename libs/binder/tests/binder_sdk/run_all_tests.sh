#!/bin/bash

set -ex

BUILD_DIR=$1
SERVICEMANAGER_DIR=${BUILD_DIR}/frameworks/native/cmds/servicemanager
BINDER_DIR=${BUILD_DIR}/frameworks/native/libs/binder
RED='\033[1;31m'
GREEN='\033[1;32m'
NO_COLOR='\033[0m'

if [ ! -d "${BINDER_DIR}" ]; then
    echo -e "${RED}Invalid build dir. Please provide it as the first argument.${NO_COLOR}"
    exit -1
fi

${SERVICEMANAGER_DIR}/servicemanager &
SERVICEMANAGER_PID=$!
function cleanup {
    kill $SERVICEMANAGER_PID
}
trap cleanup EXIT

${BINDER_DIR}/tests/binderClearBufTest
${BINDER_DIR}/tests/binderDriverInterfaceTestSdk
${BINDER_DIR}/tests/binderParcelBenchmark
${BINDER_DIR}/tests/binderUnitTest
${BINDER_DIR}/tests/binderLibTestSdk
${BINDER_DIR}/tests/binderStabilityTestSdk
${BINDER_DIR}/tests/binderRpcWireProtocolTest
${BINDER_DIR}/ndk/tests/libbinder_ndk_unit_testSdk
${BINDER_DIR}/tests/binderRpcTest
# Skipped for time sake
#${BINDER_DIR}/tests/binderRpcTestNoKernel
#${BINDER_DIR}/tests/binderRpcTestSingleThreaded
#${BINDER_DIR}/tests/binderRpcTestSingleThreadedNoKernel
${BINDER_DIR}/tests/RpcTlsUtilsTest
${BINDER_DIR}/tests/binderThroughputTest

echo -e "${GREEN}All tests succeeded${NO_COLOR}"
