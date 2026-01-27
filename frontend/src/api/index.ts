// Central Exchange API Service

import type { AuthResponse, Product, Quote, Order, Trade, User } from './types';

const API_BASE = import.meta.env.VITE_API_URL || 'https://central-exchange-production.up.railway.app';

class ApiService {
  private token: string | null = null;

  constructor() {
    this.token = localStorage.getItem('ce_token');
  }

  private headers(): HeadersInit {
    const h: HeadersInit = {
      'Content-Type': 'application/json',
    };
    if (this.token) {
      h['Authorization'] = `Bearer ${this.token}`;
    }
    return h;
  }

  setToken(token: string | null) {
    this.token = token;
    if (token) {
      localStorage.setItem('ce_token', token);
    } else {
      localStorage.removeItem('ce_token');
    }
  }

  getToken(): string | null {
    return this.token;
  }

  // ==================== AUTH ====================

  async requestOtp(phone: string): Promise<AuthResponse> {
    const res = await fetch(`${API_BASE}/api/auth/request-otp`, {
      method: 'POST',
      headers: this.headers(),
      body: JSON.stringify({ phone }),
    });
    return res.json();
  }

  async verifyOtp(phone: string, otp: string): Promise<AuthResponse> {
    const res = await fetch(`${API_BASE}/api/auth/verify-otp`, {
      method: 'POST',
      headers: this.headers(),
      body: JSON.stringify({ phone, otp }),
    });
    const data = await res.json();
    if (data.success && data.token) {
      this.setToken(data.token);
    }
    return data;
  }

  async getMe(): Promise<User | null> {
    if (!this.token) return null;
    try {
      const res = await fetch(`${API_BASE}/api/auth/me`, {
        headers: this.headers(),
      });
      if (!res.ok) return null;
      return res.json();
    } catch {
      return null;
    }
  }

  logout() {
    this.setToken(null);
  }

  // ==================== PRODUCTS ====================

  async getProducts(): Promise<Product[]> {
    const res = await fetch(`${API_BASE}/api/products`);
    return res.json();
  }

  async getQuote(symbol: string): Promise<Quote> {
    const res = await fetch(`${API_BASE}/api/quote/${symbol}`);
    return res.json();
  }

  // ==================== ORDERS ====================

  async placeOrder(order: {
    symbol: string;
    side: 'buy' | 'sell';
    type: 'limit' | 'market';
    price?: number;
    quantity: number;
  }): Promise<{ success: boolean; order_id?: string; error?: string }> {
    const res = await fetch(`${API_BASE}/api/orders`, {
      method: 'POST',
      headers: this.headers(),
      body: JSON.stringify(order),
    });
    return res.json();
  }

  async getOrderHistory(): Promise<Order[]> {
    const res = await fetch(`${API_BASE}/api/orders/history`, {
      headers: this.headers(),
    });
    return res.json();
  }

  async cancelOrder(orderId: string): Promise<{ success: boolean }> {
    const res = await fetch(`${API_BASE}/api/orders/${orderId}`, {
      method: 'DELETE',
      headers: this.headers(),
    });
    return res.json();
  }

  // ==================== TRADES ====================

  async getTradeHistory(): Promise<Trade[]> {
    const res = await fetch(`${API_BASE}/api/trades/history`);
    return res.json();
  }

  // ==================== STATS ====================

  async getStats(): Promise<{ order_books: number; total_trades: number; total_volume_mnt: number }> {
    const res = await fetch(`${API_BASE}/api/stats`);
    return res.json();
  }

  // ==================== SSE STREAMING ====================

  subscribeToQuotes(onQuote: (quotes: Quote[]) => void): () => void {
    const eventSource = new EventSource(`${API_BASE}/api/stream`);
    
    eventSource.onmessage = (event) => {
      try {
        const data = JSON.parse(event.data);
        if (Array.isArray(data)) {
          onQuote(data);
        }
      } catch (e) {
        console.error('Failed to parse SSE data:', e);
      }
    };

    eventSource.onerror = (e) => {
      console.error('SSE error:', e);
    };

    return () => eventSource.close();
  }
}

export const api = new ApiService();
