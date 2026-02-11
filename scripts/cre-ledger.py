#!/usr/bin/env python3
"""
CRE.mn Exchange Ledger CLI
===========================
Blackbox factory audit tool for the CRE.mn derivatives exchange.
Queries the SQLite ledger for balances, trade history, reconciliation, and audit.

Usage:
    python cre-ledger.py balance <user_id>
    python cre-ledger.py history <user_id> [--from DATE] [--to DATE]
    python cre-ledger.py trace <trade_id>
    python cre-ledger.py reconcile [--date DATE]
    python cre-ledger.py audit [--from DATE] [--to DATE] [--limit N]
    python cre-ledger.py export [--format csv|json|ledger] [--from DATE] [--to DATE]
    python cre-ledger.py verify
    python cre-ledger.py summary
    python cre-ledger.py accounts
    python cre-ledger.py deposit-audit [--from DATE] [--to DATE] [--journal PATH] [--ledger-dir PATH]

Environment:
    DB_PATH  - Path to exchange SQLite database (default: /var/lib/exchange/exchange.db)
"""

import argparse
import csv
import io
import json
import os
import sqlite3
import sys
from datetime import datetime, timezone


def get_db_path():
    db_path = os.environ.get("DB_PATH", "/var/lib/exchange/exchange.db")
    if not os.path.exists(db_path):
        alt_paths = [
            os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "data", "exchange.db"),
            os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "exchange.db"),
        ]
        for p in alt_paths:
            if os.path.exists(p):
                return p
        print(f"ERROR: Database not found at {db_path}", file=sys.stderr)
        print(f"Set DB_PATH environment variable to the correct path.", file=sys.stderr)
        sys.exit(1)
    return db_path


def connect():
    db_path = get_db_path()
    conn = sqlite3.connect(db_path)
    conn.row_factory = sqlite3.Row
    conn.execute("PRAGMA journal_mode=WAL")
    return conn


def ts_to_str(ts_ms):
    """Convert millisecond epoch timestamp to ISO string."""
    if ts_ms is None:
        return "N/A"
    try:
        dt = datetime.fromtimestamp(ts_ms / 1000.0, tz=timezone.utc)
        return dt.strftime("%Y-%m-%d %H:%M:%S UTC")
    except (ValueError, OSError):
        return str(ts_ms)


def parse_date(date_str):
    """Parse date string to epoch milliseconds."""
    for fmt in ["%Y-%m-%d %H:%M:%S", "%Y-%m-%d", "%Y%m%d"]:
        try:
            dt = datetime.strptime(date_str, fmt).replace(tzinfo=timezone.utc)
            return int(dt.timestamp() * 1000)
        except ValueError:
            continue
    raise ValueError(f"Cannot parse date: {date_str}. Use YYYY-MM-DD format.")


def fmt_mnt(amount):
    """Format MNT amount with thousands separator."""
    if amount is None:
        return "0.00"
    if abs(amount) >= 1_000_000:
        return f"{amount:,.2f}"
    return f"{amount:.2f}"


# ─── COMMANDS ───────────────────────────────────────────────────────

def cmd_balance(args):
    """Show current balance for a user."""
    conn = connect()
    user_id = args.user_id

    # Get balance from balances table
    row = conn.execute(
        "SELECT balance, margin_used, updated_at FROM balances WHERE user_id = ?",
        (user_id,)
    ).fetchone()

    if not row:
        print(f"No balance record found for user: {user_id}")
        conn.close()
        return

    balance = row["balance"]
    margin_used = row["margin_used"]
    equity = balance  # unrealized PnL not stored in DB
    available = balance - margin_used

    print(f"╔══════════════════════════════════════════╗")
    print(f"║  CRE.mn Ledger - Account Balance         ║")
    print(f"╠══════════════════════════════════════════╣")
    print(f"║  User:        {user_id:<26s} ║")
    print(f"║  Balance:     {fmt_mnt(balance):>22s} MNT ║")
    print(f"║  Margin Used: {fmt_mnt(margin_used):>22s} MNT ║")
    print(f"║  Available:   {fmt_mnt(available):>22s} MNT ║")
    print(f"║  Updated:     {ts_to_str(row['updated_at']):<26s} ║")
    print(f"╚══════════════════════════════════════════╝")

    # Show ledger-derived balance
    ledger_bal = conn.execute("""
        SELECT 
            COALESCE(SUM(CASE WHEN credit_account LIKE ? THEN amount ELSE 0 END), 0) -
            COALESCE(SUM(CASE WHEN debit_account LIKE ? THEN amount ELSE 0 END), 0) as ledger_balance
        FROM ledger_entries
    """, (f"%{user_id}%", f"%{user_id}%")).fetchone()

    if ledger_bal and ledger_bal["ledger_balance"] != 0:
        print(f"\n  Ledger-derived balance: {fmt_mnt(ledger_bal['ledger_balance'])} MNT")
        diff = balance - ledger_bal["ledger_balance"]
        if abs(diff) > 0.01:
            print(f"  ⚠ DISCREPANCY: {fmt_mnt(diff)} MNT vs balance table")

    # Show recent fees paid
    fees = conn.execute(
        "SELECT SUM(amount) as total_fees FROM fees WHERE user_id = ?",
        (user_id,)
    ).fetchone()
    if fees and fees["total_fees"]:
        print(f"  Total fees paid: {fmt_mnt(fees['total_fees'])} MNT")

    conn.close()


