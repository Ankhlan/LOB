#!/bin/bash
# =============================================================================
# CRE Exchange - Start Script
# =============================================================================
# Starts the Central Exchange server in a background tmux session
# Usage: ./cre-start.sh [--attach]
#
# Options:
#   --attach    Attach to the tmux session after starting
# =============================================================================

set -e

# Configuration
TMUX_SESSION="cre"
CRE_DIR="/mnt/c/workshop/repo/LOB/build"
FXCM_LIB="../deps/fxcm-linux/ForexConnectAPI-1.6.5-Linux-x86_64/lib"

# FXCM Credentials (Real account)
export FXCM_USER="8000065022"
export FXCM_PASS="Meniscus_321957"
export FXCM_CONNECTION="Real"
export ADMIN_API_KEY="cre2026admin"

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "${GREEN}╔═══════════════════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║           CRE Exchange - Startup Script                  ║${NC}"
echo -e "${GREEN}╚═══════════════════════════════════════════════════════════╝${NC}"
echo ""

# Check if tmux session already exists
if tmux has-session -t "$TMUX_SESSION" 2>/dev/null; then
    echo -e "${YELLOW}⚠ tmux session '$TMUX_SESSION' already exists${NC}"
    echo -e "  Use: ${GREEN}tmux attach -t $TMUX_SESSION${NC} to attach"
    echo -e "  Use: ${RED}tmux kill-session -t $TMUX_SESSION${NC} to stop"
    exit 0
fi

# Check if binary exists
if [ ! -f "$CRE_DIR/central_exchange" ]; then
    echo -e "${RED}✗ Binary not found at $CRE_DIR/central_exchange${NC}"
    echo -e "  Build with: cd $CRE_DIR && cmake .. && make -j8"
    exit 1
fi

# Create tmux session in background
echo -e "${GREEN}→ Starting CRE Exchange in tmux session '$TMUX_SESSION'...${NC}"

tmux new-session -d -s "$TMUX_SESSION" "cd $CRE_DIR && LD_LIBRARY_PATH=$FXCM_LIB FXCM_USER=$FXCM_USER FXCM_PASS=$FXCM_PASS FXCM_CONNECTION=$FXCM_CONNECTION ADMIN_API_KEY=$ADMIN_API_KEY ./central_exchange; exec bash"

# Wait for server to start
sleep 2

# Check if server started
if tmux has-session -t "$TMUX_SESSION" 2>/dev/null; then
    echo -e "${GREEN}✓ CRE Exchange started successfully!${NC}"
    echo ""
    echo -e "  Web UI:     ${GREEN}http://localhost:8080${NC}"
    echo -e "  WebSocket:  ${GREEN}ws://localhost:8080/ws${NC}"
    echo ""
    echo -e "  Commands:"
    echo -e "    Attach:   ${GREEN}tmux attach -t $TMUX_SESSION${NC}"
    echo -e "    Logs:     ${GREEN}tmux capture-pane -t $TMUX_SESSION -p${NC}"
    echo -e "    Stop:     ${RED}tmux kill-session -t $TMUX_SESSION${NC}"
    echo ""
    
    # Attach if requested
    if [ "$1" == "--attach" ]; then
        echo -e "${YELLOW}→ Attaching to session...${NC}"
        tmux attach -t "$TMUX_SESSION"
    fi
else
    echo -e "${RED}✗ Failed to start CRE Exchange${NC}"
    exit 1
fi
