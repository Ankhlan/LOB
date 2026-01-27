// Central Exchange API Types

export interface Product {
  symbol: string;
  name: string;
  base_currency: string;
  quote_currency: string;
  mark_price_mnt: number;
  bid?: number;
  ask?: number;
  is_active: boolean;
}

export interface Quote {
  symbol: string;
  bid: number;
  ask: number;
  spread: number;
  mark_price: number;
  timestamp: number;
}

export interface Order {
  id: string;
  user_id: string;
  symbol: string;
  side: 'buy' | 'sell';
  type: 'limit' | 'market';
  price: number;
  quantity: number;
  filled: number;
  status: 'pending' | 'filled' | 'partial' | 'cancelled';
  created_at: number;
}

export interface Trade {
  id: string;
  symbol: string;
  price: number;
  quantity: number;
  side: 'buy' | 'sell';
  timestamp: number;
}

export interface User {
  id: string;
  phone: string;
  username: string;
  balance_mnt: number;
}

export interface AuthResponse {
  success: boolean;
  token?: string;
  phone?: string;
  error?: string;
  message?: string;
  dev_otp?: string; // For development only
}

export interface OrderBook {
  symbol: string;
  bids: [number, number][]; // [price, qty][]
  asks: [number, number][]; // [price, qty][]
}
