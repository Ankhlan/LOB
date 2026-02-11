#!/bin/bash
# ═══════════════════════════════════════════════════════════════════════
# CRE.mn TAX REPORTING TEMPLATE
# Generates annual tax reports for Mongolian filing (eBarimt compatible)
# 
# Compliant with:
#   - Mongolian CIT (Corporate Income Tax) 10%/25%
#   - eBarimt e-invoicing requirements
#   - IFRS 9 realized P&L reporting
#
# Usage:
#   bash tax_report.sh annual 2026             # Full annual report
#   bash tax_report.sh user USER_ID 2026       # Per-user report
#   bash tax_report.sh vat 2026-01             # Monthly VAT report
#   bash tax_report.sh cit 2026                # CIT calculation
#
# Created: 2026-02-10 by LEDGER
# ═══════════════════════════════════════════════════════════════════════

LEDGER_FILE="${LEDGER_FILE:-ledger/master.journal}"
LEDGER_CMD="ledger -f $LEDGER_FILE"

# ─── Colors ───
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'
BOLD='\033[1m'

# ═══════════════════════════════════════════════════════════════════════
# ANNUAL TAX REPORT
# ═══════════════════════════════════════════════════════════════════════
annual_report() {
    local YEAR="${1:-2026}"
    local BEGIN="$YEAR-01-01"
    local END="$YEAR-12-31"

    echo ""
    echo "═══════════════════════════════════════════════════════════════"
    echo " CRE.mn ANNUAL TAX REPORT — $YEAR"
    echo " Mongolian Tax Authority / Монголын Татварын Ерөнхий Газар"
    echo " Generated: $(date '+%Y-%m-%d %H:%M:%S')"
    echo "═══════════════════════════════════════════════════════════════"
    echo ""

    # ─── 1. REVENUE SUMMARY ───
    echo -e "${BOLD}1. REVENUE SUMMARY (Орлогын тайлан)${NC}"
    echo "────────────────────────────────────────────────────"
    echo "Trading Revenue (Spread):"
    $LEDGER_CMD register Revenue:Trading:Spread -b "$BEGIN" -e "$END" --collapse --register-format "  %(quantity(display_total))\n" 2>/dev/null || echo "  0"
    echo ""
    echo "Fee Revenue:"
    $LEDGER_CMD register Revenue:Trading:Fees -b "$BEGIN" -e "$END" --collapse --register-format "  %(quantity(display_total))\n" 2>/dev/null || echo "  0"
    echo ""
    echo "Total Revenue:"
    $LEDGER_CMD register Revenue -b "$BEGIN" -e "$END" --collapse --register-format "  %(quantity(display_total))\n" 2>/dev/null || echo "  0"
    echo ""

    # ─── 2. EXPENSE SUMMARY ───
    echo -e "${BOLD}2. EXPENSES (Зардлын тайлан)${NC}"
    echo "────────────────────────────────────────────────────"
    echo "Customer Payouts:"
    $LEDGER_CMD register Expenses:Trading:CustomerPayout -b "$BEGIN" -e "$END" --collapse --register-format "  %(quantity(display_total))\n" 2>/dev/null || echo "  0"
    echo ""
    echo "Insurance Fund Contributions:"
    $LEDGER_CMD register Expenses:Insurance -b "$BEGIN" -e "$END" --collapse --register-format "  %(quantity(display_total))\n" 2>/dev/null || echo "  0"
    echo ""
    echo "Total Expenses:"
    $LEDGER_CMD register Expenses -b "$BEGIN" -e "$END" --collapse --register-format "  %(quantity(display_total))\n" 2>/dev/null || echo "  0"
    echo ""

    # ─── 3. NET INCOME ───
    echo -e "${BOLD}3. NET INCOME / TAXABLE INCOME (Цэвэр ашиг)${NC}"
    echo "────────────────────────────────────────────────────"
    $LEDGER_CMD balance Revenue Expenses -b "$BEGIN" -e "$END" 2>/dev/null || echo "  0"
    echo ""

    # ─── 4. VAT SUMMARY ───
    echo -e "${BOLD}4. VAT LIABILITY (НӨАТ — 10%)${NC}"
    echo "────────────────────────────────────────────────────"
    echo "Note: VAT applies to service fees charged to customers."
    echo "Spread-based revenue: consult tax advisor on VAT applicability."
    $LEDGER_CMD register Liabilities:Taxes:VAT -b "$BEGIN" -e "$END" --collapse --register-format "  %(quantity(display_total))\n" 2>/dev/null || echo "  Not yet recorded"
    echo ""

    # ─── 5. CUSTOMER FUND SEGREGATION ───
    echo -e "${BOLD}5. CUSTOMER FUNDS HELD (Харилцагчийн хөрөнгө)${NC}"
    echo "────────────────────────────────────────────────────"
    echo "Total customer liabilities (must be segregated per FRC):"
    $LEDGER_CMD balance Liabilities:Customer --flat 2>/dev/null || echo "  0"
    echo ""

    # ─── 6. INSURANCE FUND ───
    echo -e "${BOLD}6. INSURANCE FUND (Даатгалын сан)${NC}"
    echo "────────────────────────────────────────────────────"
    $LEDGER_CMD balance Assets:Exchange:InsuranceFund 2>/dev/null || echo "  0"
    echo ""

    # ─── 7. CIT CALCULATION ───
    echo -e "${BOLD}7. CORPORATE INCOME TAX ESTIMATE (ААНОАТ)${NC}"
    echo "────────────────────────────────────────────────────"
    echo "  Mongolian CIT rates:"
    echo "  - First 6B MNT: 10%"
    echo "  - Above 6B MNT: 25%"
    echo "  (Calculation requires net income from section 3)"
    echo ""

    echo "═══════════════════════════════════════════════════════════════"
    echo " END OF ANNUAL TAX REPORT"
    echo "═══════════════════════════════════════════════════════════════"
}

