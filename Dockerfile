# syntax=docker/dockerfile:1

# ---- build stage ------------------------------------------------------------
FROM debian:bookworm AS build

RUN apt-get update \
 && apt-get install -y --no-install-recommends g++ cmake ninja-build ca-certificates \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY CMakeLists.txt CMakePresets.json ./
COPY cmake ./cmake
COPY src ./src

RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DHEARTH_BUILD_TESTS=OFF \
 && cmake --build build \
 && strip build/src/hearth

# ---- runtime stage ----------------------------------------------------------
FROM debian:bookworm-slim

RUN useradd --system --uid 10001 --no-create-home hearth

COPY --from=build /src/build/src/hearth /usr/local/bin/hearth
COPY public /srv/public

USER hearth
EXPOSE 8080
ENTRYPOINT ["/usr/local/bin/hearth"]
CMD ["--port", "8080", "--root", "/srv/public"]
