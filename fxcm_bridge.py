"""
FXCM Bridge - Connect to cortex_daemon for price oracle.
Uses TCP to query live FXCM prices from the daemon.
"""

import socket
import json
from typing import Dict, Optional
from dataclasses import dataclass


@dataclass
class Quote:
    symbol: str
    bid: float
    ask: float
    mid: float


class FxcmBridge:
    """
    Bridge to cortex_daemon for FXCM price data.
    
    The daemon maintains persistent FXCM sessions with 668 instruments
    streaming in real-time. We query it for index prices.
    """
    
    # Map perp symbols to FXCM underlying symbols
    PRODUCT_MAPPING = {
        'BTC-PERP': 'BTC/USD',
        'XAU-PERP': 'XAU/USD',
        'XAG-PERP': 'XAG/USD',
        'EUR-USD-PERP': 'EUR/USD',
        'GBP-USD-PERP': 'GBP/USD',
        'USD-JPY-PERP': 'USD/JPY',
        'AUD-USD-PERP': 'AUD/USD',
        'NZD-USD-PERP': 'NZD/USD',
        'USD-CAD-PERP': 'USD/CAD',
        'USD-CHF-PERP': 'USD/CHF',
        'EUR-JPY-PERP': 'EUR/JPY',
        'GBP-JPY-PERP': 'GBP/JPY',
        'OIL-PERP': 'USOil',
        'NATGAS-PERP': 'NGAS',
        'SPX-PERP': 'SPX500',
        'NDX-PERP': 'NAS100',
    }
    
    def __init__(self, host: str = '127.0.0.1', port: int = 9998, timeout: float = 5.0):
        self.host = host
        self.port = port
        self.timeout = timeout
        self._connected = False
    
    def _send_command(self, cmd: str) -> str:
        """Send command to daemon and get response."""
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                s.settimeout(self.timeout)
                s.connect((self.host, self.port))
                s.sendall(f"{cmd}\n".encode('utf-8'))
                
                # Read response (may be multiple lines)
                response = b""
                while True:
                    chunk = s.recv(4096)
                    if not chunk:
                        break
                    response += chunk
                    if b"\n" in chunk:
                        break
                
                return response.decode('utf-8').strip()
        except socket.error as e:
            raise ConnectionError(f"Failed to connect to daemon: {e}")
    
    def ping(self) -> bool:
        """Check if daemon is responding."""
        try:
            response = self._send_command("PING")
            return "PONG" in response.upper()
        except:
            return False
    
    def get_quote(self, perp_symbol: str) -> Optional[Quote]:
        """
        Get live quote for a perp product.
        
        Args:
            perp_symbol: The perp symbol (e.g., 'XAU-PERP')
        
        Returns:
            Quote with bid/ask/mid or None if not available
        """
        fxcm_symbol = self.PRODUCT_MAPPING.get(perp_symbol, perp_symbol)
        
        try:
            # Command format: CMD FXCM_QUOTE XAU/USD
            response = self._send_command(f"CMD FXCM_QUOTE {fxcm_symbol}")
            data = json.loads(response)
            
            if 'error' in data:
                return None
            
            bid = data.get('bid', 0)
            ask = data.get('ask', 0)
            mid = (bid + ask) / 2 if bid and ask else 0
            
            return Quote(
                symbol=perp_symbol,
                bid=bid,
                ask=ask,
                mid=mid
            )
        except (json.JSONDecodeError, KeyError):
            return None
    
    def get_all_prices(self) -> Dict[str, float]:
        """
        Get all FXCM prices at once.
        
        Returns:
            Dict mapping FXCM symbol to mid price
        """
        try:
            response = self._send_command("CMD FXCM_PRICES")
            return json.loads(response)
        except:
            return {}
    
    def get_index_price(self, perp_symbol: str) -> float:
        """
        Get index price for a perp product.
        
        This is the FXCM mid price used for:
        - Mark price calculation
        - Funding rate basis
        - Liquidation reference
        """
        quote = self.get_quote(perp_symbol)
        return quote.mid if quote else 0.0
    
    def get_index_prices(self) -> Dict[str, float]:
        """
        Get index prices for all supported perp products.
        
        Returns:
            Dict mapping perp symbol to index price
        """
        all_prices = self.get_all_prices()
        result = {}
        
        for perp, fxcm in self.PRODUCT_MAPPING.items():
            if fxcm in all_prices:
                result[perp] = all_prices[fxcm]
        
        return result


# Singleton instance
_bridge: Optional[FxcmBridge] = None


def get_bridge() -> FxcmBridge:
    """Get the singleton FXCM bridge instance."""
    global _bridge
    if _bridge is None:
        import os
        host = os.environ.get('DAEMON_HOST', '127.0.0.1')
        port = int(os.environ.get('DAEMON_PORT', '9998'))
        _bridge = FxcmBridge(host=host, port=port)
    return _bridge


# Convenience functions
def get_index_price(symbol: str) -> float:
    """Get index price for a perp symbol."""
    return get_bridge().get_index_price(symbol)


def get_quote(symbol: str) -> Optional[Quote]:
    """Get quote for a perp symbol."""
    return get_bridge().get_quote(symbol)


def is_daemon_online() -> bool:
    """Check if the daemon is responding."""
    return get_bridge().ping()
