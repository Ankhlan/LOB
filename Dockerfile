# Central Exchange - C++ Build for Railway v5
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

# Use mirrors.edge.kernel.org for better connectivity (avoid archive.ubuntu.com issues)
RUN sed -i 's|http://archive.ubuntu.com|http://mirrors.edge.kernel.org|g' /etc/apt/sources.list && \
    sed -i 's|http://security.ubuntu.com|http://mirrors.edge.kernel.org|g' /etc/apt/sources.list && \
    apt-get update && apt-get install -y cmake g++ git libssl-dev libsqlite3-dev && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY central_exchange/ ./central_exchange/

WORKDIR /app/central_exchange
# DEV_MODE enabled for testing - remove for production
RUN mkdir -p build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-DDEV_MODE" && make -j$(nproc)

FROM ubuntu:22.04
# Use same mirror as builder
RUN sed -i 's|http://archive.ubuntu.com|http://mirrors.edge.kernel.org|g' /etc/apt/sources.list && \
    sed -i 's|http://security.ubuntu.com|http://mirrors.edge.kernel.org|g' /etc/apt/sources.list
# Note: ledger-cli install may fail due to network issues with libicu70
# Build without ledger for now - ledger files are included in repo
RUN apt-get update && \
    apt-get install -y --no-install-recommends libssl3 curl libsqlite3-0 && \
    rm -rf /var/lib/apt/lists/*
COPY --from=builder /app/central_exchange/build/central_exchange /usr/local/bin/
# Copy ledger files for accounting
COPY ledger/ /app/ledger/

ENV PORT=8080
EXPOSE 8080
CMD central_exchange --port $PORT