#!/bin/bash
# CRE.mn Build Script with FXCM Integration
# Run this from WSL to ensure FXCM is properly enabled

set -e

CRE_ROOT="/mnt/c/workshop/repo/LOB"
FXCM_SDK_DIR="$CRE_ROOT/deps/fxcm-linux/ForexConnectAPI-1.6.5-Linux-x86_64"

echo "=== CRE.mn Build Script ==="
echo ""

# Step 1: Verify SDK exists
echo "[1/4] Checking FXCM SDK..."
if [ ! -f "$FXCM_SDK_DIR/include/ForexConnect.h" ]; then
    echo "❌ ERROR: FXCM SDK not found at $FXCM_SDK_DIR"
    echo ""
    echo "Looking for SDK elsewhere..."
    FOUND_SDK=$(find /mnt/c/workshop -name "ForexConnect.h" -path "*Linux*" 2>/dev/null | head -1)
    if [ -n "$FOUND_SDK" ]; then
        echo "Found SDK at: $FOUND_SDK"
        echo "Please move it to: $FXCM_SDK_DIR/"
    fi
    exit 1
fi
echo "✅ SDK found"

# Verify libraries exist
if [ ! -f "$FXCM_SDK_DIR/lib/libForexConnect.so" ]; then
    echo "❌ ERROR: libForexConnect.so not found"
    exit 1
fi
echo "✅ Libraries found"

# Step 2: Clean and create build directory
echo ""
echo "[2/4] Preparing build directory..."
cd "$CRE_ROOT"
rm -rf build
mkdir -p build
cd build
echo "✅ Build directory ready"

# Step 3: Configure with CMake
echo ""
echo "[3/4] Running CMake with FXCM_ENABLED=ON..."
cmake -DFXCM_ENABLED=ON ..

# Verify FXCM was enabled
FXCM_STATUS=$(grep "FXCM_ENABLED:BOOL" CMakeCache.txt)
if [[ "$FXCM_STATUS" != *"ON"* ]]; then
    echo "❌ ERROR: FXCM was not enabled!"
    echo "CMake says: $FXCM_STATUS"
    exit 1
fi
echo "✅ FXCM_ENABLED=ON confirmed"

# Step 4: Build
echo ""
echo "[4/4] Building..."
make -j$(nproc)

echo ""
echo "=== BUILD COMPLETE ==="
echo ""
echo "FXCM Status: ENABLED ✅"
echo ""
echo "To start the server:"
echo "  cd $CRE_ROOT/build"
echo "  export LD_LIBRARY_PATH=$FXCM_SDK_DIR/lib"
echo "  export FXCM_USER=8000065022"
echo "  export FXCM_PASS=Meniscus_321957"
echo "  export FXCM_CONNECTION=Real"
echo "  export ADMIN_API_KEY=cre2026admin"
echo "  ./central_exchange"
echo ""
