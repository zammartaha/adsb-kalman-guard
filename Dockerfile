# ==========================================
# Stage 1: Build the C++17 Kalman Engine
# ==========================================
FROM debian:bookworm-slim AS cpp-builder

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    git \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY CMakeLists.txt ./
COPY backend/ ./backend/

RUN cmake -B build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build -j$(nproc)

# ==========================================
# Stage 2: Node.js WebSocket Bridge + C++ Binary
# ==========================================
FROM node:22-bookworm-slim AS runtime

WORKDIR /app

# Copy compiled C++ binary to /app/build/kalman_server (matches bridge/server.js)
COPY --from=cpp-builder /src/build/kalman_server ./build/kalman_server

# Install Node bridge dependencies
COPY bridge/package*.json ./bridge/
RUN cd bridge && npm ci --omit=dev

# Copy bridge source and sample data
COPY bridge/server.js ./bridge/server.js
COPY sample_data/ ./sample_data/

EXPOSE 8080

CMD ["node", "bridge/server.js"]