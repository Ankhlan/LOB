#!/usr/bin/env python3
"""
Mongolian Commercial Bank FX Rate Scraper
==========================================

Scrapes live USD/MNT rates from:
- Golomt Bank: https://www.golomtbank.com/en/exchange/
- TDB: https://www.tdbm.mn/en/exchange-rates
- Khan Bank: https://www.khanbank.com/en/

Output: JSON file for C++ server to read
"""

import requests
import re
import json
import time
from datetime import datetime
from bs4 import BeautifulSoup
from pathlib import Path

# Output file for C++ server to read
OUTPUT_FILE = Path(__file__).parent.parent / "data" / "bank_rates.json"

def fetch_golomt():
    """Scrape Golomt Bank rates"""
    try:
        url = "https://www.golomtbank.com/en/exchange/"
        headers = {"User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) Chrome/120.0"}
        resp = requests.get(url, headers=headers, timeout=10)
        resp.raise_for_status()
        
        soup = BeautifulSoup(resp.text, 'html.parser')
        
        # Find USD row in rate table
        # Looking for pattern: USD | official | buy | sell | cash_buy | cash_sell
        tables = soup.find_all('table')
        
        for table in tables:
            rows = table.find_all('tr')
            for row in rows:
                cells = row.find_all(['td', 'th'])
                cell_text = [c.get_text(strip=True) for c in cells]
                
                # Look for USD row
                if any('USD' in t for t in cell_text):
                    # Parse numbers
                    numbers = []
                    for t in cell_text:
                        # Clean and parse numbers like "3,564.35"
                        clean = re.sub(r'[^\d.]', '', t.replace(',', ''))
                        if clean:
                            try:
                                numbers.append(float(clean))
                            except:
                                pass
                    
                    # Format: [official, buy, sell, cash_buy, cash_sell]
                    if len(numbers) >= 3:
                        return {
                            "bank_id": "GOLOMT",
                            "bank_name": "Golomt Bank",
                            "bid": numbers[1] if len(numbers) > 1 else 0,  # Non-cash buy
                            "ask": numbers[2] if len(numbers) > 2 else 0,  # Non-cash sell
                            "cash_bid": numbers[3] if len(numbers) > 3 else 0,
                            "cash_ask": numbers[4] if len(numbers) > 4 else 0,
                            "timestamp": datetime.utcnow().isoformat(),
                            "source": url
                        }
        
        return None
    except Exception as e:
        print(f"[ERROR] Golomt: {e}")
        return None

def fetch_tdb():
    """Scrape TDB rates"""
    try:
        url = "https://www.tdbm.mn/en/exchange-rates"
        headers = {"User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) Chrome/120.0"}
        resp = requests.get(url, headers=headers, timeout=10)
        resp.raise_for_status()
        
        soup = BeautifulSoup(resp.text, 'html.parser')
        
        # Find USD row
        tables = soup.find_all('table')
        
        for table in tables:
            rows = table.find_all('tr')
            for row in rows:
                cells = row.find_all(['td', 'th'])
                cell_text = [c.get_text(strip=True) for c in cells]
                
                if any('USD' in t or 'United States Dollar' in t for t in cell_text):
                    numbers = []
                    for t in cell_text:
                        clean = re.sub(r'[^\d.]', '', t.replace(',', ''))
                        if clean:
                            try:
                                numbers.append(float(clean))
                            except:
                                pass
                    
                    # Format: [official, non_cash_buy, non_cash_sell, cash_buy, cash_sell]
                    if len(numbers) >= 3:
                        return {
                            "bank_id": "TDB",
                            "bank_name": "Trade and Development Bank",
                            "bid": numbers[1] if len(numbers) > 1 else 0,
                            "ask": numbers[2] if len(numbers) > 2 else 0,
                            "cash_bid": numbers[3] if len(numbers) > 3 else 0,
                            "cash_ask": numbers[4] if len(numbers) > 4 else 0,
                            "timestamp": datetime.utcnow().isoformat(),
                            "source": url
                        }
        
        return None
    except Exception as e:
        print(f"[ERROR] TDB: {e}")
        return None

def fetch_all():
    """Fetch rates from all banks"""
    rates = {
        "timestamp": datetime.utcnow().isoformat(),
        "banks": []
    }
    
    # Golomt
    golomt = fetch_golomt()
    if golomt:
        rates["banks"].append(golomt)
        print(f"[OK] Golomt: bid={golomt['bid']}, ask={golomt['ask']}")
    
    # TDB
    tdb = fetch_tdb()
    if tdb:
        rates["banks"].append(tdb)
        print(f"[OK] TDB: bid={tdb['bid']}, ask={tdb['ask']}")
    
    # Calculate best rates
    if rates["banks"]:
        bids = [b["bid"] for b in rates["banks"] if b["bid"] > 0]
        asks = [b["ask"] for b in rates["banks"] if b["ask"] > 0]
        
        rates["best_bid"] = max(bids) if bids else 0
        rates["best_ask"] = min(asks) if asks else 0
        rates["spread"] = rates["best_ask"] - rates["best_bid"]
        rates["mid"] = (rates["best_bid"] + rates["best_ask"]) / 2
    
    return rates

def main():
    """Main entry point"""
    print(f"[SCRAPE] Fetching bank rates at {datetime.now()}")
    
    rates = fetch_all()
    
    # Ensure output directory exists
    OUTPUT_FILE.parent.mkdir(parents=True, exist_ok=True)
    
    # Write JSON
    with open(OUTPUT_FILE, 'w') as f:
        json.dump(rates, f, indent=2)
    
    print(f"[SAVED] {OUTPUT_FILE}")
    print(f"[RESULT] Best bid: {rates.get('best_bid', 0)}, Best ask: {rates.get('best_ask', 0)}")
    
    return rates

if __name__ == "__main__":
    main()
