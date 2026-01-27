# Central Exchange - Mongolia
# C++ Exchange with FXCM Integration

FROM ubuntu:22.04 AS builder

# Install build dependencies
RUN apt-get update && apt-get install -y \
    cmake \
    g++ \
    libssl-dev \
    git \
    && rm -rf /var/lib/apt/lists/*

# Copy source
COPY central_exchange /app/central_exchange

# Build
WORKDIR /app/central_exchange
RUN mkdir -p build && cd build && \
    cmake .. -DBUILD_WITH_FXCM=OFF -DCMAKE_BUILD_TYPE=Release && \
    cmake --build . --config Release

# Runtime image
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    libssl3 \
    && rm -rf /var/lib/apt/lists/*

# Copy binary
COPY --from=builder /app/central_exchange/build/central_exchange /usr/local/bin/

# Default port
ENV PORT=8080

EXPOSE 8080

CMD ["sh", "-c", "central_exchange --port $PORT"]