# ═══════════════════════════════════════════════════════════════════════
# PER-USER TAX REPORT
# ═══════════════════════════════════════════════════════════════════════
user_report() {
    local USER_ID="$1"
    local YEAR="${2:-2026}"
    local BEGIN="$YEAR-01-01"
    local END="$YEAR-12-31"

    if [ -z "$USER_ID" ]; then
        echo "Usage: bash tax_report.sh user USER_ID [YEAR]"
        exit 1
    fi

    echo ""
    echo "═══════════════════════════════════════════════════════════════"
    echo " CRE.mn USER TAX STATEMENT — $YEAR"
    echo " User: $USER_ID"
    echo " Generated: $(date '+%Y-%m-%d %H:%M:%S')"
    echo "═══════════════════════════════════════════════════════════════"
    echo ""

    # ─── Deposits ───
    echo -e "${BOLD}DEPOSITS (Орлого оруулалт)${NC}"
    echo "────────────────────────────────────────────────────"
    $LEDGER_CMD register "Liabilities:Customer:${USER_ID}:Balance" and @deposit -b "$BEGIN" -e "$END" 2>/dev/null || echo "  No deposits"
    echo ""

    # ─── Withdrawals ───
    echo -e "${BOLD}WITHDRAWALS (Мөнгө авалт)${NC}"
    echo "────────────────────────────────────────────────────"
    $LEDGER_CMD register "Liabilities:Customer:${USER_ID}:Balance" and @withdrawal -b "$BEGIN" -e "$END" 2>/dev/null || echo "  No withdrawals"
    echo ""

    # ─── Trading Activity ───
    echo -e "${BOLD}TRADING P&L (Арилжааны ашиг/алдагдал)${NC}"
    echo "────────────────────────────────────────────────────"
    $LEDGER_CMD register "Liabilities:Customer:${USER_ID}" -b "$BEGIN" -e "$END" 2>/dev/null || echo "  No trading activity"
    echo ""

    # ─── Current Balance ───
    echo -e "${BOLD}CURRENT BALANCE (Одоогийн үлдэгдэл)${NC}"
    echo "────────────────────────────────────────────────────"
    $LEDGER_CMD balance "Liabilities:Customer:${USER_ID}" 2>/dev/null || echo "  0"
    echo ""

    # ─── Margin Activity ───
    echo -e "${BOLD}MARGIN ACTIVITY (Барьцааны хөдөлгөөн)${NC}"
    echo "────────────────────────────────────────────────────"
    $LEDGER_CMD register "Assets:Customer:${USER_ID}:Margin" -b "$BEGIN" -e "$END" 2>/dev/null || echo "  No margin activity"
    echo ""

    echo "═══════════════════════════════════════════════════════════════"
    echo " Note: This statement is for informational purposes."
    echo " For Mongolian personal income tax (HHӨAT), realized trading"
    echo " gains are taxable at 10%. Consult a licensed tax advisor."
    echo "═══════════════════════════════════════════════════════════════"
}

