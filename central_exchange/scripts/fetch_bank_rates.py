#!/usr/bin/env python3
"""
Mongolian Commercial Bank FX Rate Scraper v2
============================================

Scrapes live USD/MNT rates from:
- TDB: https://www.tdbm.mn/en/exchange-rates
- Golomt Bank: https://www.golomtbank.com/en/exchange/  
- Khan Bank: https://www.khanbank.com/en/personal/exchange-rates

Output: JSON file for C++ server to read
"""

import requests
import re
import json
import urllib3
from datetime import datetime, timezone
from bs4 import BeautifulSoup
from pathlib import Path

# Suppress SSL warnings for Khan Bank
urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

OUTPUT_FILE = Path(__file__).parent.parent / "data" / "bank_rates.json"

HEADERS = {"User-Agent": "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 Chrome/120.0.0.0 Safari/537.36"}

def parse_number(text):
    """Parse a number from text like 3,564.35 or 3564.35"""
    clean = re.sub(r"[^\d.]", "", text.replace(",", ""))
    try:
        return float(clean) if clean else 0
    except:
        return 0

def fetch_tdb():
    """Scrape TDB rates - most reliable"""
    try:
        url = "https://www.tdbm.mn/en/exchange-rates"
        resp = requests.get(url, headers=HEADERS, timeout=10)
        resp.raise_for_status()
        soup = BeautifulSoup(resp.text, "html.parser")
        
        for table in soup.find_all("table"):
            for row in table.find_all("tr"):
                cells = row.find_all(["td", "th"])
                text = " ".join(c.get_text(strip=True) for c in cells)
                
                if "USD" in text or "United States" in text:
                    numbers = [parse_number(c.get_text()) for c in cells if parse_number(c.get_text()) > 1000]
                    # TDB columns: Mongol Bank | Non-cash Buy | Non-cash Sell | Cash Buy | Cash Sell
                    # numbers[0]=3564.35 (CB rate), numbers[1]=3557 (buy), numbers[2]=3565 (sell)
                    if len(numbers) >= 3:
                        return {
                            "bank_id": "TDB",
                            "bank_name": "Trade and Development Bank",
                            "bid": numbers[1],   # Bank buys USD at this rate
                            "ask": numbers[2],   # Bank sells USD at this rate
                            "cash_bid": numbers[3] if len(numbers) > 3 else numbers[1],
                            "cash_ask": numbers[4] if len(numbers) > 4 else numbers[2],
                            "timestamp": datetime.now(timezone.utc).isoformat(),
                            "source": url
                        }
        return None
    except Exception as e:
        print(f"[ERROR] TDB: {e}")
        return None

def fetch_golomt():
    """Fetch Golomt Bank rates via their API"""
    try:
        # Format date as YYYYMMDD
        today = datetime.now().strftime("%Y%m%d")
        url = f"https://www.golomtbank.com/api/exchange/?date={today}"
        
        headers = {
            "User-Agent": HEADERS["User-Agent"],
            "Accept": "application/json, text/plain, */*",
            "Accept-Language": "en-US,en;q=0.9",
            "Referer": "https://www.golomtbank.com/en/exchange/",
            "Origin": "https://www.golomtbank.com"
        }
        
        resp = requests.get(url, headers=headers, timeout=10)
        resp.raise_for_status()
        data = resp.json()
        
        # Extract USD from API response
        usd = data.get("result", {}).get("USD", {})
        if not usd:
            return None
        
        return {
            "bank_id": "GOLOMT",
            "bank_name": "Golomt Bank",
            "bid": usd.get("non_cash_buy", {}).get("cvalue", 0),
            "ask": usd.get("non_cash_sell", {}).get("cvalue", 0),
            "cash_bid": usd.get("cash_buy", {}).get("cvalue", 0),
            "cash_ask": usd.get("cash_sell", {}).get("cvalue", 0),
            "timestamp": datetime.now(timezone.utc).isoformat(),
            "source": url
        }
    except Exception as e:
        print(f"[ERROR] Golomt: {e}")
        return None

def fetch_xacbank():
    """Scrape XacBank rates from homepage"""
    try:
        url = "https://www.xacbank.mn"
        resp = requests.get(url, headers=HEADERS, timeout=10)
        resp.raise_for_status()
        soup = BeautifulSoup(resp.text, "html.parser")
        
        # XacBank shows rates on homepage: USD | Buy | 3,558.00 | Sell | 3,566.00
        text = soup.get_text()
        # Find USD section - format: USD Авах 3,558.00 Зарах 3,566.00
        import re
        match = re.search(r'USD\s+(?:Авах|Buy)\s+([\d,\.]+)\s+(?:Зарах|Sell)\s+([\d,\.]+)', text)
        if match:
            buy = parse_number(match.group(1))
            sell = parse_number(match.group(2))
            if buy > 1000 and sell > 1000:
                return {
                    "bank_id": "XAC",
                    "bank_name": "Xac Bank",
                    "bid": buy,
                    "ask": sell,
                    "cash_bid": buy,
                    "cash_ask": sell,
                    "timestamp": datetime.now(timezone.utc).isoformat(),
                    "source": url
                }
        return None
    except Exception as e:
        print(f"[ERROR] XacBank: {e}")
        return None

def fetch_all():
    """Fetch rates from all banks"""
    rates = {
        "timestamp": datetime.now(timezone.utc).isoformat(),
        "banks": []
    }
    
    for name, fetcher in [("TDB", fetch_tdb), ("Golomt", fetch_golomt), ("XacBank", fetch_xacbank)]:
        result = fetcher()
        if result:
            rates["banks"].append(result)
            bid = result["bid"]
            ask = result["ask"]
            print(f"[OK] {name}: bid={bid}, ask={ask}")
        else:
            print(f"[FAIL] {name}: no data")
    
    if rates["banks"]:
        bids = [b["bid"] for b in rates["banks"] if b["bid"] > 0]
        asks = [b["ask"] for b in rates["banks"] if b["ask"] > 0]
        rates["best_bid"] = max(bids) if bids else 0
        rates["best_ask"] = min(asks) if asks else 0
        rates["spread"] = rates["best_ask"] - rates["best_bid"]
        rates["mid"] = (rates["best_bid"] + rates["best_ask"]) / 2
    
    return rates

def main():
    print(f"[SCRAPE] Fetching bank rates at {datetime.now()}")
    rates = fetch_all()
    OUTPUT_FILE.parent.mkdir(parents=True, exist_ok=True)
    with open(OUTPUT_FILE, "w") as f:
        json.dump(rates, f, indent=2)
    print(f"[SAVED] {OUTPUT_FILE}")
    best_bid = rates.get("best_bid", 0)
    best_ask = rates.get("best_ask", 0)
    spread = rates.get("spread", 0)
    print(f"[RESULT] Best bid: {best_bid}, Best ask: {best_ask}, Spread: {spread} MNT")
    return rates

if __name__ == "__main__":
    main()
