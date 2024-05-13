FROM debian:bookworm

RUN echo 'deb http://deb.debian.org/debian bookworm-backports main' >> /etc/apt/sources.list && \
    apt-get update -y && \
    apt-get install -y clang cmake ninja-build libgtest-dev libgmock-dev unzip

ADD binder_sdk.zip /
RUN unzip -q -d binder_sdk binder_sdk.zip

WORKDIR /binder_sdk
RUN CC=clang CXX=clang++ cmake -G Ninja -B build -DBENCHMARK_ENABLE_TESTING:BOOL=OFF .
RUN cmake --build build

ENTRYPOINT [ \
    "/bin/bash", \
    "/binder_sdk/frameworks/native/libs/binder/tests/binder_sdk/run_all_tests.sh", \
    "/binder_sdk/build" \
]
