FROM debian:sid-slim as build

WORKDIR /build

# Install dependencies
RUN apt-get update && apt-get install -y \
    cmake git gcc g++ ninja-build \
    uuid-dev zlib1g-dev libc-ares-dev libbrotli-dev \
    postgresql-all libhiredis-dev libspdlog-dev

# Build and install jsoncpp
RUN git clone https://github.com/open-source-parsers/jsoncpp && cd jsoncpp && git checkout 1.9.6 \
    && cmake -S . -B build -G "Ninja" -DJSONCPP_WITH_TESTS=OFF -DJSONCPP_WITH_EXAMPLE=OFF -DBUILD_SHARED_LIBS=OFF -DCMAKE_INSTALL_PREFIX:PATH=/usr \
    && cmake --build build --config Release --target install

# Build and install Drogon
RUN git clone https://github.com/drogonframework/drogon && cd drogon && git submodule update --init \
    && cmake -S . -B build -G "Ninja" -DBUILD_CTL=OFF -DBUILD_SHARED_LIBS=OFF -DBUILD_EXAMPLES=OFF -DCMAKE_CXX_STANDARD=20 \
    && cmake --build build --config Release --target install

WORKDIR /build

# Build app
WORKDIR /build/src

COPY $PWD/modules /build/src/modules
COPY $PWD/src /build/src/src
COPY $PWD/CMakeLists.txt /build/src/

RUN cmake -S . -B build -G "Ninja" && cmake --build build --config Release

FROM debian:sid-slim as main

WORKDIR /app

RUN apt-get update && apt-get install -y ca-certificates libspdlog1.12 libcares2 libfmt10 libbrotli1 libhiredis1.1.0 libpq5

COPY --from=build /build/src/build/bin/wiki_service /app

# Cleanup
RUN rm -rf -d /build

EXPOSE 8080

CMD ["./wiki_service"]
