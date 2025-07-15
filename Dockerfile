FROM debian:sid-slim AS build

WORKDIR /build

# Install dependencies
RUN apt-get update && apt-get install -y \
    cmake git gcc g++ ninja-build \
    uuid-dev zlib1g-dev libc-ares-dev libbrotli-dev \
    postgresql-all libhiredis-dev libspdlog-dev libzip-dev \
    libcurl4-openssl-dev curl binutils

# Install Sentry CLI
RUN curl -sL https://sentry.io/get-cli/ | sh

# Build and install jsoncpp
RUN git clone https://github.com/open-source-parsers/jsoncpp && cd jsoncpp && git checkout 1.9.6 \
    && cmake -S . -B build -G "Ninja" -DJSONCPP_WITH_TESTS=OFF -DJSONCPP_WITH_EXAMPLE=OFF -DBUILD_SHARED_LIBS=OFF -DCMAKE_INSTALL_PREFIX:PATH=/usr \
    && cmake --build build --config Release --target install

# Build and install Drogon
RUN git clone https://github.com/drogonframework/drogon && cd drogon && git submodule update --init \
    && cmake -S . -B build -G "Ninja" -DBUILD_CTL=OFF -DBUILD_SHARED_LIBS=OFF -DBUILD_EXAMPLES=OFF -DCMAKE_CXX_STANDARD=20 \
    && cmake --build build --config Release --target install

# Build app
WORKDIR /build/src

COPY $PWD/.git /build/src/.git
COPY $PWD/builtin /build/src/builtin
COPY $PWD/modules /build/src/modules
COPY $PWD/src /build/src/src
COPY $PWD/CMakeLists.txt /build/src/

RUN cmake -S . -B build -G "Ninja" && cmake --build build --config RelWithDebInfo

# Extract debug info
RUN objcopy --only-keep-debug --compress-debug-sections=zlib build/bin/wiki_service build/bin/wiki_service.debug \
    && objcopy --strip-debug --strip-unneeded build/bin/wiki_service \
    && objcopy --add-gnu-debuglink=build/bin/wiki_service.debug build/bin/wiki_service

FROM debian:sid-slim AS main

WORKDIR /app

RUN apt-get update && apt-get install -y ca-certificates libspdlog-dev libc-ares-dev libfmt10 libbrotli-dev \
    libhiredis-dev libpq5 libzip-dev curl

COPY --from=build /build/src/build/bin/wiki_service /build/src/build/bin/crashpad_handler /app/

EXPOSE 8080

CMD ["./wiki_service"]
