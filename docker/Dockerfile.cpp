# ─── Stage 1: Build ───────────────────────────────────────────────────────────
FROM ubuntu:22.04 AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake g++ && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY cpp_core/ .

RUN cmake -B build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build --parallel "$(nproc)"

# ─── Stage 2: Runtime ─────────────────────────────────────────────────────────
FROM ubuntu:22.04 AS runtime

RUN apt-get update && apt-get install -y --no-install-recommends \
    libstdc++6 && \
    rm -rf /var/lib/apt/lists/*

COPY --from=builder /src/build/quarkcache_server /usr/local/bin/quarkcache_server

EXPOSE 9000
ENTRYPOINT ["quarkcache_server"]
