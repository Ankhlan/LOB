#!/bin/bash
# CRE.mn Startup Script with FXCM
# Run this from WSL to start the exchange server

CRE_ROOT="/mnt/c/workshop/repo/LOB"
FXCM_SDK_DIR="$CRE_ROOT/deps/fxcm-linux/ForexConnectAPI-1.6.5-Linux-x86_64"

# Check if binary exists
if [ ! -f "$CRE_ROOT/build/central_exchange" ]; then
    echo "❌ ERROR: Binary not found. Run build_cre.sh first!"
    exit 1
fi

# Check if FXCM is enabled
FXCM_STATUS=$(grep "FXCM_ENABLED:BOOL" "$CRE_ROOT/build/CMakeCache.txt" 2>/dev/null)
if [[ "$FXCM_STATUS" != *"ON"* ]]; then
    echo "⚠️  WARNING: FXCM is NOT enabled in this build!"
    echo "   Status: $FXCM_STATUS"
    echo "   Run build_cre.sh to rebuild with FXCM"
    echo ""
fi

# Set environment
export LD_LIBRARY_PATH="$FXCM_SDK_DIR/lib"
export FXCM_USER="${FXCM_USER:-8000065022}"
export FXCM_PASS="${FXCM_PASS:-Meniscus_321957}"
export FXCM_CONNECTION="${FXCM_CONNECTION:-Real}"
export ADMIN_API_KEY="${ADMIN_API_KEY:-cre2026admin}"

echo "=== CRE.mn Server ==="
echo "FXCM User: $FXCM_USER"
echo "Connection: $FXCM_CONNECTION"
echo "Admin Key: ${ADMIN_API_KEY:0:4}..."
echo ""

cd "$CRE_ROOT/build"
exec ./central_exchange
