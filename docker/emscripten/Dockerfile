FROM emscripten/emsdk:latest AS builder

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

COPY ISLE/emscripten/libwasmfs_fetch.js.patch /tmp/
RUN cd /emsdk/upstream/emscripten && \
    git apply --check /tmp/libwasmfs_fetch.js.patch && \
    git apply /tmp/libwasmfs_fetch.js.patch

COPY --chown=emscripten:emscripten . .

RUN emcmake cmake -S . -B build -DISLE_BUILD_CONFIG=OFF -DISLE_DEBUG=OFF -DCMAKE_BUILD_TYPE=Release -DISLE_EMSCRIPTEN_HOST=/assets && \
    emmake cmake --build build -j 32

RUN echo "Fetching isle.pizza frontend..."; \
    git clone --depth 1 https://github.com/isledecomp/isle.pizza /tmp/isle.pizza && \
    cp -r /tmp/isle.pizza/* /src/build/ && \
    rm -rf /tmp/isle.pizza;

FROM nginx:alpine

COPY docker/emscripten/nginx.conf /etc/nginx/nginx.conf
COPY --from=builder /src/build /usr/share/nginx/html
EXPOSE 6931
