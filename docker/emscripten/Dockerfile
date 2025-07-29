FROM emscripten/emsdk:4.0.10 AS builder

ARG CMAKE_VERSION=3.29.3

WORKDIR /src

USER root

RUN apt-get update && apt-get install -y git wget && rm -rf /var/lib/apt/lists/*
RUN wget https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-Linux-x86_64.sh -O /tmp/cmake.sh && \
    chmod +x /tmp/cmake.sh && \
    /tmp/cmake.sh --skip-license --prefix=/usr/local && \
    rm /tmp/cmake.sh

RUN chown -R emscripten:emscripten /src

USER emscripten

COPY ISLE/emscripten/emscripten.patch /tmp/
RUN cd /emsdk/upstream/emscripten && \
    git apply --check /tmp/emscripten.patch && \
    git apply /tmp/emscripten.patch

COPY --chown=emscripten:emscripten 3rdparty/ ./3rdparty/
COPY --chown=emscripten:emscripten LEGO1/ ./LEGO1/
COPY --chown=emscripten:emscripten ISLE/ ./ISLE/
COPY --chown=emscripten:emscripten miniwin/ ./miniwin/
COPY --chown=emscripten:emscripten util/ ./util/
COPY --chown=emscripten:emscripten CMake/ ./CMake/
COPY --chown=emscripten:emscripten packaging/ ./packaging/
COPY --chown=emscripten:emscripten extensions/ ./extensions/
COPY --chown=emscripten:emscripten CMakeLists.txt .

RUN emcmake cmake -S . -B build -DISLE_BUILD_CONFIG=OFF -DISLE_DEBUG=OFF -DCMAKE_BUILD_TYPE=Release -DISLE_EMSCRIPTEN_HOST=/assets && \
    emmake cmake --build build -j 32

RUN echo "Fetching isle.pizza frontend..."; \
    git clone --depth 1 https://github.com/isledecomp/isle.pizza /tmp/isle.pizza;

FROM openresty/openresty:alpine

COPY docker/emscripten/nginx.conf /usr/local/openresty/nginx/conf/nginx.conf
COPY --from=builder /tmp/isle.pizza /usr/local/openresty/nginx/html
COPY --from=builder /src/build/isle.* /usr/local/openresty/nginx/html
EXPOSE 6931
