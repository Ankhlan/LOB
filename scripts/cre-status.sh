#!/bin/bash
# =============================================================================
# CRE Exchange - Status Script
# =============================================================================
# Shows the status of the Central Exchange server
# Usage: ./cre-status.sh
# =============================================================================

TMUX_SESSION="cre"

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
CYAN='\033[0;36m'
NC='\033[0m'

echo -e "${CYAN}╔═══════════════════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║           CRE Exchange - Status                          ║${NC}"
echo -e "${CYAN}╚═══════════════════════════════════════════════════════════╝${NC}"
echo ""

# Check tmux session
if tmux has-session -t "$TMUX_SESSION" 2>/dev/null; then
    echo -e "${GREEN}✓ Server Status: RUNNING${NC}"
    echo ""
    
    # Show session info
    echo -e "${CYAN}Session Info:${NC}"
    tmux list-sessions -F "  Session: #{session_name} | Windows: #{session_windows} | Created: #{session_created_string}" 2>/dev/null | grep "$TMUX_SESSION"
    echo ""
    
    # Check if port is listening
    if netstat -tuln 2>/dev/null | grep -q ":8080 "; then
        echo -e "${GREEN}✓ Port 8080: LISTENING${NC}"
    elif ss -tuln 2>/dev/null | grep -q ":8080 "; then
        echo -e "${GREEN}✓ Port 8080: LISTENING${NC}"
    else
        echo -e "${YELLOW}⚠ Port 8080: Unable to verify${NC}"
    fi
    
    # Show last few lines of output
    echo ""
    echo -e "${CYAN}Recent Output:${NC}"
    tmux capture-pane -t "$TMUX_SESSION" -p | tail -10
else
    echo -e "${RED}✗ Server Status: STOPPED${NC}"
    echo ""
    echo -e "  Start with: ${GREEN}./cre-start.sh${NC}"
fi