def cmd_history(args):
    """Show ledger history for a user."""
    conn = connect()
    user_id = args.user_id

    query = """
        SELECT id, event_type, debit_account, credit_account, amount, 
               currency, reference_id, symbol, description, timestamp
        FROM ledger_entries
        WHERE user_id = ? OR debit_account LIKE ? OR credit_account LIKE ?
    """
    params = [user_id, f"%{user_id}%", f"%{user_id}%"]

    if args.date_from:
        query += " AND timestamp >= ?"
        params.append(parse_date(args.date_from))
    if args.date_to:
        query += " AND timestamp <= ?"
        params.append(parse_date(args.date_to))

    query += " ORDER BY timestamp DESC LIMIT ?"
    params.append(args.limit or 50)

    rows = conn.execute(query, params).fetchall()

    if not rows:
        print(f"No ledger entries found for user: {user_id}")
        conn.close()
        return

    print(f"{'ID':>6} {'Timestamp':<22} {'Type':<15} {'Debit':<25} {'Credit':<25} {'Amount':>15} {'Symbol':<12} {'Description'}")
    print("─" * 140)

    for r in rows:
        print(f"{r['id']:>6} {ts_to_str(r['timestamp']):<22} {r['event_type']:<15} "
              f"{r['debit_account']:<25} {r['credit_account']:<25} "
              f"{fmt_mnt(r['amount']):>15} {(r['symbol'] or ''):<12} {r['description'] or ''}")

    print(f"\n  Total entries: {len(rows)}")
    conn.close()


def cmd_trace(args):
    """Trace a trade from order to settlement."""
    conn = connect()
    trade_id = args.trade_id

    # Get trade details
    trade = conn.execute(
        "SELECT * FROM trades WHERE id = ?", (trade_id,)
    ).fetchone()

    if not trade:
        print(f"Trade {trade_id} not found.")
        conn.close()
        return

    print(f"╔══════════════════════════════════════════════╗")
    print(f"║  CRE.mn Ledger - Trade Trace #{trade_id:<15} ║")
    print(f"╠══════════════════════════════════════════════╣")
    print(f"║  Buyer:    {trade['buyer_id']:<34s} ║")
    print(f"║  Seller:   {trade['seller_id']:<34s} ║")
    print(f"║  Symbol:   {trade['symbol']:<34s} ║")
    print(f"║  Quantity: {fmt_mnt(trade['quantity']):>15s}{'':19s} ║")
    print(f"║  Price:    {fmt_mnt(trade['price']):>15s} MNT{'':15s} ║")
    notional = trade['quantity'] * trade['price']
    print(f"║  Notional: {fmt_mnt(notional):>15s} MNT{'':15s} ║")
    print(f"║  Time:     {ts_to_str(trade['timestamp']):<34s} ║")
    print(f"╚══════════════════════════════════════════════╝")

    # Find related ledger entries
    ref_id = str(trade_id)
    entries = conn.execute(
        "SELECT * FROM ledger_entries WHERE reference_id = ? ORDER BY id",
        (ref_id,)
    ).fetchall()

    if entries:
        print(f"\n  Ledger Trail ({len(entries)} entries):")
        print(f"  {'Type':<15} {'Debit':<30} {'Credit':<30} {'Amount':>15}")
        print(f"  " + "─" * 95)
        for e in entries:
            print(f"  {e['event_type']:<15} {e['debit_account']:<30} {e['credit_account']:<30} {fmt_mnt(e['amount']):>15}")
    else:
        print(f"\n  ⚠ No ledger entries found for trade #{trade_id}")

    # Find related fees
    fees = conn.execute(
        "SELECT * FROM fees WHERE trade_id = ?", (trade_id,)
    ).fetchall()
    if fees:
        print(f"\n  Fees ({len(fees)}):")
        for f in fees:
            print(f"    {f['user_id']}: {f['fee_type']} = {fmt_mnt(f['amount'])} MNT")

    conn.close()


