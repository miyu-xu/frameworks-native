#!/bin/bash

set -ex

TEST_NAME="$(basename "$0")"
DOCKER_TAG="${TEST_NAME}-${RANDOM}${RANDOM}"
DOCKER_FILE=*.Dockerfile
DOCKER_RUN_FLAGS=

# Guess if we're running as an Android test or directly
if [ "$(ls -1 ${DOCKER_FILE} | wc -l)" == "1" ]; then
    # likely running as `atest binder_sdk_docker_test_XYZ`
    DOCKER_PATH="$(dirname $(readlink --canonicalize --no-newline binder_sdk.zip))"
else
    # likely running directly as `./binder_sdk_docker_test.sh` - provide mode for easy testing
    RED='\033[1;31m'
    NO_COLOR='\033[0m'

    if ! modinfo vsock_loopback &>/dev/null ; then
        echo -e "${RED}Module vsock_loopback is not installed.${NO_COLOR}"
        exit 1
    fi
    if modprobe --dry-run --first-time vsock_loopback &>/dev/null ; then
        echo "Module vsock_loopback is not loaded. Attempting to load..."
        if ! sudo modprobe vsock_loopback ; then
            echo -e "${RED}Module vsock_loopback is not loaded and attempt to load failed.${NO_COLOR}"
            exit 1
        fi
    fi

    DOCKER_RUN_FLAGS="--interactive --tty"

    DOCKER_FILE="$1"
    if [ ! -f "${DOCKER_FILE}" ]; then
        echo -e "${RED}Docker file '${DOCKER_FILE}' doesn't exist. Please provide one as an argument.${NO_COLOR}"
        exit 1
    fi

    if [ ! -d "${ANDROID_BUILD_TOP}" ]; then
        echo -e "${RED}ANDROID_BUILD_TOP doesn't exist. Please lunch some target.${NO_COLOR}"
        exit 1
    fi
    ${ANDROID_BUILD_TOP}/build/soong/soong_ui.bash --make-mode binder_sdk
    BINDER_SDK_ZIP="${ANDROID_BUILD_TOP}/out/soong/.intermediates/frameworks/native/libs/binder/binder_sdk/*/binder_sdk.zip"
    DOCKER_PATH="$(dirname $(ls -1 ${BINDER_SDK_ZIP} | head --lines=1))"
fi

function cleanup {
    docker rmi --force "${DOCKER_TAG}" 2>/dev/null || true
}
trap cleanup EXIT

docker build --force-rm --tag "${DOCKER_TAG}" --file ${DOCKER_FILE} ${DOCKER_PATH}
docker run ${DOCKER_RUN_FLAGS} --rm "${DOCKER_TAG}"
