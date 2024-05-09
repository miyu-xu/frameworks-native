FROM debian:bookworm

RUN echo 'deb http://deb.debian.org/debian bookworm-backports main' >> /etc/apt/sources.list && \
    apt-get update -y && \
    apt-get install -y clang cmake ninja-build libgtest-dev unzip

ADD binder_sdk.zip /
RUN unzip -q -d binder_sdk binder_sdk.zip

WORKDIR /binder_sdk
RUN CC=clang CXX=clang++ cmake -G Ninja -B build .
RUN cmake --build build

WORKDIR /binder_sdk/build/frameworks/native/libs/binder/tests/
ENTRYPOINT [ "/binder_sdk/build/frameworks/native/libs/binder/tests/binderRpcTestNoKernel" ]