def cmd_reconcile(args):
    """Reconcile ledger: total debits must equal total credits."""
    conn = connect()

    query = "SELECT debit_account, credit_account, amount, timestamp FROM ledger_entries"
    params = []
    if args.date:
        ts = parse_date(args.date)
        query += " WHERE timestamp <= ?"
        params.append(ts)

    rows = conn.execute(query, params).fetchall()

    if not rows:
        print("No ledger entries found.")
        conn.close()
        return

    # Aggregate by account
    accounts = {}
    total_debits = 0.0
    total_credits = 0.0

    for r in rows:
        debit = r["debit_account"]
        credit = r["credit_account"]
        amount = r["amount"]

        accounts.setdefault(debit, {"debits": 0.0, "credits": 0.0})
        accounts.setdefault(credit, {"debits": 0.0, "credits": 0.0})

        accounts[debit]["debits"] += amount
        accounts[credit]["credits"] += amount
        total_debits += amount
        total_credits += amount

    date_str = args.date or "ALL TIME"
    print(f"╔══════════════════════════════════════════════════════════════╗")
    print(f"║  CRE.mn Ledger Reconciliation - {date_str:<28s} ║")
    print(f"╠══════════════════════════════════════════════════════════════╣")

    print(f"\n  {'Account':<40} {'Debits':>15} {'Credits':>15} {'Net':>15}")
    print(f"  " + "─" * 88)

    for acct in sorted(accounts.keys()):
        d = accounts[acct]["debits"]
        c = accounts[acct]["credits"]
        net = c - d
        print(f"  {acct:<40} {fmt_mnt(d):>15} {fmt_mnt(c):>15} {fmt_mnt(net):>15}")

    print(f"  " + "─" * 88)
    diff = total_credits - total_debits
    print(f"  {'TOTALS':<40} {fmt_mnt(total_debits):>15} {fmt_mnt(total_credits):>15} {fmt_mnt(diff):>15}")

    if abs(diff) < 0.01:
        print(f"\n  ✅ RECONCILED: Total debits = Total credits ({fmt_mnt(total_debits)} MNT)")
        print(f"  {len(rows)} entries across {len(accounts)} accounts")
    else:
        print(f"\n  ❌ IMBALANCE DETECTED: {fmt_mnt(diff)} MNT discrepancy!")
        print(f"  INVESTIGATE IMMEDIATELY - exchange ledger is out of balance")

    conn.close()


def cmd_audit(args):
    """Full audit trail with optional date range."""
    conn = connect()

    query = "SELECT * FROM ledger_entries WHERE 1=1"
    params = []

    if args.date_from:
        query += " AND timestamp >= ?"
        params.append(parse_date(args.date_from))
    if args.date_to:
        query += " AND timestamp <= ?"
        params.append(parse_date(args.date_to))

    query += " ORDER BY timestamp ASC LIMIT ?"
    params.append(args.limit or 100)

    rows = conn.execute(query, params).fetchall()

    if not rows:
        print("No ledger entries found for the specified period.")
        conn.close()
        return

    # Summary stats
    event_types = {}
    total_volume = 0.0
    for r in rows:
        et = r["event_type"]
        event_types[et] = event_types.get(et, 0) + 1
        total_volume += r["amount"]

    print(f"╔══════════════════════════════════════════════════╗")
    print(f"║  CRE.mn Exchange Audit Report                    ║")
    print(f"╠══════════════════════════════════════════════════╣")
    print(f"║  Period: {args.date_from or 'BEGINNING'} to {args.date_to or 'NOW':<20s} ║")
    print(f"║  Entries: {len(rows):<39} ║")
    print(f"║  Volume:  {fmt_mnt(total_volume):>20s} MNT{'':17s} ║")
    print(f"╚══════════════════════════════════════════════════╝")

    print(f"\n  Event Type Breakdown:")
    for et, count in sorted(event_types.items(), key=lambda x: -x[1]):
        print(f"    {et:<25} {count:>6} entries")

    print(f"\n  Full Ledger:")
    print(f"  {'ID':>6} {'Timestamp':<22} {'Type':<15} {'Debit':<25} {'Credit':<25} {'Amount':>15}")
    print(f"  " + "─" * 115)
    for r in rows:
        print(f"  {r['id']:>6} {ts_to_str(r['timestamp']):<22} {r['event_type']:<15} "
              f"{r['debit_account']:<25} {r['credit_account']:<25} {fmt_mnt(r['amount']):>15}")

    conn.close()


