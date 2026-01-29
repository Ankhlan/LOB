# CRE Build Instructions

## Prerequisites
- C++17 compiler (GCC 9+ or Clang 10+)
- CMake 3.16+
- Linux (WSL2 supported)

## Quick Start
`ash
cd central_exchange
mkdir -p build && cd build
cmake ..
make -j
./central_exchange
`

## Dependencies (header-only, included)
- cpp-httplib (HTTP server)
- nlohmann/json (JSON)
- SQLite3 (system)

## System Dependencies
Ubuntu: pt install build-essential cmake libsqlite3-dev

## Build Types
- Debug: cmake -DCMAKE_BUILD_TYPE=Debug ..
- Release: cmake -DCMAKE_BUILD_TYPE=Release ..

## Running
Server starts on http://localhost:8080
Use tmux: 	mux new-session -d -s cre "./central_exchange"

## Stress Test
`ash
cd test
g++ -std=c++17 -O3 -I../src stress_test.cpp -o stress_test -lpthread
./stress_test 100000
`

## Docker
`dockerfile
FROM ubuntu:22.04
RUN apt-get update && apt-get install -y build-essential cmake libsqlite3-dev
COPY . /app
WORKDIR /app/build
RUN cmake .. && make -j
CMD ["./central_exchange"]
`
