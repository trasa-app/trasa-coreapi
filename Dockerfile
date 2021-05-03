# Copyright (C) Karim Agha - All Rights Reserved
# Unauthorized copying of this file, via any medium is strictly prohibited
# Proprietary and confidential. Authored by Karim Agha <karim@sentio.cloud>

# Build env prep stage
FROM ubuntu:20.04 AS build-deps

# Prepare environment and install prereqs
ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y \
  osmctools \
  cmake  \
  gcc-9 \
  g++-9 \
  gcc-10 \
  g++-10 \
  clang \
  libboost-all-dev \
  libssl-dev \
  zlib1g-dev \
  git \
  pkg-config \
  libbz2-dev \
  libxml2-dev \
  libzip-dev \
  lua5.2 \
  liblua5.2-dev \
  libtbb-dev \
  libsqlite3-dev \
  libcurl4-openssl-dev \
  libarchive-dev \
  curl \
  autoconf \
  automake \
  libtool \
  binutils \
  unzip \
  uuid-dev 

# install libosrm libraries and headers on build machine
RUN mkdir -p /deps && cd /deps && \
  git clone --single-branch --depth 1 --progress --verbose \
  https://github.com/Project-OSRM/osrm-backend.git --branch=v5.24.0 && \
  cd osrm-backend && mkdir build && cd build && \
  BUILD_TYPE=Release \
  ENABLE_ASSERTIONS=OFF \ 
  BUILD_TOOLS=OFF \
  CXXFLAGS="-DTBB_SUPPRESS_DEPRECATED_MESSAGES -w" \
  cmake .. && \
  make -j$(nproc) && \
  make install

# install amazon web services c++ sdk
RUN mkdir -p /deps && cd /deps && \
  git clone --single-branch --depth 1 \ 
  https://github.com/aws/aws-sdk-cpp.git --branch 1.8.0 --progress --verbose && \
  cd aws-sdk-cpp && mkdir sdk_build && cd sdk_build && \
  cmake ..  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_ONLY="dynamodb;logs;events;monitoring;sqs;s3" \
  -DBUILD_SHARED_LIBS=OFF \
  -DCPP_STANDARD=17 \
  -DCMAKE_CXX_STANDARD=17 \
  -DENABLE_UNITY_BUILD=ON \
  -DENABLE_TESTING=OFF && \
  make -j$(nproc) && make install

# install libtorch
RUN mkdir -p /deps/torch && cd /deps/torch && \
  wget -nv https://download.pytorch.org/libtorch/nightly/cpu/libtorch-cxx11-abi-shared-with-deps-latest.zip && \
  unzip libtorch-cxx11-abi-shared-with-deps-latest.zip && cd libtorch/ && \
  cp -R . /usr/local


# Product code compilation stage
FROM build-deps as build-code

# copy code to container
ADD . /code

# build the code
RUN cd /code && rm -rf build && mkdir build && cd build && \ 
  cmake .. && make turbo_server -j$(nproc)

# copy built targets 
RUN mkdir -p /app && \
  cp /code/build/product/turbo_server /app/turbo_server && \
  cp /code/build/product/config.prod.json /app/config.prod.json && \
  cp /code/build/product/config.dev.json /app/config.dev.json


# Data Import stage

# stripped of source code and all libraries needed for building the
# server code. Predownload map data locally to microservice image.

FROM ubuntu:20.04 AS import-data
ARG DEBIAN_FRONTEND=noninteractive
RUN apt -o Acquire::AllowInsecureRepositories=true \
  -o Acquire::AllowDowngradeToInsecureRepositories=true update
RUN apt-get --allow-unauthenticated install -y \
  libssl1.1 libcurl4 libtbb2 libarchive13 locales
RUN locale-gen en_US.UTF-8 && update-locale LANG=en_US.UTF-8
RUN ulimit -n 999999
COPY --from=build-code /app /app
COPY --from=build-code /deps/torch/libtorch /usr/local
ENTRYPOINT ["/app/turbo_server", "/app/config.prod.json" , "none"]


# Production stage - rpc role only
FROM import-data
ENTRYPOINT ["/app/turbo_server", "/app/config.prod.json" , "rpc"]