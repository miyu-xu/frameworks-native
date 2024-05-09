FROM gcc:9

RUN echo 'deb http://deb.debian.org/debian bullseye-backports main' >> /etc/apt/sources.list && \
    apt-get update -y && \
    apt-get install -y python3 golang-1.19 cmake libgtest-dev wget unzip
ENV PATH="/usr/lib/go-1.19/bin:$PATH"

ADD binder_sdk.zip /
RUN unzip -q -d binder_sdk binder_sdk.zip

WORKDIR /binder_sdk
RUN cmake .
RUN make -j

WORKDIR /binder_sdk/frameworks/native/libs/binder/tests/
ENTRYPOINT [ "/binder_sdk/frameworks/native/libs/binder/tests/binderRpcTestNoKernel" ]
