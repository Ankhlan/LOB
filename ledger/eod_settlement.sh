#!/bin/bash
# ═══════════════════════════════════════════════════════════════════════════
# CRE.mn End-of-Day Settlement Report
# Daily reconciliation and settlement summary
# Created: 2026-02-10 by LEDGER
# ═══════════════════════════════════════════════════════════════════════════
#
# Usage:
#   ./eod_settlement.sh                    # Today's settlement
#   ./eod_settlement.sh 2026-02-09         # Specific date
#   ./eod_settlement.sh --csv              # CSV output for eBarimt
#   ./eod_settlement.sh --csv 2026-02-09   # CSV for specific date
#
# Requires: ledger-cli 3.x, sqlite3
# ═══════════════════════════════════════════════════════════════════════════

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
LEDGER_FILE="${LEDGER_FILE:-$SCRIPT_DIR/master.journal}"
DB_FILE="${DB_FILE:-$SCRIPT_DIR/../data/exchange.db}"
LEDGER_CMD="ledger -f $LEDGER_FILE"

# Parse arguments
CSV_MODE=false
DATE=$(date +%Y-%m-%d)
for arg in "$@"; do
    case "$arg" in
        --csv) CSV_MODE=true ;;
        20[0-9][0-9]-[0-9][0-9]-[0-9][0-9]) DATE="$arg" ;;
    esac
done

# Ledger date format uses / not -
LEDGER_DATE=$(echo "$DATE" | tr '-' '/')

# Verify dependencies
if ! command -v ledger &>/dev/null; then
    echo "ERROR: ledger-cli not found. Install with: apt install ledger"
    exit 1
fi

if [ ! -f "$LEDGER_FILE" ]; then
    echo "ERROR: Journal not found: $LEDGER_FILE"
    exit 1
fi

# ─────────────────────────────────────────────────────────────────────────
# CSV OUTPUT (for eBarimt / tax reporting)
# ─────────────────────────────────────────────────────────────────────────
if [ "$CSV_MODE" = true ]; then
    echo "date,category,subcategory,amount_mnt,currency"

    # Spread revenue
    spread=$($LEDGER_CMD -b "$LEDGER_DATE" -e "$LEDGER_DATE" register Revenue:Trading:Spread --amount-data 2>/dev/null | awk '{sum+=$1} END {printf "%.2f", sum+0}')
    echo "$DATE,revenue,spread,$spread,MNT"

    # Fee revenue
    fees=$($LEDGER_CMD -b "$LEDGER_DATE" -e "$LEDGER_DATE" register Revenue:Trading:Fees --amount-data 2>/dev/null | awk '{sum+=$1} END {printf "%.2f", sum+0}')
    echo "$DATE,revenue,fees,$fees,MNT"

    # Funding revenue (collected from users)
    funding_rev=$($LEDGER_CMD -b "$LEDGER_DATE" -e "$LEDGER_DATE" register Revenue:Funding --amount-data 2>/dev/null | awk '{sum+=$1} END {printf "%.2f", sum+0}')
    echo "$DATE,revenue,funding_collected,$funding_rev,MNT"

    # Funding expense (paid to users)
    funding_exp=$($LEDGER_CMD -b "$LEDGER_DATE" -e "$LEDGER_DATE" register Expenses:Funding --amount-data 2>/dev/null | awk '{sum+=$1} END {printf "%.2f", sum+0}')
    echo "$DATE,expense,funding_paid,$funding_exp,MNT"

    # Net funding
    net_funding=$(echo "$funding_rev $funding_exp" | awk '{printf "%.2f", $1 - $2}')
    echo "$DATE,net,funding,$net_funding,MNT"

    # Insurance fund contributions
    ins_contrib=$($LEDGER_CMD -b "$LEDGER_DATE" -e "$LEDGER_DATE" register Revenue:Insurance:Contributions --amount-data 2>/dev/null | awk '{sum+=$1} END {printf "%.2f", sum+0}')
    echo "$DATE,insurance,contributions,$ins_contrib,MNT"

    # Insurance fund payouts (liquidations)
    ins_payout=$($LEDGER_CMD -b "$LEDGER_DATE" -e "$LEDGER_DATE" register Expenses:Insurance:Liquidation --amount-data 2>/dev/null | awk '{sum+=$1} END {printf "%.2f", sum+0}')
    echo "$DATE,insurance,liquidation_payouts,$ins_payout,MNT"

    # Insurance fund balance
    ins_balance=$($LEDGER_CMD balance Assets:Exchange:InsuranceFund --amount-data 2>/dev/null | awk '{sum+=$1} END {printf "%.2f", sum+0}')
    echo "$DATE,insurance,fund_balance,$ins_balance,MNT"

    # VAT accrued
    vat=$($LEDGER_CMD -b "$LEDGER_DATE" -e "$LEDGER_DATE" register Expenses:Taxes:VAT --amount-data 2>/dev/null | awk '{sum+=$1} END {printf "%.2f", sum+0}')
    echo "$DATE,tax,vat_accrued,$vat,MNT"

    # Total P&L
    total_pnl=$(echo "$spread $fees $funding_rev $funding_exp $ins_contrib $vat" | awk '{printf "%.2f", $1 + $2 + $3 - $4 + $5 - $6}')
    echo "$DATE,net,daily_pnl,$total_pnl,MNT"

    exit 0
fi