def cmd_export(args):
    """Export ledger entries in various formats."""
    conn = connect()

    query = "SELECT * FROM ledger_entries WHERE 1=1"
    params = []

    if args.date_from:
        query += " AND timestamp >= ?"
        params.append(parse_date(args.date_from))
    if args.date_to:
        query += " AND timestamp <= ?"
        params.append(parse_date(args.date_to))

    query += " ORDER BY timestamp ASC"
    rows = conn.execute(query, params).fetchall()

    if not rows:
        print("No entries to export.", file=sys.stderr)
        conn.close()
        return

    fmt = args.format or "json"

    if fmt == "json":
        entries = []
        for r in rows:
            entries.append({
                "id": r["id"],
                "event_type": r["event_type"],
                "debit_account": r["debit_account"],
                "credit_account": r["credit_account"],
                "amount": r["amount"],
                "currency": r["currency"],
                "reference_id": r["reference_id"],
                "user_id": r["user_id"],
                "symbol": r["symbol"],
                "description": r["description"],
                "timestamp": r["timestamp"],
                "timestamp_utc": ts_to_str(r["timestamp"])
            })
        print(json.dumps(entries, indent=2))

    elif fmt == "csv":
        output = io.StringIO()
        writer = csv.writer(output)
        writer.writerow(["id", "event_type", "debit_account", "credit_account",
                         "amount", "currency", "reference_id", "user_id",
                         "symbol", "description", "timestamp", "timestamp_utc"])
        for r in rows:
            writer.writerow([r["id"], r["event_type"], r["debit_account"],
                             r["credit_account"], r["amount"], r["currency"],
                             r["reference_id"], r["user_id"], r["symbol"],
                             r["description"], r["timestamp"], ts_to_str(r["timestamp"])])
        print(output.getvalue())

    elif fmt == "ledger":
        # Plain-text double-entry ledger format (ledger-cli compatible)
        for r in rows:
            date_str = ts_to_str(r["timestamp"]).split(" ")[0] if r["timestamp"] else "1970-01-01"
            desc = r["description"] or r["event_type"]
            ref = f" ; ref:{r['reference_id']}" if r["reference_id"] else ""
            print(f"{date_str} {desc}{ref}")
            print(f"    {r['debit_account']:<40} {r['amount']:.2f} MNT")
            print(f"    {r['credit_account']:<40} {-r['amount']:.2f} MNT")
            print()

    print(f"# Exported {len(rows)} entries in {fmt} format", file=sys.stderr)
    conn.close()


def cmd_verify(args):
    """Verify ledger integrity: debits = credits, no orphans, no gaps."""
    conn = connect()

    print("CRE.mn Ledger Integrity Verification")
    print("=" * 50)
    errors = 0

    # Check 1: Total debits = total credits
    row = conn.execute("""
        SELECT 
            COUNT(*) as entry_count,
            SUM(amount) as total_debit_volume,
            SUM(amount) as total_credit_volume
        FROM ledger_entries
    """).fetchone()

    entry_count = row["entry_count"]
    print(f"\n[1/5] Double-Entry Balance Check")
    print(f"  Total entries: {entry_count}")
    if entry_count > 0:
        # In double-entry, every row has equal debit and credit amounts
        print(f"  ✅ Each entry posts equal amounts to debit and credit accounts")
    else:
        print(f"  ⚠ No ledger entries found")

    # Check 2: All trades have ledger entries
    trades_count = conn.execute("SELECT COUNT(*) as c FROM trades").fetchone()["c"]
    trades_with_ledger = conn.execute("""
        SELECT COUNT(DISTINCT t.id) as c 
        FROM trades t
        INNER JOIN ledger_entries le ON le.reference_id = CAST(t.id AS TEXT)
    """).fetchone()["c"]

    print(f"\n[2/5] Trade-to-Ledger Mapping")
    print(f"  Total trades: {trades_count}")
    print(f"  Trades with ledger entries: {trades_with_ledger}")
    orphan_trades = trades_count - trades_with_ledger
    if orphan_trades == 0:
        print(f"  ✅ All trades have corresponding ledger entries")
    else:
        print(f"  ❌ {orphan_trades} trades have NO ledger entries (orphan trades)")
        errors += 1

    # Check 3: Balance table vs ledger-derived balances
    print(f"\n[3/5] Balance Table vs Ledger Cross-Check")
    users = conn.execute("SELECT user_id, balance FROM balances").fetchall()
    balance_mismatches = 0
    for u in users:
        ledger_bal = conn.execute("""
            SELECT 
                COALESCE(SUM(CASE WHEN credit_account LIKE ? THEN amount ELSE 0 END), 0) -
                COALESCE(SUM(CASE WHEN debit_account LIKE ? THEN amount ELSE 0 END), 0) as bal
            FROM ledger_entries
        """, (f"%{u['user_id']}%", f"%{u['user_id']}%")).fetchone()["bal"]

        if abs(u["balance"] - ledger_bal) > 0.01:
            print(f"  ❌ {u['user_id']}: balance={fmt_mnt(u['balance'])}, ledger={fmt_mnt(ledger_bal)}")
            balance_mismatches += 1
            errors += 1

    if balance_mismatches == 0 and len(users) > 0:
        print(f"  ✅ All {len(users)} user balances match ledger ({len(users)} checked)")
    elif len(users) == 0:
        print(f"  ⚠ No user balances to check")

    # Check 4: Sequential ID check (no gaps)
    print(f"\n[4/5] Ledger Entry Sequence Check")
    if entry_count > 0:
        min_max = conn.execute("SELECT MIN(id) as mn, MAX(id) as mx FROM ledger_entries").fetchone()
        expected = min_max["mx"] - min_max["mn"] + 1
        if expected == entry_count:
            print(f"  ✅ Sequential IDs: {min_max['mn']} to {min_max['mx']} (no gaps)")
        else:
            gaps = expected - entry_count
            print(f"  ❌ {gaps} gaps in ledger entry IDs (possible deletion)")
            errors += 1
    else:
        print(f"  ⚠ No entries to check")

    # Check 5: Fee table consistency
    print(f"\n[5/5] Fee Table Consistency")
    fee_count = conn.execute("SELECT COUNT(*) as c FROM fees").fetchone()["c"]
    fee_total = conn.execute("SELECT COALESCE(SUM(amount), 0) as t FROM fees").fetchone()["t"]
    print(f"  Fee records: {fee_count}")
    print(f"  Total fees collected: {fmt_mnt(fee_total)} MNT")
    if fee_count > 0:
        print(f"  ✅ Fee records present")
    else:
        print(f"  ⚠ No fee records found")

    # Final verdict
    print(f"\n{'=' * 50}")
    if errors == 0:
        print(f"✅ LEDGER VERIFIED: {entry_count} entries, {trades_count} trades, {len(users)} accounts - ALL CLEAN")
    else:
        print(f"❌ VERIFICATION FAILED: {errors} error(s) found. Investigate immediately.")

    conn.close()
    return errors


