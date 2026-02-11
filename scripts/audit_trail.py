#!/usr/bin/env python3
"""
CRE.mn Audit Trail Query Tool
Query and export audit_log entries from SQLite.

Usage:
    python3 audit_trail.py                          # Show recent 50 events
    python3 audit_trail.py --user USER_ID           # Filter by user
    python3 audit_trail.py --type deposit           # Filter by event type
    python3 audit_trail.py --since 2026-02-01       # Filter by date
    python3 audit_trail.py --export audit.json      # Export to JSON

Created: 2026-02-10 by LEDGER
"""

import sqlite3
import json
import sys
import os
from datetime import datetime

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
LOB_DIR = os.path.dirname(SCRIPT_DIR)
DB_PATH = os.environ.get('CRE_DB',
    os.environ.get('DB_PATH', '/var/lib/exchange/exchange.db'))


def query_audit_log(db_path, user_id=None, event_type=None, since=None, limit=50):
    """Query audit_log table with optional filters."""
    if not os.path.exists(db_path):
        print(f"Database not found: {db_path}")
        print("Set CRE_DB or DB_PATH environment variable, or ensure exchange is running.")
        return []

    conn = sqlite3.connect(db_path)
    conn.row_factory = sqlite3.Row
    cur = conn.cursor()

    query = "SELECT * FROM audit_log WHERE 1=1"
    params = []

    if user_id:
        query += " AND user_id = ?"
        params.append(user_id)

    if event_type:
        query += " AND event_type = ?"
        params.append(event_type)

    if since:
        # Convert date string to epoch ms
        dt = datetime.strptime(since, '%Y-%m-%d')
        epoch_ms = int(dt.timestamp() * 1000)
        query += " AND timestamp >= ?"
        params.append(epoch_ms)

    query += " ORDER BY timestamp DESC LIMIT ?"
    params.append(limit)

    cur.execute(query, params)
    rows = [dict(r) for r in cur.fetchall()]
    conn.close()
    return rows


def format_timestamp(ts_ms):
    """Convert epoch milliseconds to human-readable format."""
    try:
        dt = datetime.fromtimestamp(ts_ms / 1000)
        return dt.strftime('%Y-%m-%d %H:%M:%S')
    except (ValueError, OSError):
        return str(ts_ms)


def print_audit_log(rows):
    """Pretty print audit log entries."""
    if not rows:
        print("No audit entries found.")
        return

    print(f"\n{'═' * 80}")
    print(f" CRE.mn AUDIT TRAIL — {len(rows)} entries")
    print(f"{'═' * 80}\n")

    for row in rows:
        ts = format_timestamp(row['timestamp'])
        event = row['event_type']
        user = row.get('user_id', '-')
        data = row.get('data', '')

        # Parse JSON data if possible
        try:
            parsed = json.loads(data) if data else {}
            data_str = json.dumps(parsed, indent=None)
        except json.JSONDecodeError:
            data_str = data

        # Color-code by event type
        icon = {
            'deposit': '💰',
            'deposit_qpay': '💳',
            'withdrawal': '📤',
            'trade': '📊',
            'kyc_approved': '✅',
            'kyc_rejected': '❌',
            'kyc_submitted': '📝',
            'login': '🔑',
            'order_placed': '📋',
        }.get(event, '📒')

        print(f"  {icon} [{ts}] {event.upper():20s} user={user}")
        if data_str and data_str != '{}':
            print(f"     └─ {data_str}")
        print()


if __name__ == '__main__':
    import argparse

    parser = argparse.ArgumentParser(description='CRE.mn Audit Trail Query')
    parser.add_argument('--db', help='SQLite database path', default=DB_PATH)
    parser.add_argument('--user', help='Filter by user ID')
    parser.add_argument('--type', dest='event_type', help='Filter by event type')
    parser.add_argument('--since', help='Filter events since date (YYYY-MM-DD)')
    parser.add_argument('--limit', type=int, default=50, help='Max entries (default: 50)')
    parser.add_argument('--export', help='Export to JSON file')
    parser.add_argument('--json', action='store_true', help='Output as JSON')

    args = parser.parse_args()

    rows = query_audit_log(args.db, args.user, args.event_type, args.since, args.limit)

    if args.export:
        with open(args.export, 'w') as f:
            json.dump(rows, f, indent=2)
        print(f"Exported {len(rows)} entries to {args.export}")
    elif args.json:
        print(json.dumps(rows, indent=2))
    else:
        print_audit_log(rows)
