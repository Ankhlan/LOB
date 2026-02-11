#!/bin/bash
# ═══════════════════════════════════════════════════════════════════════════
# CRE.mn Monthly Closing Process
# Closes Revenue/Expense accounts to Equity:CurrentPL
# НББОУС (Mongolian Accounting Standards) compliant
#
# Usage:
#   ./monthly_close.sh                    # Current month
#   ./monthly_close.sh 2026-01            # Specific month
#   ./monthly_close.sh 2026-01 --execute  # Actually write entries
#
# Without --execute, produces a DRY RUN preview only.
# Created: 2026-01-29 by LEDGER
# ═══════════════════════════════════════════════════════════════════════════

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
LEDGER_FILE="${LEDGER_FILE:-$SCRIPT_DIR/master.journal}"
CLOSING_DIR="${CLOSING_DIR:-$SCRIPT_DIR/../data/ledger}"
LEDGER_CMD="ledger -f $LEDGER_FILE"

# Parse arguments
EXECUTE=false
MONTH=$(date +%Y-%m)
for arg in "$@"; do
    case "$arg" in
        --execute) EXECUTE=true ;;
        20[0-9][0-9]-[0-9][0-9]) MONTH="$arg" ;;
    esac
done

BEGIN="${MONTH}-01"
# End of month (next month 1st)
END=$(date -d "$BEGIN + 1 month" '+%Y-%m-%d' 2>/dev/null || \
     date -v+1m -jf '%Y-%m-%d' "$BEGIN" '+%Y-%m-%d' 2>/dev/null || \
     echo "${MONTH}-31")
CLOSE_DATE=$(date -d "$END - 1 day" '+%Y/%m/%d' 2>/dev/null || echo "${MONTH}/28")

# Verify dependencies
if ! command -v ledger &>/dev/null; then
    echo "ERROR: ledger-cli not found. Install with: apt install ledger"
    exit 1
fi

if [ ! -f "$LEDGER_FILE" ]; then
    echo "ERROR: Journal not found: $LEDGER_FILE"
    exit 1
fi

cat << EOF
═══════════════════════════════════════════════════════════════════════════
 CENTRAL RETAIL EXCHANGE LLC (CRE.mn)
 MONTHLY CLOSING ENTRIES — $MONTH
 Date: $CLOSE_DATE
 Mode: $([ "$EXECUTE" = true ] && echo "EXECUTE — writing to closing.ledger" || echo "DRY RUN — preview only")
═══════════════════════════════════════════════════════════════════════════

1. PRE-CLOSE TRIAL BALANCE
─────────────────────────────────────────────────────────────────────────
EOF

echo "Revenue accounts (period):"
$LEDGER_CMD -b "$BEGIN" -e "$END" balance Revenue --depth 3 -X MNT 2>/dev/null | sed 's/^/  /' || echo "  (none)"
echo ""
echo "Expense accounts (period):"
$LEDGER_CMD -b "$BEGIN" -e "$END" balance Expenses --depth 3 -X MNT 2>/dev/null | sed 's/^/  /' || echo "  (none)"
echo ""
echo "Net Income for period:"
$LEDGER_CMD -b "$BEGIN" -e "$END" balance Revenue Expenses -X MNT 2>/dev/null | sed 's/^/  /' || echo "  (none)"
echo ""

cat << EOF

2. CLOSING ENTRIES
─────────────────────────────────────────────────────────────────────────
EOF

# Generate closing entries
# Revenue accounts: DR Revenue / CR Equity:CurrentPL
# Expense accounts: DR Equity:CurrentPL / CR Expenses
CLOSING_ENTRIES=""

echo "  Revenue → Equity:CurrentPL:"
# Get each Revenue sub-account balance
while IFS= read -r line; do
    amount=$(echo "$line" | grep -oP '[\-]?[\d,]+\.?\d*(?=\s+MNT)' | tr -d ',')
    account=$(echo "$line" | grep -oP 'Revenue:\S+')
    if [ -n "$amount" ] && [ -n "$account" ]; then
        # Revenue has credit (negative) normal balance; to close, debit it (make positive)
        close_amt=$(echo "$amount" | sed 's/^-//' )
        echo "    DR $account  $close_amt MNT"
        echo "    CR Equity:CurrentPL  -$close_amt MNT"
        CLOSING_ENTRIES="${CLOSING_ENTRIES}
$CLOSE_DATE * Monthly close - $account
    $account    $close_amt MNT
    Equity:CurrentPL    -$close_amt MNT
"
    fi
done < <($LEDGER_CMD -b "$BEGIN" -e "$END" balance Revenue --flat -X MNT 2>/dev/null)

echo ""
echo "  Expenses → Equity:CurrentPL:"
while IFS= read -r line; do
    amount=$(echo "$line" | grep -oP '[\-]?[\d,]+\.?\d*(?=\s+MNT)' | tr -d ',')
    account=$(echo "$line" | grep -oP 'Expenses:\S+')
    if [ -n "$amount" ] && [ -n "$account" ]; then
        # Expenses have debit (positive) normal balance; to close, credit them (make negative)
        echo "    CR $account  -$amount MNT"
        echo "    DR Equity:CurrentPL  $amount MNT"
        CLOSING_ENTRIES="${CLOSING_ENTRIES}
$CLOSE_DATE * Monthly close - $account
    $account    -$amount MNT
    Equity:CurrentPL    $amount MNT
"
    fi
done < <($LEDGER_CMD -b "$BEGIN" -e "$END" balance Expenses --flat -X MNT 2>/dev/null)

echo ""

if [ "$EXECUTE" = true ]; then
    CLOSING_FILE="$CLOSING_DIR/closing.ledger"
    echo "$CLOSING_ENTRIES" >> "$CLOSING_FILE"
    echo "  ✅ Closing entries written to: $CLOSING_FILE"
    echo ""
    
    cat << EOF

3. POST-CLOSE VERIFICATION
─────────────────────────────────────────────────────────────────────────
EOF
    echo "Revenue (should be 0):"
    $LEDGER_CMD balance Revenue -X MNT 2>/dev/null | sed 's/^/  /' || echo "  0"
    echo ""
    echo "Expenses (should be 0):"
    $LEDGER_CMD balance Expenses -X MNT 2>/dev/null | sed 's/^/  /' || echo "  0"
    echo ""
    echo "Equity:CurrentPL (accumulated P&L):"
    $LEDGER_CMD balance Equity:CurrentPL -X MNT 2>/dev/null | sed 's/^/  /' || echo "  0"
else
    echo "  ℹ️  DRY RUN — no entries written."
    echo "  To execute: $0 $MONTH --execute"
fi

cat << EOF

═══════════════════════════════════════════════════════════════════════════
 END OF MONTHLY CLOSE — $MONTH
═══════════════════════════════════════════════════════════════════════════
EOF
