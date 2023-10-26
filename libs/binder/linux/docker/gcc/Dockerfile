FROM gcc:9

RUN echo 'deb http://deb.debian.org/debian bullseye-backports main' >> /etc/apt/sources.list && \
    apt-get update -y && \
    apt-get install -y python3 golang-1.19 cmake ninja-build libgtest-dev wget unzip
ENV PATH="/usr/lib/go-1.19/bin:$PATH"

ADD linux-binder.zip /
RUN unzip -d linux-binder linux-binder.zip

WORKDIR /linux-binder
RUN CC=gcc CXX=g++ /linux-binder/frameworks/native/libs/binder/linux/docker/build.sh

WORKDIR /linux-binder/build/frameworks/native/libs/binder/tests/
ENTRYPOINT [ "/linux-binder/build/frameworks/native/libs/binder/tests/binderRpcTestSdk" ]
