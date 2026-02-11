#!/bin/bash
# ═══════════════════════════════════════════════════════════════════════════
# CRE.mn Financial Statement Generator
# IFRS-Compliant Reports from ledger-cli
# Created: 2026-02-10 by LEDGER
# ═══════════════════════════════════════════════════════════════════════════
#
# Usage:
#   ./financial_statements.sh balance_sheet    # IFRS Balance Sheet
#   ./financial_statements.sh income           # Income Statement
#   ./financial_statements.sh cashflow         # Cash Flow Statement
#   ./financial_statements.sh trial_balance    # Trial Balance
#   ./financial_statements.sh full             # All statements
#
# Requires: ledger-cli 3.x installed
# ═══════════════════════════════════════════════════════════════════════════

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
LEDGER_FILE="${LEDGER_FILE:-$SCRIPT_DIR/master.journal}"
DATE=$(date +%Y-%m-%d)
CURRENCY="MNT"

# Verify ledger-cli is available
if ! command -v ledger &>/dev/null; then
    echo "ERROR: ledger-cli not found. Install with: apt install ledger"
    exit 1
fi

# Verify journal exists
if [ ! -f "$LEDGER_FILE" ]; then
    echo "ERROR: Journal not found: $LEDGER_FILE"
    exit 1
fi

LEDGER_CMD="ledger -f $LEDGER_FILE"

# ─────────────────────────────────────────────────────────────────────────
# IFRS Balance Sheet (IAS 1)
# ─────────────────────────────────────────────────────────────────────────
balance_sheet() {
    cat << EOF
═══════════════════════════════════════════════════════════════════════════
 CENTRAL RETAIL EXCHANGE LLC (CRE.mn)
 STATEMENT OF FINANCIAL POSITION
 As at: $DATE
 (In Mongolian Tugrik — MNT)
 Prepared in accordance with IFRS
═══════════════════════════════════════════════════════════════════════════

ASSETS (Хөрөнгө)
─────────────────────────────────────────────────────────────────────────
Current Assets:
EOF
    echo "  Cash and Bank Balances:"
    $LEDGER_CMD balance Assets:Exchange:Cash Assets:Bank --depth 3 -X $CURRENCY 2>/dev/null | sed 's/^/    /'
    echo ""
    echo "  Customer Margin Held (Restricted):"
    $LEDGER_CMD balance Assets:Margin --depth 3 -X $CURRENCY 2>/dev/null | sed 's/^/    /'
    echo ""
    echo "  Receivables:"
    $LEDGER_CMD balance Assets:Receivables --depth 3 -X $CURRENCY 2>/dev/null | sed 's/^/    /'
    echo ""

    echo "Non-Current Assets:"
    echo "  Hedging Instruments (FXCM):"
    $LEDGER_CMD balance Assets:Hedge --depth 3 -X $CURRENCY 2>/dev/null | sed 's/^/    /'
    echo ""

    echo "───────────────────────────────────"
    echo "TOTAL ASSETS:"
    $LEDGER_CMD balance Assets -X $CURRENCY 2>/dev/null | sed 's/^/  /'
    echo ""

    cat << EOF

LIABILITIES (Өр төлбөр)
─────────────────────────────────────────────────────────────────────────
Current Liabilities:
EOF
    echo "  Customer Balances Owed:"
    $LEDGER_CMD balance Liabilities:Customers --depth 3 -X $CURRENCY 2>/dev/null | sed 's/^/    /'
    echo ""
    echo "  Pending Withdrawals:"
    $LEDGER_CMD balance Liabilities:Pending --depth 3 -X $CURRENCY 2>/dev/null | sed 's/^/    /'
    echo ""
    echo "  Taxes Payable:"
    $LEDGER_CMD balance Liabilities:Taxes --depth 3 -X $CURRENCY 2>/dev/null | sed 's/^/    /'
    echo ""

    echo "───────────────────────────────────"
    echo "TOTAL LIABILITIES:"
    $LEDGER_CMD balance Liabilities -X $CURRENCY 2>/dev/null | sed 's/^/  /'
    echo ""

    cat << EOF

EQUITY (Эзний өмч)
─────────────────────────────────────────────────────────────────────────
EOF
    echo "  Share Capital:"
    $LEDGER_CMD balance Equity:Capital --depth 3 -X $CURRENCY 2>/dev/null | sed 's/^/    /'
    echo ""
    echo "  Retained Earnings:"
    $LEDGER_CMD balance Equity:Retained -X $CURRENCY 2>/dev/null | sed 's/^/    /'
    echo ""
    echo "  Current Period P&L:"
    $LEDGER_CMD balance Revenue Expenses -X $CURRENCY 2>/dev/null | sed 's/^/    /'
    echo ""

    echo "───────────────────────────────────"
    echo "TOTAL EQUITY:"
    $LEDGER_CMD balance Equity Revenue Expenses -X $CURRENCY 2>/dev/null | sed 's/^/  /'
    echo ""

    cat << EOF

═══════════════════════════════════════════════════════════════════════════
 VERIFICATION: Assets = Liabilities + Equity
EOF
    $LEDGER_CMD balance -X $CURRENCY 2>/dev/null | sed 's/^/ /'
    echo "═══════════════════════════════════════════════════════════════════════════"
}