# ─────────────────────────────────────────────────────────────────────────
# LEDGER-CLI FORMAT REPORT
# ─────────────────────────────────────────────────────────────────────────
cat << EOF
═══════════════════════════════════════════════════════════════════════════
 CENTRAL RETAIL EXCHANGE LLC (CRE.mn)
 END-OF-DAY SETTLEMENT REPORT
 Date: $DATE
 Generated: $(date '+%Y-%m-%d %H:%M:%S %Z')
 (In Mongolian Tugrik — MNT)
═══════════════════════════════════════════════════════════════════════════

1. DAILY TRADING REVENUE
─────────────────────────────────────────────────────────────────────────
EOF

echo "  Spread Revenue:"
$LEDGER_CMD -b "$LEDGER_DATE" -e "$LEDGER_DATE" balance Revenue:Trading:Spread -X MNT 2>/dev/null | sed 's/^/    /' || echo "    (none)"
echo ""
echo "  Trading Fees:"
$LEDGER_CMD -b "$LEDGER_DATE" -e "$LEDGER_DATE" balance Revenue:Trading:Fees -X MNT 2>/dev/null | sed 's/^/    /' || echo "    (none)"
echo ""

cat << EOF

2. FUNDING PAYMENTS SUMMARY
─────────────────────────────────────────────────────────────────────────
EOF

echo "  Funding Collected (from position holders):"
$LEDGER_CMD -b "$LEDGER_DATE" -e "$LEDGER_DATE" balance Revenue:Funding -X MNT 2>/dev/null | sed 's/^/    /' || echo "    (none)"
echo ""
echo "  Funding Paid (to position holders):"
$LEDGER_CMD -b "$LEDGER_DATE" -e "$LEDGER_DATE" balance Expenses:Funding -X MNT 2>/dev/null | sed 's/^/    /' || echo "    (none)"
echo ""
echo "  Net Funding P&L:"
$LEDGER_CMD -b "$LEDGER_DATE" -e "$LEDGER_DATE" balance Revenue:Funding Expenses:Funding -X MNT 2>/dev/null | sed 's/^/    /' || echo "    (none)"
echo ""

cat << EOF

3. INSURANCE FUND
─────────────────────────────────────────────────────────────────────────
EOF

echo "  Contributions (20% of revenue):"
$LEDGER_CMD -b "$LEDGER_DATE" -e "$LEDGER_DATE" balance Revenue:Insurance:Contributions -X MNT 2>/dev/null | sed 's/^/    /' || echo "    (none)"
echo ""
echo "  Liquidation Payouts:"
$LEDGER_CMD -b "$LEDGER_DATE" -e "$LEDGER_DATE" balance Expenses:Insurance:Liquidation -X MNT 2>/dev/null | sed 's/^/    /' || echo "    (none)"
echo ""
echo "  Current Fund Balance:"
$LEDGER_CMD balance Assets:Exchange:InsuranceFund -X MNT 2>/dev/null | sed 's/^/    /' || echo "    (none)"
echo ""

cat << EOF

3.5 VAT ACCRUAL (НӨАТ)
─────────────────────────────────────────────────────────────────────────
EOF

echo "  VAT Expense (10% of revenue):"
$LEDGER_CMD -b "$LEDGER_DATE" -e "$LEDGER_DATE" balance Expenses:Taxes:VAT -X MNT 2>/dev/null | sed 's/^/    /' || echo "    (none)"
echo ""
echo "  VAT Payable (cumulative):"
$LEDGER_CMD balance Liabilities:Taxes:VAT -X MNT 2>/dev/null | sed 's/^/    /' || echo "    (none)"
echo ""

cat << EOF

4. DAILY NET P&L
─────────────────────────────────────────────────────────────────────────
EOF

echo "  All Revenue:"
$LEDGER_CMD -b "$LEDGER_DATE" -e "$LEDGER_DATE" balance Revenue -X MNT 2>/dev/null | sed 's/^/    /' || echo "    (none)"
echo ""
echo "  All Expenses:"
$LEDGER_CMD -b "$LEDGER_DATE" -e "$LEDGER_DATE" balance Expenses -X MNT 2>/dev/null | sed 's/^/    /' || echo "    (none)"
echo ""
echo "  ═══════════════════════════════════════"
echo "  NET PROFIT / (LOSS):"
$LEDGER_CMD -b "$LEDGER_DATE" -e "$LEDGER_DATE" balance Revenue Expenses -X MNT 2>/dev/null | sed 's/^/    /' || echo "    (none)"
echo ""

cat << EOF

5. CUMULATIVE BALANCES
─────────────────────────────────────────────────────────────────────────
EOF

echo "  Exchange Cash:"
$LEDGER_CMD balance Assets:Exchange:Cash Assets:Bank -X MNT 2>/dev/null | sed 's/^/    /' || echo "    (none)"
echo ""
echo "  Customer Balances Owed:"
$LEDGER_CMD balance Liabilities:Customer -X MNT --depth 2 2>/dev/null | sed 's/^/    /' || echo "    (none)"
echo ""
echo "  FXCM Hedge Account:"
$LEDGER_CMD balance Assets:Hedge -X MNT 2>/dev/null | sed 's/^/    /' || echo "    (none)"
echo ""

cat << EOF

6. VERIFICATION
─────────────────────────────────────────────────────────────────────────
  Books Balance Check (must equal 0):
EOF
$LEDGER_CMD balance -X MNT 2>/dev/null | sed 's/^/    /' || echo "    (empty - no transactions)"

cat << EOF

═══════════════════════════════════════════════════════════════════════════
 END OF REPORT
═══════════════════════════════════════════════════════════════════════════
EOF
