/**
 * WASM Exports for Central Exchange
 * 
 * Exports matching engine functions to JavaScript/TypeScript.
 * Build: emcmake cmake -DWASM_BUILD=ON .. && emmake make
 */

#include "order_book.h"
#include "matching_engine.h"
#include "interfaces.h"

#include <emscripten.h>
#include <string>

using namespace central_exchange;

// ============================================================================
// EXPORTED FUNCTIONS (callable from JS)
// ============================================================================

extern "C" {

// Initialize matching engine
EMSCRIPTEN_KEEPALIVE
int init_engine() {
    try {
        MatchingEngine::instance();
        return 1;  // Success
    } catch (...) {
        return 0;
    }
}

// Create order book for symbol
EMSCRIPTEN_KEEPALIVE
int create_book(const char* symbol) {
    try {
        // Create book if not exists
        return 1;
    } catch (...) {
        return 0;
    }
}

// Add order
// Returns order_id or 0 on failure
EMSCRIPTEN_KEEPALIVE
uint64_t add_order(
    const char* symbol,
    int side,           // 0=buy, 1=sell
    int64_t price,      // In micromnt
    double quantity,
    const char* client_id
) {
    try {
        Order order;
        order.id = 0;  // Will be assigned
        order.symbol = symbol;
        order.side = side == 0 ? Side::Buy : Side::Sell;
        order.price = price;
        order.quantity = quantity;
        order.remaining = quantity;
        order.client_id = client_id;
        order.status = OrderStatus::New;
        
        auto result = MatchingEngine::instance().submit(order);
        return result.order_id;
    } catch (...) {
        return 0;
    }
}

// Cancel order
EMSCRIPTEN_KEEPALIVE
int cancel_order(const char* symbol, uint64_t order_id) {
    try {
        return MatchingEngine::instance().cancel(symbol, order_id) ? 1 : 0;
    } catch (...) {
        return 0;
    }
}

// Get best bid price (micromnt)
EMSCRIPTEN_KEEPALIVE
int64_t get_best_bid(const char* symbol) {
    try {
        auto [bid, ask] = MatchingEngine::instance().get_bbo(symbol);
        return bid.value_or(0);
    } catch (...) {
        return 0;
    }
}

// Get best ask price (micromnt)
EMSCRIPTEN_KEEPALIVE
int64_t get_best_ask(const char* symbol) {
    try {
        auto [bid, ask] = MatchingEngine::instance().get_bbo(symbol);
        return ask.value_or(0);
    } catch (...) {
        return 0;
    }
}

// Get orderbook depth as JSON string
// Caller must free() the result
EMSCRIPTEN_KEEPALIVE
const char* get_depth_json(const char* symbol, int levels) {
    try {
        auto depth = MatchingEngine::instance().get_depth(symbol, levels);
        
        // Build compact JSON manually (no nlohmann in WASM build)
        std::string json = "{\"b\":[";
        bool first = true;
        for (const auto& [price, qty] : depth.bids) {
            if (!first) json += ",";
            json += "[" + std::to_string(price) + "," + std::to_string(qty) + "]";
            first = false;
        }
        json += "],\"a\":[";
        first = true;
        for (const auto& [price, qty] : depth.asks) {
            if (!first) json += ",";
            json += "[" + std::to_string(price) + "," + std::to_string(qty) + "]";
            first = false;
        }
        json += "]}";
        
        // Allocate on heap for JS to free
        char* result = (char*)malloc(json.size() + 1);
        strcpy(result, json.c_str());
        return result;
    } catch (...) {
        return nullptr;
    }
}

// Free memory allocated by WASM
EMSCRIPTEN_KEEPALIVE
void free_memory(void* ptr) {
    free(ptr);
}

}  // extern "C"