# ─────────────────────────────────────────────────────────────────────────
# IFRS Income Statement (IAS 1 / IFRS 15)
# ─────────────────────────────────────────────────────────────────────────
income_statement() {
    cat << EOF
═══════════════════════════════════════════════════════════════════════════
 CENTRAL RETAIL EXCHANGE LLC (CRE.mn)
 STATEMENT OF PROFIT OR LOSS
 For the period ending: $DATE
 (In Mongolian Tugrik — MNT)
 Prepared in accordance with IFRS 15
═══════════════════════════════════════════════════════════════════════════

REVENUE (Орлого) — IFRS 15 Performance Obligations
─────────────────────────────────────────────────────────────────────────
EOF
    echo "  Trading Revenue (Spread Income):"
    $LEDGER_CMD balance Revenue:Trading --depth 3 -X $CURRENCY 2>/dev/null | sed 's/^/    /'
    echo ""
    echo "  Financing Revenue (Swap/Interest):"
    $LEDGER_CMD balance Revenue:Financing --depth 3 -X $CURRENCY 2>/dev/null | sed 's/^/    /'
    echo ""
    echo "  Hedging P&L (IFRS 9):"
    $LEDGER_CMD balance Revenue:Hedging --depth 3 -X $CURRENCY 2>/dev/null | sed 's/^/    /'
    echo ""
    echo "  Funding Revenue (Collected):"
    $LEDGER_CMD balance Revenue:Funding --depth 3 -X $CURRENCY 2>/dev/null | sed 's/^/    /'
    echo ""
    echo "  Insurance Contributions:"
    $LEDGER_CMD balance Revenue:Insurance --depth 3 -X $CURRENCY 2>/dev/null | sed 's/^/    /'
    echo ""
    echo "  Other Revenue:"
    $LEDGER_CMD balance Revenue:Other --depth 3 -X $CURRENCY 2>/dev/null | sed 's/^/    /'
    echo ""

    echo "───────────────────────────────────"
    echo "TOTAL REVENUE:"
    $LEDGER_CMD balance Revenue -X $CURRENCY 2>/dev/null | sed 's/^/  /'
    echo ""

    cat << EOF

EXPENSES (Зардал)
─────────────────────────────────────────────────────────────────────────
EOF
    echo "  Hedging Costs (FXCM):"
    $LEDGER_CMD balance Expenses:Hedging --depth 3 -X $CURRENCY 2>/dev/null | sed 's/^/    /'
    echo ""
    echo "  Customer Payouts (Trading Losses):"
    $LEDGER_CMD balance Expenses:Trading --depth 3 -X $CURRENCY 2>/dev/null | sed 's/^/    /'
    echo ""
    echo "  Funding Payments (to Users):"
    $LEDGER_CMD balance Expenses:Funding --depth 3 -X $CURRENCY 2>/dev/null | sed 's/^/    /'
    echo ""
    echo "  Insurance Fund Payouts:"
    $LEDGER_CMD balance Expenses:Insurance --depth 3 -X $CURRENCY 2>/dev/null | sed 's/^/    /'
    echo ""
    echo "  Operating Expenses:"
    $LEDGER_CMD balance Expenses:Operations --depth 3 -X $CURRENCY 2>/dev/null | sed 's/^/    /'
    echo ""
    echo "  Compliance & Regulatory:"
    $LEDGER_CMD balance Expenses:Compliance --depth 3 -X $CURRENCY 2>/dev/null | sed 's/^/    /'
    echo ""
    echo "  Bank Fees:"
    $LEDGER_CMD balance Expenses:Bank --depth 3 -X $CURRENCY 2>/dev/null | sed 's/^/    /'
    echo ""
    echo "  Tax Expenses (VAT / CIT):"
    $LEDGER_CMD balance Expenses:Taxes --depth 3 -X $CURRENCY 2>/dev/null | sed 's/^/    /'
    echo ""

    echo "───────────────────────────────────"
    echo "TOTAL EXPENSES:"
    $LEDGER_CMD balance Expenses -X $CURRENCY 2>/dev/null | sed 's/^/  /'
    echo ""

    cat << EOF

═══════════════════════════════════════════════════════════════════════════
 NET PROFIT / (LOSS):
EOF
    $LEDGER_CMD balance Revenue Expenses -X $CURRENCY 2>/dev/null | sed 's/^/  /'
    echo ""
    echo "═══════════════════════════════════════════════════════════════════════════"
}