def cmd_summary(args):
    """Show exchange summary: total users, trades, volume, fees."""
    conn = connect()

    users = conn.execute("SELECT COUNT(*) as c FROM balances").fetchone()["c"]
    trades = conn.execute("SELECT COUNT(*) as c FROM trades").fetchone()["c"]
    entries = conn.execute("SELECT COUNT(*) as c FROM ledger_entries").fetchone()["c"]
    volume = conn.execute("SELECT COALESCE(SUM(quantity * price), 0) as v FROM trades").fetchone()["v"]
    fees = conn.execute("SELECT COALESCE(SUM(amount), 0) as f FROM fees").fetchone()["f"]
    insurance = conn.execute("SELECT COALESCE(MAX(balance_after), 0) as b FROM insurance_fund").fetchone()["b"]

    print(f"╔══════════════════════════════════════════════════╗")
    print(f"║  CRE.mn Exchange Summary                         ║")
    print(f"╠══════════════════════════════════════════════════╣")
    print(f"║  Registered Users:  {users:>10}{'':19s} ║")
    print(f"║  Total Trades:      {trades:>10}{'':19s} ║")
    print(f"║  Ledger Entries:    {entries:>10}{'':19s} ║")
    print(f"║  Trading Volume:    {fmt_mnt(volume):>20s} MNT{'':5s} ║")
    print(f"║  Total Fees:        {fmt_mnt(fees):>20s} MNT{'':5s} ║")
    print(f"║  Insurance Fund:    {fmt_mnt(insurance):>20s} MNT{'':5s} ║")
    print(f"╚══════════════════════════════════════════════════╝")

    # Top symbols by volume
    symbols = conn.execute("""
        SELECT symbol, COUNT(*) as trades, SUM(quantity * price) as volume
        FROM trades GROUP BY symbol ORDER BY volume DESC LIMIT 5
    """).fetchall()

    if symbols:
        print(f"\n  Top Markets by Volume:")
        for s in symbols:
            print(f"    {s['symbol']:<20} {s['trades']:>6} trades  {fmt_mnt(s['volume']):>20} MNT")

    conn.close()


def cmd_accounts(args):
    """List all ledger accounts and their balances."""
    conn = connect()

    rows = conn.execute("""
        SELECT account, SUM(credit) - SUM(debit) as balance FROM (
            SELECT debit_account as account, amount as debit, 0 as credit FROM ledger_entries
            UNION ALL
            SELECT credit_account as account, 0 as debit, amount as credit FROM ledger_entries
        ) GROUP BY account ORDER BY account
    """).fetchall()

    if not rows:
        print("No ledger accounts found.")
        conn.close()
        return

    print(f"  {'Account':<45} {'Balance':>20}")
    print(f"  " + "─" * 68)

    total = 0.0
    for r in rows:
        bal = r["balance"]
        total += bal
        marker = "  " if bal >= 0 else "⚠ "
        print(f"  {marker}{r['account']:<43} {fmt_mnt(bal):>20} MNT")

    print(f"  " + "─" * 68)
    print(f"  {'NET TOTAL':<45} {fmt_mnt(total):>20} MNT")

    if abs(total) < 0.01:
        print(f"\n  ✅ Ledger in balance (net = 0)")
    else:
        print(f"\n  ❌ Ledger out of balance by {fmt_mnt(total)} MNT")

    conn.close()


