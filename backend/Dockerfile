FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive
ENV VCPKG_ROOT=/opt/vcpkg

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    cmake \
    g++ \
    git \
    make \
    ninja-build \
    pkg-config \
    curl \
    zip \
    unzip \
    tar \
    wget \
    libssl-dev \
    libcurl4-openssl-dev \
    bison \
    flex \
    libtool \
    autoconf \
    python3 \
    && rm -rf /var/lib/apt/lists/*

RUN git clone https://github.com/microsoft/vcpkg.git ${VCPKG_ROOT} \
    && ${VCPKG_ROOT}/bootstrap-vcpkg.sh

WORKDIR /app

# Copy manifest file first to leverage Docker layer caching for dependencies
COPY vcpkg.json .

# Pre-build dependencies using vcpkg manifest mode
RUN ${VCPKG_ROOT}/vcpkg install

COPY . .

RUN cmake -B build -S . \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake
RUN cmake --build build --config Release

EXPOSE 8080

CMD ["./build/proactive_it_dashboard"]
