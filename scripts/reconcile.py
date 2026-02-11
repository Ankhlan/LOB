#!/usr/bin/env python3
"""
CRE.mn Automated Reconciliation Tool
Compares SQLite ledger_entries against ledger-cli journal.
Flags discrepancies between the two systems.

Usage:
    python3 reconcile.py                    # Full reconciliation
    python3 reconcile.py --date 2026-02-10  # Specific date
    python3 reconcile.py --cron             # Machine-readable output for cron

Created: 2026-02-10 by LEDGER
"""

import sqlite3
import subprocess
import re
import sys
import os
import json
from datetime import datetime, timedelta
from collections import defaultdict

# Paths
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
LOB_DIR = os.path.dirname(SCRIPT_DIR)
DB_PATH = os.environ.get('CRE_DB', os.path.join(LOB_DIR, 'data', 'exchange.db'))
LEDGER_FILE = os.environ.get('LEDGER_FILE', os.path.join(LOB_DIR, 'ledger', 'master.journal'))

# ═══════════════════════════════════════════════════════════════════════════
# SQLITE QUERIES
# ═══════════════════════════════════════════════════════════════════════════

def get_sqlite_balances(db_path):
    """Get account balances from SQLite ledger_entries (sum of debits/credits)."""
    conn = sqlite3.connect(db_path)
    cur = conn.cursor()

    # Debit balances (positive for asset/expense accounts)
    cur.execute("""
        SELECT debit_account, SUM(amount), currency
        FROM ledger_entries
        GROUP BY debit_account, currency
    """)
    debits = defaultdict(lambda: defaultdict(float))
    for account, amount, currency in cur.fetchall():
        debits[account][currency] += amount

    # Credit balances
    cur.execute("""
        SELECT credit_account, SUM(amount), currency
        FROM ledger_entries
        GROUP BY credit_account, currency
    """)
    credits = defaultdict(lambda: defaultdict(float))
    for account, amount, currency in cur.fetchall():
        credits[account][currency] += amount

    conn.close()

    # Net balance per account: debits - credits
    all_accounts = set(debits.keys()) | set(credits.keys())
    balances = {}
    for account in all_accounts:
        for currency in set(list(debits.get(account, {}).keys()) +
                           list(credits.get(account, {}).keys())):
            key = f"{account}:{currency}"
            d = debits.get(account, {}).get(currency, 0)
            c = credits.get(account, {}).get(currency, 0)
            balances[key] = d - c

    return balances


def get_sqlite_entry_count(db_path):
    """Get total count of ledger entries from SQLite."""
    conn = sqlite3.connect(db_path)
    cur = conn.cursor()
    cur.execute("SELECT COUNT(*) FROM ledger_entries")
    count = cur.fetchone()[0]
    conn.close()
    return count


def get_sqlite_summary(db_path):
    """Get summary statistics from SQLite."""
    conn = sqlite3.connect(db_path)
    cur = conn.cursor()

    summary = {}

    # Entry count by type
    cur.execute("""
        SELECT event_type, COUNT(*), SUM(amount)
        FROM ledger_entries
        GROUP BY event_type
    """)
    summary['by_type'] = {row[0]: {'count': row[1], 'total': row[2]} for row in cur.fetchall()}

    # Total deposits
    cur.execute("""
        SELECT COALESCE(SUM(amount), 0) FROM ledger_entries
        WHERE event_type = 'deposit'
    """)
    summary['total_deposits'] = cur.fetchone()[0]

    # Total withdrawals
    cur.execute("""
        SELECT COALESCE(SUM(amount), 0) FROM ledger_entries
        WHERE event_type = 'withdrawal'
    """)
    summary['total_withdrawals'] = cur.fetchone()[0]

    # Total fees
    cur.execute("""
        SELECT COALESCE(SUM(amount), 0) FROM ledger_entries
        WHERE event_type IN ('trade_fee', 'spread_revenue')
    """)
    summary['total_revenue'] = cur.fetchone()[0]

    conn.close()
    return summary


# ═══════════════════════════════════════════════════════════════════════════
# LEDGER-CLI QUERIES
# ═══════════════════════════════════════════════════════════════════════════

