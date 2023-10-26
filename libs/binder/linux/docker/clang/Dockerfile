FROM debian:bookworm

RUN echo 'deb http://deb.debian.org/debian bookworm-backports main' >> /etc/apt/sources.list && \
    apt-get update -y && \
    apt-get install -y clang cmake ninja-build libgtest-dev unzip

ADD linux-binder.zip /
RUN unzip -q -d linux-binder linux-binder.zip

WORKDIR /linux-binder
RUN CC=clang CXX=clang++ /linux-binder/frameworks/native/libs/binder/linux/docker/build.sh

WORKDIR /linux-binder/build/frameworks/native/libs/binder/tests/
ENTRYPOINT [ "/linux-binder/build/frameworks/native/libs/binder/tests/binderRpcTest" ]