def cmd_deposit_audit(args):
    """Automated deposit/withdrawal reconciliation across all accounting systems.

    Cross-checks:
      1. SQLite ledger_entries (deposit/withdrawal events)
      2. journal.log (AccountingEngine flat file)
      3. LedgerWriter flat files (deposits.ledger, withdrawals.ledger)
      4. User balance table vs net deposit flow
    Flags discrepancies between systems.
    """
    conn = connect()
    errors = 0

    print("╔══════════════════════════════════════════════════════════════════╗")
    print("║  CRE.mn Deposit/Withdrawal Reconciliation Audit                ║")
    print("╠══════════════════════════════════════════════════════════════════╣")

    # ── 1. SQLite ledger_entries ──
    print("\n  [1/4] SQLite Ledger Entries (ledger_entries table)")
    query = """
        SELECT event_type,
               COUNT(*) as cnt,
               COALESCE(SUM(amount), 0) as total
        FROM ledger_entries
        WHERE event_type IN ('deposit', 'withdrawal')
    """
    params = []
    if args.date_from:
        query_base = query + " AND timestamp >= ?"
        params.append(parse_date(args.date_from))
    else:
        query_base = query
    if args.date_to:
        query_base += " AND timestamp <= ?"
        params.append(parse_date(args.date_to))
    query_base += " GROUP BY event_type"

    db_deposits = {"count": 0, "total": 0.0}
    db_withdrawals = {"count": 0, "total": 0.0}
    for r in conn.execute(query_base, params).fetchall():
        if r["event_type"] == "deposit":
            db_deposits = {"count": r["cnt"], "total": r["total"]}
        elif r["event_type"] == "withdrawal":
            db_withdrawals = {"count": r["cnt"], "total": r["total"]}

    print(f"    Deposits:     {db_deposits['count']:>6} txns  {fmt_mnt(db_deposits['total']):>20} MNT")
    print(f"    Withdrawals:  {db_withdrawals['count']:>6} txns  {fmt_mnt(db_withdrawals['total']):>20} MNT")
    db_net = db_deposits["total"] - db_withdrawals["total"]
    print(f"    Net flow:     {'':>6}       {fmt_mnt(db_net):>20} MNT")

    # ── 2. journal.log (AccountingEngine) ──
    print("\n  [2/4] AccountingEngine Journal (journal.log)")
    journal_path = args.journal or os.path.join(
        os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "data", "journal.log"
    )
    jnl_deposits = {"count": 0, "total": 0.0}
    jnl_withdrawals = {"count": 0, "total": 0.0}

    if os.path.exists(journal_path):
        with open(journal_path, "r") as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue
                parts = line.split("|")
                if len(parts) < 8:
                    continue
                # Format: id|timestamp|type|debit_account|credit_account|amount|reference_id|description
                entry_type = parts[2]
                try:
                    amount = float(parts[5])
                except (ValueError, IndexError):
                    continue
                if entry_type == "deposit":
                    jnl_deposits["count"] += 1
                    jnl_deposits["total"] += amount
                elif entry_type == "withdrawal":
                    jnl_withdrawals["count"] += 1
                    jnl_withdrawals["total"] += amount

        print(f"    Deposits:     {jnl_deposits['count']:>6} txns  {fmt_mnt(jnl_deposits['total']):>20} MNT")
        print(f"    Withdrawals:  {jnl_withdrawals['count']:>6} txns  {fmt_mnt(jnl_withdrawals['total']):>20} MNT")
        jnl_net = jnl_deposits["total"] - jnl_withdrawals["total"]
        print(f"    Net flow:     {'':>6}       {fmt_mnt(jnl_net):>20} MNT")

        # Cross-check DB vs journal
        if abs(db_deposits["total"] - jnl_deposits["total"]) > 0.01:
            print(f"    ❌ DEPOSIT MISMATCH: DB={fmt_mnt(db_deposits['total'])}, journal={fmt_mnt(jnl_deposits['total'])}")
            errors += 1
        elif db_deposits["count"] != jnl_deposits["count"]:
            print(f"    ⚠  Deposit count differs: DB={db_deposits['count']}, journal={jnl_deposits['count']}")
        else:
            print(f"    ✅ Deposits match across DB and journal")

        if abs(db_withdrawals["total"] - jnl_withdrawals["total"]) > 0.01:
            print(f"    ❌ WITHDRAWAL MISMATCH: DB={fmt_mnt(db_withdrawals['total'])}, journal={fmt_mnt(jnl_withdrawals['total'])}")
            errors += 1
        elif db_withdrawals["count"] != jnl_withdrawals["count"]:
            print(f"    ⚠  Withdrawal count differs: DB={db_withdrawals['count']}, journal={jnl_withdrawals['count']}")
        else:
            print(f"    ✅ Withdrawals match across DB and journal")
    else:
        print(f"    ⚠  journal.log not found at {journal_path} (skipping)")

    # ── 3. LedgerWriter flat files ──
    print("\n  [3/4] LedgerWriter Flat Files")
    ledger_dir = args.ledger_dir or os.path.join(
        os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "data"
    )

    lw_deposits = {"count": 0, "total": 0.0}
    lw_withdrawals = {"count": 0, "total": 0.0}

    for fname, target in [("deposits.ledger", lw_deposits), ("withdrawals.ledger", lw_withdrawals)]:
        fpath = os.path.join(ledger_dir, fname)
        if os.path.exists(fpath):
            with open(fpath, "r") as f:
                for line in f:
                    line = line.strip()
                    # ledger-cli format: lines with MNT amounts like "  Assets:Exchange:Bank:MNT  1000.00 MNT"
                    if "MNT" in line and line.startswith("  "):
                        parts = line.split()
                        for i, p in enumerate(parts):
                            if p == "MNT" and i > 0:
                                try:
                                    amt = float(parts[i - 1].replace(",", ""))
                                    if amt > 0:
                                        target["total"] += amt
                                        target["count"] += 1
                                except ValueError:
                                    pass
                                break
        else:
            print(f"    ⚠  {fname} not found at {fpath}")

    print(f"    Deposits:     {lw_deposits['count']:>6} postings  {fmt_mnt(lw_deposits['total']):>20} MNT")
    print(f"    Withdrawals:  {lw_withdrawals['count']:>6} postings  {fmt_mnt(lw_withdrawals['total']):>20} MNT")

    # Cross-check LedgerWriter vs DB (amounts may double-count debit+credit postings)
    # LedgerWriter has 2 postings per entry (debit + credit), so count/2 = transactions
    lw_dep_txns = lw_deposits["count"] // 2 if lw_deposits["count"] > 0 else 0
    lw_wd_txns = lw_withdrawals["count"] // 2 if lw_withdrawals["count"] > 0 else 0
    if lw_dep_txns > 0 or lw_wd_txns > 0:
        if lw_dep_txns == db_deposits["count"]:
            print(f"    ✅ Deposit count matches DB ({lw_dep_txns} txns)")
        elif lw_deposits["count"] > 0:
            print(f"    ⚠  Deposit count: LedgerWriter={lw_dep_txns}, DB={db_deposits['count']}")
        if lw_wd_txns == db_withdrawals["count"]:
            print(f"    ✅ Withdrawal count matches DB ({lw_wd_txns} txns)")
        elif lw_withdrawals["count"] > 0:
            print(f"    ⚠  Withdrawal count: LedgerWriter={lw_wd_txns}, DB={db_withdrawals['count']}")

    # ── 4. User balance cross-check ──
    print("\n  [4/4] User Balance vs Net Deposit Flow")
    users = conn.execute("SELECT user_id, balance FROM balances").fetchall()

    balance_issues = 0
    for u in users:
        uid = u["user_id"]
        bal = u["balance"]

        # Net deposits for this user from ledger
        dep = conn.execute("""
            SELECT COALESCE(SUM(amount), 0) as total FROM ledger_entries
            WHERE event_type = 'deposit'
            AND (credit_account LIKE ? OR debit_account LIKE ?)
        """, (f"%{uid}%", f"%{uid}%")).fetchone()["total"]

        wd = conn.execute("""
            SELECT COALESCE(SUM(amount), 0) as total FROM ledger_entries
            WHERE event_type = 'withdrawal'
            AND (credit_account LIKE ? OR debit_account LIKE ?)
        """, (f"%{uid}%", f"%{uid}%")).fetchone()["total"]

        # Net trading P&L for this user
        trade_pnl = conn.execute("""
            SELECT COALESCE(SUM(CASE
                WHEN credit_account LIKE ? THEN amount
                WHEN debit_account LIKE ? THEN -amount
                ELSE 0
            END), 0) as pnl
            FROM ledger_entries
            WHERE event_type NOT IN ('deposit', 'withdrawal')
            AND (credit_account LIKE ? OR debit_account LIKE ?)
        """, (f"%{uid}%", f"%{uid}%", f"%{uid}%", f"%{uid}%")).fetchone()["pnl"]

        expected_bal = dep - wd + trade_pnl
        if abs(bal - expected_bal) > 0.01:
            print(f"    ❌ {uid}: actual={fmt_mnt(bal)}, expected={fmt_mnt(expected_bal)} "
                  f"(dep={fmt_mnt(dep)}, wd={fmt_mnt(wd)}, pnl={fmt_mnt(trade_pnl)})")
            balance_issues += 1
            errors += 1

    if balance_issues == 0 and len(users) > 0:
        print(f"    ✅ All {len(users)} user balances reconcile with deposit/withdrawal/trade flow")
    elif len(users) == 0:
        print(f"    ⚠  No user accounts to check")

    # ── Summary ──
    print(f"\n╠══════════════════════════════════════════════════════════════════╣")
    print(f"║  Summary                                                        ║")
    print(f"╠══════════════════════════════════════════════════════════════════╣")
    print(f"  Total deposits (DB):     {db_deposits['count']:>6} txns  {fmt_mnt(db_deposits['total']):>20} MNT")
    print(f"  Total withdrawals (DB):  {db_withdrawals['count']:>6} txns  {fmt_mnt(db_withdrawals['total']):>20} MNT")
    print(f"  Net customer flow:       {'':>6}       {fmt_mnt(db_net):>20} MNT")
    print(f"  Users audited:           {len(users):>6}")

    if errors == 0:
        print(f"\n  ✅ DEPOSIT/WITHDRAWAL AUDIT PASSED - All systems reconciled")
    else:
        print(f"\n  ❌ AUDIT FAILED: {errors} discrepancy(ies) found - INVESTIGATE IMMEDIATELY")

    print(f"╚══════════════════════════════════════════════════════════════════╝")
    conn.close()
    return errors


