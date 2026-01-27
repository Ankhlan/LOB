# Central Exchange - C++ Build for Railway v4
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y cmake g++ git libssl-dev libsqlite3-dev && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY central_exchange/ ./central_exchange/

WORKDIR /app/central_exchange
# Force rebuild by changing this comment: BUILD_V4_20260127_1640
RUN mkdir -p build && cd build && cmake .. -DCMAKE_BUILD_TYPE=Release && make -j$(nproc)

FROM ubuntu:22.04
RUN apt-get update && apt-get install -y libssl3 curl libsqlite3-0 && rm -rf /var/lib/apt/lists/*
COPY --from=builder /app/central_exchange/build/central_exchange /usr/local/bin/

ENV PORT=8080
EXPOSE 8080
CMD central_exchange --port $PORT
