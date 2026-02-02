/**
 * WASM Module Loader - Central Exchange
 * Loads and wraps the C++ matching engine compiled to WebAssembly
 * KITSUNE 2026-01-30
 */

class WasmOrderBook {
    constructor() {
        this.module = null;
        this.ready = false;
        this.readyCallbacks = [];
    }

    async load(wasmPath = '/wasm/exchange.wasm') {
        try {
            // Check if already loaded
            if (this.ready) return true;

            // Load WASM module
            const response = await fetch(wasmPath);
            if (!response.ok) {
                console.warn('WASM not available, using JS fallback');
                return false;
            }

            const wasmBuffer = await response.arrayBuffer();

            // Import object for WASM
            const importObject = {
                env: {
                    memory: new WebAssembly.Memory({ initial: 256 }),
                    __memory_base: 0,
                    __table_base: 0,
                    abort: () => console.error('WASM abort called'),
                    // Add any other imports the module needs
                }
            };

            const result = await WebAssembly.instantiate(wasmBuffer, importObject);
            this.module = result.instance.exports;

            // Initialize engine
            if (this.module.init_engine) {
                const initResult = this.module.init_engine();
                if (!initResult) {
                    console.error('Failed to initialize WASM engine');
                    return false;
                }
            }

            this.ready = true;
            console.log('WASM OrderBook engine loaded successfully');

            // Call ready callbacks
            this.readyCallbacks.forEach(cb => cb());
            this.readyCallbacks = [];

            return true;
        } catch (err) {
            console.warn('WASM load failed, using JS fallback:', err.message);
            return false;
        }
    }

    onReady(callback) {
        if (this.ready) {
            callback();
        } else {
            this.readyCallbacks.push(callback);
        }
    }

    // Create order book for symbol
    createBook(symbol) {
        if (!this.ready || !this.module.create_book) return false;
        const symbolPtr = this._allocString(symbol);
        const result = this.module.create_book(symbolPtr);
        this._freeString(symbolPtr);
        return result === 1;
    }

    // Add order
    addOrder(symbol, side, price, quantity, clientId = 'demo') {
        if (!this.ready || !this.module.add_order) return 0;

        const symbolPtr = this._allocString(symbol);
        const clientIdPtr = this._allocString(clientId);

        const orderId = this.module.add_order(
            symbolPtr,
            side === 'buy' ? 0 : 1,
            BigInt(price),  // int64 for price in micromnt
            quantity,
            clientIdPtr
        );

        this._freeString(symbolPtr);
        this._freeString(clientIdPtr);

        return orderId;
    }

    // Cancel order
    cancelOrder(symbol, orderId) {
        if (!this.ready || !this.module.cancel_order) return false;
        const symbolPtr = this._allocString(symbol);
        const result = this.module.cancel_order(symbolPtr, BigInt(orderId));
        this._freeString(symbolPtr);
        return result === 1;
    }

    // Get best bid/ask
    getBBO(symbol) {
        if (!this.ready) return { bid: 0, ask: 0 };

        const symbolPtr = this._allocString(symbol);
        const bid = this.module.get_best_bid ? Number(this.module.get_best_bid(symbolPtr)) : 0;
        const ask = this.module.get_best_ask ? Number(this.module.get_best_ask(symbolPtr)) : 0;
        this._freeString(symbolPtr);

        return { bid, ask };
    }

    // Get orderbook depth
    getDepth(symbol, levels = 10) {
        if (!this.ready || !this.module.get_depth_json) {
            return { bids: [], asks: [] };
        }

        const symbolPtr = this._allocString(symbol);
        const jsonPtr = this.module.get_depth_json(symbolPtr, levels);
        this._freeString(symbolPtr);

        if (!jsonPtr) return { bids: [], asks: [] };

        // Read string from WASM memory
        const json = this._readString(jsonPtr);
        this.module.free_memory(jsonPtr);

        try {
            const data = JSON.parse(json);
            return {
                bids: data.b || [],
                asks: data.a || []
            };
        } catch (e) {
            console.error('Failed to parse depth JSON:', e);
            return { bids: [], asks: [] };
        }
    }

    // Memory helpers for string handling
    _allocString(str) {
        if (!this.module.malloc) return 0;
        const bytes = new TextEncoder().encode(str + '\0');
        const ptr = this.module.malloc(bytes.length);
        const memory = new Uint8Array(this.module.memory.buffer);
        memory.set(bytes, ptr);
        return ptr;
    }

    _freeString(ptr) {
        if (ptr && this.module.free) {
            this.module.free(ptr);
        }
    }

    _readString(ptr) {
        const memory = new Uint8Array(this.module.memory.buffer);
        let end = ptr;
        while (memory[end] !== 0) end++;
        return new TextDecoder().decode(memory.slice(ptr, end));
    }
}

// JS Fallback implementation (when WASM not available)
class JSOrderBook {
    constructor() {
        this.books = {};
    }

    createBook(symbol) {
        if (!this.books[symbol]) {
            this.books[symbol] = { bids: [], asks: [] };
        }
        return true;
    }

    addBid(symbol, price, qty) {
        if (!this.books[symbol]) this.createBook(symbol);
        this.books[symbol].bids.push([price, qty]);
        this.books[symbol].bids.sort((a, b) => b[0] - a[0]); // Descending
        return true;
    }

    addAsk(symbol, price, qty) {
        if (!this.books[symbol]) this.createBook(symbol);
        this.books[symbol].asks.push([price, qty]);
        this.books[symbol].asks.sort((a, b) => a[0] - b[0]); // Ascending
        return true;
    }

    getDepth(symbol, levels = 10) {
        const book = this.books[symbol] || { bids: [], asks: [] };
        return {
            bids: book.bids.slice(0, levels),
            asks: book.asks.slice(0, levels)
        };
    }

    getBBO(symbol) {
        const book = this.books[symbol];
        if (!book) return { bid: 0, ask: 0 };
        return {
            bid: book.bids.length > 0 ? book.bids[0][0] : 0,
            ask: book.asks.length > 0 ? book.asks[0][0] : 0
        };
    }

    // Update from SSE data
    updateFromSSE(symbol, data) {
        if (!this.books[symbol]) this.createBook(symbol);
        if (data.b) this.books[symbol].bids = data.b;
        if (data.a) this.books[symbol].asks = data.a;
    }
}

// Factory function
async function createOrderBook() {
    const wasmBook = new WasmOrderBook();
    const loaded = await wasmBook.load();

    if (loaded) {
        console.log('Using WASM OrderBook (native performance)');
        return wasmBook;
    } else {
        console.log('Using JS OrderBook (fallback)');
        return new JSOrderBook();
    }
}

// Export
if (typeof window !== 'undefined') {
    window.WasmOrderBook = WasmOrderBook;
    window.JSOrderBook = JSOrderBook;
    window.createOrderBook = createOrderBook;
}
