# CRE - Central Exchange
# Multi-stage build for minimal production image

# ============================================================================
# STAGE 1: Build
# ============================================================================
FROM ubuntu:22.04 AS builder

# Install build dependencies
RUN apt-get update && apt-get install -y     build-essential     cmake     libsqlite3-dev     && rm -rf /var/lib/apt/lists/*

# Copy source
WORKDIR /app
COPY . .

# Build with optimizations
RUN mkdir -p build && cd build     && cmake -DCMAKE_BUILD_TYPE=Release ..     && make -j

# ============================================================================
# STAGE 2: Runtime
# ============================================================================
FROM ubuntu:22.04-slim AS runtime

# Install runtime dependencies only
RUN apt-get update && apt-get install -y     libsqlite3-0     && rm -rf /var/lib/apt/lists/*

# Create non-root user
RUN useradd -m -s /bin/bash cre

# Copy binary from builder
WORKDIR /app
COPY --from=builder /app/build/central_exchange .
COPY --from=builder /app/data ./data

# Set ownership
RUN chown -R cre:cre /app

# Switch to non-root user
USER cre

# Expose HTTP port
EXPOSE 8080

# Health check
HEALTHCHECK --interval=30s --timeout=10s --retries=3     CMD curl -f http://localhost:8080/api/health || exit 1

# Run
CMD ["./central_exchange"]
