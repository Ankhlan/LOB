#!/bin/bash
# =============================================================================
# CRE Exchange - Stop Script
# =============================================================================
# Stops the Central Exchange server tmux session
# Usage: ./cre-stop.sh
# =============================================================================

TMUX_SESSION="cre"

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

if tmux has-session -t "$TMUX_SESSION" 2>/dev/null; then
    echo -e "${YELLOW}→ Stopping CRE Exchange...${NC}"
    tmux kill-session -t "$TMUX_SESSION"
    echo -e "${GREEN}✓ CRE Exchange stopped${NC}"
else
    echo -e "${YELLOW}⚠ No CRE session running${NC}"
fi