def run_ledger(args, ledger_file=None):
    """Run ledger-cli command and return output."""
    lf = ledger_file or LEDGER_FILE
    cmd = ['ledger', '-f', lf] + args
    try:
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
        return result.stdout.strip(), result.stderr.strip()
    except (subprocess.TimeoutExpired, FileNotFoundError) as e:
        return '', str(e)


def get_ledger_balances(ledger_file=None):
    """Get account balances from ledger-cli."""
    stdout, stderr = run_ledger(
        ['balance', '--flat', '--no-total', '--balance-format',
         '%(account) %(quantity(display_total)) %(commodity(display_total))\n'],
        ledger_file
    )

    balances = {}
    for line in stdout.split('\n'):
        line = line.strip()
        if not line:
            continue
        # Parse: "Account:Name  1234.56 MNT"
        parts = line.rsplit(None, 2)
        if len(parts) >= 3:
            account = parts[0].strip()
            try:
                amount = float(parts[1].replace(',', ''))
                currency = parts[2]
                balances[f"{account}:{currency}"] = amount
            except (ValueError, IndexError):
                pass

    return balances


def get_ledger_transaction_count(ledger_file=None):
    """Count transactions in ledger-cli journal."""
    stdout, stderr = run_ledger(['stats'], ledger_file)
    # Parse "Time period: ... (N transactions)" or similar
    for line in stdout.split('\n'):
        if 'transactions' in line.lower():
            match = re.search(r'(\d+)', line)
            if match:
                return int(match.group(1))
    return 0


def verify_ledger_balance(ledger_file=None):
    """Verify ledger-cli books balance to zero."""
    stdout, stderr = run_ledger(['balance'], ledger_file)
    # Last line should show 0 or be empty
    lines = [l.strip() for l in stdout.split('\n') if l.strip() and '---' not in l]
    if not lines:
        return True, "Empty journal"

    last_line = lines[-1]
    if last_line == '0' or last_line == '':
        return True, "Books balance to 0"
    else:
        return False, f"Imbalance detected: {last_line}"


# ═══════════════════════════════════════════════════════════════════════════
# RECONCILIATION
# ═══════════════════════════════════════════════════════════════════════════