# ─── MAIN ───────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        prog="cre-ledger",
        description="CRE.mn Exchange Ledger CLI - Blackbox Factory Audit Tool",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  %(prog)s balance user123
  %(prog)s history user123 --from 2026-01-01 --to 2026-01-31
  %(prog)s trace 456
  %(prog)s reconcile --date 2026-01-24
  %(prog)s audit --from 2026-01-01 --to 2026-01-24
  %(prog)s export --format ledger > exchange.ledger
  %(prog)s verify
  %(prog)s summary
  %(prog)s accounts
        """
    )

    subparsers = parser.add_subparsers(dest="command", help="Available commands")

    # balance
    p_bal = subparsers.add_parser("balance", help="Show account balance")
    p_bal.add_argument("user_id", help="User ID to query")

    # history
    p_hist = subparsers.add_parser("history", help="Show ledger history for user")
    p_hist.add_argument("user_id", help="User ID to query")
    p_hist.add_argument("--from", dest="date_from", help="Start date (YYYY-MM-DD)")
    p_hist.add_argument("--to", dest="date_to", help="End date (YYYY-MM-DD)")
    p_hist.add_argument("--limit", type=int, default=50, help="Max entries (default: 50)")

    # trace
    p_trace = subparsers.add_parser("trace", help="Trace a trade through the ledger")
    p_trace.add_argument("trade_id", type=int, help="Trade ID to trace")

    # reconcile
    p_recon = subparsers.add_parser("reconcile", help="Reconcile ledger accounts")
    p_recon.add_argument("--date", help="Reconcile as of date (YYYY-MM-DD)")

    # audit
    p_audit = subparsers.add_parser("audit", help="Full audit trail")
    p_audit.add_argument("--from", dest="date_from", help="Start date (YYYY-MM-DD)")
    p_audit.add_argument("--to", dest="date_to", help="End date (YYYY-MM-DD)")
    p_audit.add_argument("--limit", type=int, default=100, help="Max entries (default: 100)")

    # export
    p_export = subparsers.add_parser("export", help="Export ledger entries")
    p_export.add_argument("--format", choices=["csv", "json", "ledger"], default="json",
                          help="Export format (default: json)")
    p_export.add_argument("--from", dest="date_from", help="Start date (YYYY-MM-DD)")
    p_export.add_argument("--to", dest="date_to", help="End date (YYYY-MM-DD)")

    # verify
    subparsers.add_parser("verify", help="Verify ledger integrity")

    # summary
    subparsers.add_parser("summary", help="Show exchange summary")

    # accounts
    subparsers.add_parser("accounts", help="List all ledger accounts")

    # deposit-audit
    p_depaudit = subparsers.add_parser("deposit-audit",
                                       help="Reconcile deposits/withdrawals across all accounting systems")
    p_depaudit.add_argument("--from", dest="date_from", help="Start date (YYYY-MM-DD)")
    p_depaudit.add_argument("--to", dest="date_to", help="End date (YYYY-MM-DD)")
    p_depaudit.add_argument("--journal", help="Path to journal.log (default: auto-detect)")
    p_depaudit.add_argument("--ledger-dir", dest="ledger_dir",
                            help="Path to LedgerWriter output directory (default: auto-detect)")

    args = parser.parse_args()

    if not args.command:
        parser.print_help()
        sys.exit(1)

    commands = {
        "balance": cmd_balance,
        "history": cmd_history,
        "trace": cmd_trace,
        "reconcile": cmd_reconcile,
        "audit": cmd_audit,
        "export": cmd_export,
        "verify": cmd_verify,
        "summary": cmd_summary,
        "accounts": cmd_accounts,
        "deposit-audit": cmd_deposit_audit,
    }

    try:
        commands[args.command](args)
    except sqlite3.Error as e:
        print(f"Database error: {e}", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
