# Central Exchange - Mongolia
# C++ Exchange with FXCM Integration
# Multi-stage build for Railway deployment

FROM ubuntu:22.04 AS builder

# Install build dependencies
RUN apt-get update && apt-get install -y \
    cmake \
    g++ \
    libssl-dev \
    git \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Create app directory
WORKDIR /app

# Copy only central_exchange source
COPY central_exchange/ ./central_exchange/

# Build the exchange
WORKDIR /app/central_exchange
RUN mkdir -p build && cd build && \
    cmake .. -DBUILD_WITH_FXCM=OFF -DCMAKE_BUILD_TYPE=Release && \
    cmake --build . --config Release -j$(nproc)

# Runtime image (minimal)
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    libssl3 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Create non-root user
RUN useradd -m exchange
USER exchange

# Copy binary
COPY --from=builder /app/central_exchange/build/central_exchange /usr/local/bin/

# Railway sets PORT env var
ENV PORT=8080

EXPOSE 8080

# Health check
HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD curl -f http://localhost:$PORT/health || exit 1

CMD ["sh", "-c", "central_exchange --port $PORT"]