# ─────────────────────────────────────────────────────────────────────────
# Cash Flow Statement (IAS 7)
# ─────────────────────────────────────────────────────────────────────────
cashflow_statement() {
    cat << EOF
═══════════════════════════════════════════════════════════════════════════
 CENTRAL RETAIL EXCHANGE LLC (CRE.mn)
 STATEMENT OF CASH FLOWS (Indirect Method)
 For the period ending: $DATE
 (In Mongolian Tugrik — MNT)
 Prepared in accordance with IAS 7
═══════════════════════════════════════════════════════════════════════════

OPERATING ACTIVITIES
─────────────────────────────────────────────────────────────────────────
EOF
    echo "  Customer Deposits Received:"
    $LEDGER_CMD register Assets:Bank Liabilities:Customers --depth 3 -X $CURRENCY --collapse 2>/dev/null | sed 's/^/    /'
    echo ""
    echo "  Trading Revenue Collected:"
    $LEDGER_CMD balance Revenue:Trading -X $CURRENCY 2>/dev/null | sed 's/^/    /'
    echo ""
    echo "  Operating Expenses Paid:"
    $LEDGER_CMD balance Expenses:Operations Expenses:Bank -X $CURRENCY 2>/dev/null | sed 's/^/    /'
    echo ""

    cat << EOF

INVESTING ACTIVITIES
─────────────────────────────────────────────────────────────────────────
EOF
    echo "  FXCM Hedge Account Movements:"
    $LEDGER_CMD register Assets:Hedge -X $CURRENCY --collapse 2>/dev/null | sed 's/^/    /'
    echo ""

    cat << EOF

FINANCING ACTIVITIES
─────────────────────────────────────────────────────────────────────────
EOF
    echo "  Capital Contributions:"
    $LEDGER_CMD balance Equity:Capital -X $CURRENCY 2>/dev/null | sed 's/^/    /'
    echo ""

    cat << EOF

═══════════════════════════════════════════════════════════════════════════
 CASH AND EQUIVALENTS AT END OF PERIOD:
EOF
    $LEDGER_CMD balance Assets:Exchange:Cash Assets:Bank -X $CURRENCY 2>/dev/null | sed 's/^/  /'
    echo ""
    echo "═══════════════════════════════════════════════════════════════════════════"
}

# ─────────────────────────────────────────────────────────────────────────
# Trial Balance
# ─────────────────────────────────────────────────────────────────────────
trial_balance() {
    cat << EOF
═══════════════════════════════════════════════════════════════════════════
 CENTRAL RETAIL EXCHANGE LLC (CRE.mn)
 TRIAL BALANCE
 As at: $DATE
 (Multi-currency)
═══════════════════════════════════════════════════════════════════════════

EOF
    $LEDGER_CMD balance --depth 3
    echo ""
    echo "═══════════════════════════════════════════════════════════════════════════"
    echo " Verification (must equal 0):"
    $LEDGER_CMD balance
    echo "═══════════════════════════════════════════════════════════════════════════"
}

# ─────────────────────────────────────────────────────────────────────────
# Main
# ─────────────────────────────────────────────────────────────────────────
case "${1:-full}" in
    balance_sheet|bs)
        balance_sheet
        ;;
    income|pnl|is)
        income_statement
        ;;
    cashflow|cf)
        cashflow_statement
        ;;
    trial_balance|tb)
        trial_balance
        ;;
    full|all)
        balance_sheet
        echo ""
        echo ""
        income_statement
        echo ""
        echo ""
        cashflow_statement
        echo ""
        echo ""
        trial_balance
        ;;
    *)
        echo "Usage: $0 {balance_sheet|income|cashflow|trial_balance|full}"
        echo "  balance_sheet (bs)  - IFRS Statement of Financial Position"
        echo "  income (pnl/is)     - IFRS Statement of Profit or Loss"
        echo "  cashflow (cf)       - IAS 7 Cash Flow Statement"
        echo "  trial_balance (tb)  - Trial Balance"
        echo "  full (all)          - All statements"
        exit 1
        ;;
esac