# ═══════════════════════════════════════════════════════════════════════
# MONTHLY VAT REPORT
# ═══════════════════════════════════════════════════════════════════════
vat_report() {
    local MONTH="${1:-$(date '+%Y-%m')}"
    local BEGIN="${MONTH}-01"
    # Calculate end of month
    local END=$(date -d "$BEGIN + 1 month" '+%Y-%m-%d' 2>/dev/null || date -v+1m -jf '%Y-%m-%d' "$BEGIN" '+%Y-%m-%d' 2>/dev/null || echo "${MONTH}-31")

    echo ""
    echo "═══════════════════════════════════════════════════════════════"
    echo " CRE.mn MONTHLY VAT REPORT (НӨАТ) — $MONTH"
    echo " eBarimt Filing Period"
    echo " Generated: $(date '+%Y-%m-%d %H:%M:%S')"
    echo "═══════════════════════════════════════════════════════════════"
    echo ""

    echo -e "${BOLD}TAXABLE REVENUE (НӨАТ-тай орлого)${NC}"
    echo "────────────────────────────────────────────────────"
    echo "Service Fee Revenue (10% VAT applies):"
    $LEDGER_CMD register Revenue:Trading:Fees -b "$BEGIN" -e "$END" --collapse --register-format "  %(quantity(display_total))\n" 2>/dev/null || echo "  0"
    echo ""
    echo "Spread Revenue (VAT treatment TBD — consult advisor):"
    $LEDGER_CMD register Revenue:Trading:Spread -b "$BEGIN" -e "$END" --collapse --register-format "  %(quantity(display_total))\n" 2>/dev/null || echo "  0"
    echo ""

    echo -e "${BOLD}VAT CALCULATION (НӨАТ тооцоо)${NC}"
    echo "────────────────────────────────────────────────────"
    echo "  VAT Rate: 10% (auto-accrued on every trade)"
    echo "  Method: Gross — DR Expenses:Taxes:VAT / CR Liabilities:Taxes:VAT"
    echo ""
    echo "  VAT Expense Accrued:"
    $LEDGER_CMD register Expenses:Taxes:VAT -b "$BEGIN" -e "$END" --collapse --register-format "  %(quantity(display_total))\n" 2>/dev/null || echo "  0"
    echo ""
    echo "  VAT Payable (Liability):"
    $LEDGER_CMD register Liabilities:Taxes:VAT -b "$BEGIN" -e "$END" --collapse --register-format "  %(quantity(display_total))\n" 2>/dev/null || echo "  0"
    echo ""

    echo -e "${BOLD}eBarimt FILING REQUIREMENTS${NC}"
    echo "────────────────────────────────────────────────────"
    echo "  - Issue eBarimt receipt for each fee/service charge"
    echo "  - GS1 product codes required (since Jan 2025)"
    echo "  - Digital signature on all receipts"
    echo "  - QR code verification enabled"
    echo "  - 5-year archive of all receipts"
    echo ""

    echo "═══════════════════════════════════════════════════════════════"
}

# ═══════════════════════════════════════════════════════════════════════
# CIT CALCULATION
# ═══════════════════════════════════════════════════════════════════════
cit_report() {
    local YEAR="${1:-2026}"
    local BEGIN="$YEAR-01-01"
    local END="$YEAR-12-31"

    echo ""
    echo "═══════════════════════════════════════════════════════════════"
    echo " CRE.mn CORPORATE INCOME TAX (ААНОАТ) — $YEAR"
    echo " Generated: $(date '+%Y-%m-%d %H:%M:%S')"
    echo "═══════════════════════════════════════════════════════════════"
    echo ""

    echo -e "${BOLD}GROSS REVENUE${NC}"
    echo "────────────────────────────────────────────────────"
    $LEDGER_CMD balance Revenue -b "$BEGIN" -e "$END" 2>/dev/null || echo "  0"
    echo ""

    echo -e "${BOLD}ALLOWABLE DEDUCTIONS${NC}"
    echo "────────────────────────────────────────────────────"
    $LEDGER_CMD balance Expenses -b "$BEGIN" -e "$END" 2>/dev/null || echo "  0"
    echo ""

    echo -e "${BOLD}NET TAXABLE INCOME${NC}"
    echo "────────────────────────────────────────────────────"
    $LEDGER_CMD balance Revenue Expenses -b "$BEGIN" -e "$END" 2>/dev/null || echo "  0"
    echo ""

    echo -e "${BOLD}CIT RATES (ААНОАТ хувь)${NC}"
    echo "────────────────────────────────────────────────────"
    echo "  Tier 1: First 6,000,000,000 MNT @ 10%  =  600,000,000 MNT max"
    echo "  Tier 2: Above 6,000,000,000 MNT @ 25%"
    echo ""
    echo "  Filing deadline: February 10 of following year"
    echo "  Quarterly advance payments required"
    echo ""

    echo "═══════════════════════════════════════════════════════════════"
}

# ═══════════════════════════════════════════════════════════════════════
# MAIN
# ═══════════════════════════════════════════════════════════════════════
case "${1:-annual}" in
    annual)
        annual_report "$2"
        ;;
    user)
        user_report "$2" "$3"
        ;;
    vat)
        vat_report "$2"
        ;;
    cit)
        cit_report "$2"
        ;;
    *)
        echo "CRE.mn Tax Report Generator"
        echo ""
        echo "Usage:"
        echo "  bash tax_report.sh annual [YEAR]           Annual tax report"
        echo "  bash tax_report.sh user USER_ID [YEAR]     Per-user tax statement"
        echo "  bash tax_report.sh vat [YYYY-MM]           Monthly VAT report"
        echo "  bash tax_report.sh cit [YEAR]              CIT calculation"
        ;;
esac