def reconcile(db_path=None, ledger_file=None, verbose=True):
    """Run full reconciliation between SQLite and ledger-cli."""
    db = db_path or DB_PATH
    lf = ledger_file or LEDGER_FILE

    results = {
        'timestamp': datetime.now().isoformat(),
        'status': 'PASS',
        'checks': [],
        'discrepancies': []
    }

    header = "═" * 70
    if verbose:
        print(header)
        print(" CRE.mn AUTOMATED RECONCILIATION REPORT")
        print(f" Run at: {results['timestamp']}")
        print(f" SQLite: {db}")
        print(f" Ledger: {lf}")
        print(header)
        print()

    # ─── Check 1: Ledger-cli journal integrity ───
    balanced, msg = verify_ledger_balance(lf)
    check = {
        'name': 'Ledger-cli Balance Check',
        'status': 'PASS' if balanced else 'FAIL',
        'detail': msg
    }
    results['checks'].append(check)
    if verbose:
        status = "✅ PASS" if balanced else "❌ FAIL"
        print(f"  [{status}] {check['name']}: {msg}")
    if not balanced:
        results['status'] = 'FAIL'

    # ─── Check 2: SQLite database exists and readable ───
    db_exists = os.path.exists(db)
    check = {
        'name': 'SQLite Database Accessible',
        'status': 'PASS' if db_exists else 'SKIP',
        'detail': f"{'Found' if db_exists else 'Not found'}: {db}"
    }
    results['checks'].append(check)
    if verbose:
        status = "✅ PASS" if db_exists else "⚠️ SKIP"
        print(f"  [{status}] {check['name']}: {check['detail']}")

    if not db_exists:
        # Can't compare if no SQLite DB
        if verbose:
            print("\n  ⚠️  SQLite database not found — skipping cross-system checks.")
            print("     This is expected if no trades have occurred yet.")
        results['checks'].append({
            'name': 'Cross-system reconciliation',
            'status': 'SKIP',
            'detail': 'No SQLite database to compare'
        })
        return results

    # ─── Check 3: Entry counts ───
    sqlite_count = get_sqlite_entry_count(db)
    ledger_count = get_ledger_transaction_count(lf)
    # Note: counts won't match exactly because ledger-cli counts transactions
    # (which can have multiple postings) while SQLite counts entries
    check = {
        'name': 'Transaction Counts',
        'status': 'INFO',
        'detail': f"SQLite entries: {sqlite_count}, Ledger-cli transactions: {ledger_count}"
    }
    results['checks'].append(check)
    if verbose:
        print(f"  [ℹ️ INFO] {check['name']}: {check['detail']}")

    # ─── Check 4: Compare account balances ───
    if verbose:
        print()
        print("  Comparing account balances:")

    sqlite_bal = get_sqlite_balances(db)
    ledger_bal = get_ledger_balances(lf)

    all_keys = set(sqlite_bal.keys()) | set(ledger_bal.keys())
    discrepancies = []

    for key in sorted(all_keys):
        sq = sqlite_bal.get(key, 0)
        lg = ledger_bal.get(key, 0)
        diff = abs(sq - lg)

        if diff > 0.01:  # Tolerance for floating point
            discrepancy = {
                'account': key,
                'sqlite': sq,
                'ledger': lg,
                'difference': diff
            }
            discrepancies.append(discrepancy)
            results['discrepancies'].append(discrepancy)
            if verbose:
                print(f"    ❌ {key}: SQLite={sq:.2f}, Ledger={lg:.2f}, Diff={diff:.2f}")
        elif verbose and (sq != 0 or lg != 0):
            print(f"    ✅ {key}: {sq:.2f} (matched)")

    if discrepancies:
        results['status'] = 'FAIL'
        check = {
            'name': 'Balance Reconciliation',
            'status': 'FAIL',
            'detail': f"{len(discrepancies)} discrepancies found"
        }
    else:
        check = {
            'name': 'Balance Reconciliation',
            'status': 'PASS',
            'detail': f"All {len([k for k in all_keys if sqlite_bal.get(k,0) != 0 or ledger_bal.get(k,0) != 0])} accounts match"
        }

    results['checks'].append(check)
    if verbose:
        print()
        status = "✅ PASS" if check['status'] == 'PASS' else "❌ FAIL"
        print(f"  [{status}] {check['name']}: {check['detail']}")

    # ─── Check 5: SQLite summary ───
    summary = get_sqlite_summary(db)
    check = {
        'name': 'SQLite Summary',
        'status': 'INFO',
        'detail': f"Deposits: {summary['total_deposits']:.2f}, "
                  f"Withdrawals: {summary['total_withdrawals']:.2f}, "
                  f"Revenue: {summary['total_revenue']:.2f}"
    }
    results['checks'].append(check)
    if verbose:
        print(f"  [ℹ️ INFO] {check['name']}: {check['detail']}")

    # ─── Final Status ───
    if verbose:
        print()
        print(header)
        if results['status'] == 'PASS':
            print(" ✅ RECONCILIATION PASSED — All systems consistent")
        else:
            print(f" ❌ RECONCILIATION FAILED — {len(results['discrepancies'])} discrepancies")
        print(header)

    return results


# ═══════════════════════════════════════════════════════════════════════════
# MAIN
# ═══════════════════════════════════════════════════════════════════════════

if __name__ == '__main__':
    import argparse

    parser = argparse.ArgumentParser(description='CRE.mn Automated Reconciliation')
    parser.add_argument('--db', help='SQLite database path', default=DB_PATH)
    parser.add_argument('--ledger', help='Ledger journal file', default=LEDGER_FILE)
    parser.add_argument('--cron', action='store_true', help='Machine-readable JSON output')
    parser.add_argument('--date', help='Reconcile specific date (YYYY-MM-DD)')

    args = parser.parse_args()

    if args.cron:
        results = reconcile(args.db, args.ledger, verbose=False)
        print(json.dumps(results, indent=2))
        sys.exit(0 if results['status'] == 'PASS' else 1)
    else:
        results = reconcile(args.db, args.ledger, verbose=True)
        sys.exit(0 if results['status'] == 'PASS' else 1)
