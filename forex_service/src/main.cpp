/*
 * main.cpp - LOB Forex Service
 * 
 * Resells FXCM products to Mongolian market with:
 * - Real-time streaming prices
 * - MNT-quoted products (Gold, Silver, Copper, FX, Indices)
 * - Spread markup for revenue
 * - HTTP API for web frontend
 * 
 * Build:
 *   cmake -B build -S .
 *   cmake --build build --config Release
 * 
 * Usage:
 *   lob_forex_service.exe --user DEMO --pass PASSWORD --http-port 8080
 */

#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <thread>
#include <chrono>
#include <atomic>
#include <csignal>
#include <unordered_map>

#include "fxcm_session.h"
#include "http_server.h"
#include "product_catalog.h"
#include "logger.h"

// Global state
static std::atomic<bool> gRunning{true};
static FxcmSession* gFxcm = nullptr;
static double gUsdMntRate = 3450.0;  // Default USD/MNT rate

// Signal handler
void signalHandler(int sig) {
    LOG_INFO("Shutting down...");
    gRunning = false;
}

// Convert USD price to MNT
double toMnt(double usdPrice) {
    return usdPrice * gUsdMntRate;
}

// Simple JSON builder
class Json {
public:
    Json() : mFirst(true) { mSs << "{"; }
    
    Json& add(const std::string& key, const std::string& val) {
        if (!mFirst) mSs << ",";
        mFirst = false;
        mSs << "\"" << key << "\":\"" << val << "\"";
        return *this;
    }
    
    Json& add(const std::string& key, double val) {
        if (!mFirst) mSs << ",";
        mFirst = false;
        mSs << "\"" << key << "\":" << std::fixed << std::setprecision(4) << val;
        return *this;
    }
    
    Json& add(const std::string& key, int val) {
        if (!mFirst) mSs << ",";
        mFirst = false;
        mSs << "\"" << key << "\":" << val;
        return *this;
    }
    
    Json& add(const std::string& key, bool val) {
        if (!mFirst) mSs << ",";
        mFirst = false;
        mSs << "\"" << key << "\":" << (val ? "true" : "false");
        return *this;
    }
    
    std::string str() const {
        return mSs.str() + "}";
    }
    
private:
    std::ostringstream mSs;
    bool mFirst;
};

// JSON array builder
class JsonArray {
public:
    JsonArray() : mFirst(true) { mSs << "["; }
    
    JsonArray& add(const std::string& item) {
        if (!mFirst) mSs << ",";
        mFirst = false;
        mSs << item;
        return *this;
    }
    
    std::string str() const {
        return mSs.str() + "]";
    }
    
private:
    std::ostringstream mSs;
    bool mFirst;
};

// Get quote for a product (with MNT conversion and spread markup)
std::string getQuote(const std::string& symbol) {
    const Product* prod = findProduct(symbol);
    if (!prod) {
        return "{\"error\":\"Product not found\"}";
    }
    
    if (!gFxcm || !gFxcm->isConnected()) {
        return "{\"error\":\"FXCM not connected\"}";
    }
    
    auto quote = gFxcm->getQuote(prod->fxcmSymbol);
    if (quote.mid <= 0) {
        return "{\"error\":\"No price available\"}";
    }
    
    // Apply spread markup
    double spreadPips = prod->spreadMarkup * prod->tickSize;
    double bid = quote.bid - spreadPips / 2;
    double ask = quote.ask + spreadPips / 2;
    double mid = (bid + ask) / 2;
    
    // Convert to MNT if needed
    if (prod->isMNTQuoted) {
        bid = toMnt(bid);
        ask = toMnt(ask);
        mid = toMnt(mid);
    }
    
    return Json()
        .add("symbol", symbol)
        .add("name", prod->name)
        .add("bid", bid)
        .add("ask", ask)
        .add("mid", mid)
        .add("spread", ask - bid)
        .add("currency", prod->isMNTQuoted ? "MNT" : "USD")
        .add("underlying", prod->fxcmSymbol)
        .str();
}

// Get product catalog
std::string getProducts() {
    JsonArray arr;
    
    for (const auto& prod : getProductCatalog()) {
        if (!prod.isActive) continue;
        
        std::string categoryStr;
        switch (prod.category) {
            case ProductCategory::COMMODITY: categoryStr = "Commodity"; break;
            case ProductCategory::FX_MAJOR: categoryStr = "FX Major"; break;
            case ProductCategory::FX_MINOR: categoryStr = "FX Minor"; break;
            case ProductCategory::FX_EXOTIC: categoryStr = "FX Exotic"; break;
            case ProductCategory::INDEX: categoryStr = "Index"; break;
            case ProductCategory::CRYPTO: categoryStr = "Crypto"; break;
        }
        
        arr.add(Json()
            .add("symbol", prod.symbol)
            .add("name", prod.name)
            .add("description", prod.description)
            .add("category", categoryStr)
            .add("underlying", prod.fxcmSymbol)
            .add("contractSize", prod.contractSize)
            .add("tickSize", prod.tickSize)
            .add("minOrder", prod.minOrderSize)
            .add("maxOrder", prod.maxOrderSize)
            .add("marginRate", prod.marginRate)
            .add("mntQuoted", prod.isMNTQuoted)
            .str());
    }
    
    return arr.str();
}

// Get account summary
std::string getAccount() {
    if (!gFxcm || !gFxcm->isConnected()) {
        return "{\"error\":\"FXCM not connected\"}";
    }
    
    auto acc = gFxcm->getAccountInfo();
    
    return Json()
        .add("balance", acc.balance)
        .add("balanceMnt", toMnt(acc.balance))
        .add("equity", acc.equity)
        .add("equityMnt", toMnt(acc.equity))
        .add("usedMargin", acc.usedMargin)
        .add("usableMargin", acc.usableMargin)
        .add("usdMntRate", gUsdMntRate)
        .str();
}

// Get all streaming prices
std::string getAllPrices() {
    if (!gFxcm || !gFxcm->isConnected()) {
        return "{\"error\":\"FXCM not connected\"}";
    }
    
    JsonArray arr;
    
    for (const auto& prod : getProductCatalog()) {
        if (!prod.isActive) continue;
        
        auto quote = gFxcm->getQuote(prod.fxcmSymbol);
        if (quote.mid <= 0) continue;
        
        double mid = prod.isMNTQuoted ? toMnt(quote.mid) : quote.mid;
        
        arr.add(Json()
            .add("symbol", prod.symbol)
            .add("price", mid)
            .add("currency", prod.isMNTQuoted ? "MNT" : "USD")
            .str());
    }
    
    return arr.str();
}

// Get health status
std::string getHealth() {
    return Json()
        .add("status", gFxcm && gFxcm->isConnected() ? "connected" : "disconnected")
        .add("service", "LOB Forex Service")
        .add("version", "0.1.0")
        .add("market", "Mongolia")
        .str();
}

// Landing page HTML
std::string getLandingPage() {
    std::ostringstream html;
    html << R"(<!DOCTYPE html>
<html>
<head>
    <title>LOB Forex Service - Mongolia</title>
    <meta charset="UTF-8">
    <style>
        body { font-family: 'Segoe UI', sans-serif; background: #1a1a2e; color: #eee; margin: 0; padding: 20px; }
        h1 { color: #ffc107; }
        .container { max-width: 1200px; margin: 0 auto; }
        .grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(280px, 1fr)); gap: 15px; }
        .card { background: #16213e; padding: 15px; border-radius: 8px; border-left: 4px solid #ffc107; }
        .card h3 { margin: 0 0 10px 0; color: #ffc107; }
        .card .price { font-size: 1.5em; font-family: monospace; }
        .card .currency { color: #888; font-size: 0.9em; }
        .status { padding: 10px; background: #0f3460; border-radius: 4px; margin-bottom: 20px; }
        .status.connected { border-left: 4px solid #4caf50; }
        .status.disconnected { border-left: 4px solid #f44336; }
        .bid { color: #4caf50; }
        .ask { color: #f44336; }
        .category { font-size: 0.8em; color: #888; text-transform: uppercase; margin-bottom: 5px; }
        .footer { margin-top: 30px; color: #666; font-size: 0.9em; text-align: center; }
    </style>
</head>
<body>
    <div class="container">
        <h1>🇲🇳 LOB Forex Service - Mongolia Market</h1>
        <div class="status" id="status">Connecting...</div>
        <h2>Live Prices (MNT)</h2>
        <div class="grid" id="prices"></div>
        <div class="footer">
            <p>Powered by FXCM | Real-time streaming prices</p>
            <p>API: <code>/api/quote/:symbol</code> | <code>/api/products</code> | <code>/api/prices</code></p>
        </div>
    </div>
    <script>
        async function updatePrices() {
            try {
                const res = await fetch('/api/prices');
                const prices = await res.json();
                
                if (prices.error) {
                    document.getElementById('status').className = 'status disconnected';
                    document.getElementById('status').textContent = 'Disconnected: ' + prices.error;
                    return;
                }
                
                document.getElementById('status').className = 'status connected';
                document.getElementById('status').textContent = '✓ Connected - ' + prices.length + ' products streaming';
                
                const html = prices.map(p => `
                    <div class="card">
                        <h3>${p.symbol}</h3>
                        <div class="price">${p.currency} ${p.price.toLocaleString('en-US', {minimumFractionDigits: 2})}</div>
                    </div>
                `).join('');
                
                document.getElementById('prices').innerHTML = html;
            } catch (e) {
                document.getElementById('status').className = 'status disconnected';
                document.getElementById('status').textContent = 'Error: ' + e.message;
            }
        }
        
        updatePrices();
        setInterval(updatePrices, 1000);
    </script>
</body>
</html>)";
    return html.str();
}

// Parse command line arguments
struct Config {
    std::string user = "";
    std::string pass = "";
    std::string server = "Demo";
    int httpPort = 8080;
    bool verbose = false;
};

Config parseArgs(int argc, char* argv[]) {
    Config cfg;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "--user" && i + 1 < argc) {
            cfg.user = argv[++i];
        } else if (arg == "--pass" && i + 1 < argc) {
            cfg.pass = argv[++i];
        } else if (arg == "--server" && i + 1 < argc) {
            cfg.server = argv[++i];
        } else if (arg == "--http-port" && i + 1 < argc) {
            cfg.httpPort = std::stoi(argv[++i]);
        } else if (arg == "-v" || arg == "--verbose") {
            cfg.verbose = true;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "LOB Forex Service - Resell FXCM to Mongolia\n\n"
                      << "Usage: lob_forex_service.exe [options]\n\n"
                      << "Options:\n"
                      << "  --user USER       FXCM username\n"
                      << "  --pass PASS       FXCM password\n"
                      << "  --server SERVER   Demo or Real (default: Demo)\n"
                      << "  --http-port PORT  HTTP port (default: 8080)\n"
                      << "  -v, --verbose     Verbose logging\n"
                      << "  -h, --help        Show this help\n";
            exit(0);
        }
    }
    
    // Try environment variables
    if (cfg.user.empty()) {
        const char* env = std::getenv("FXCM_USER");
        if (env) cfg.user = env;
    }
    if (cfg.pass.empty()) {
        const char* env = std::getenv("FXCM_PASS");
        if (env) cfg.pass = env;
    }
    
    return cfg;
}

int main(int argc, char* argv[]) {
    std::cout << "\n";
    std::cout << "==============================================\n";
    std::cout << "  LOB FOREX SERVICE v0.1.0\n";
    std::cout << "  Reselling FXCM to Mongolian Market\n";
    std::cout << "==============================================\n\n";
    
    // Parse args
    Config cfg = parseArgs(argc, argv);
    
    if (cfg.user.empty() || cfg.pass.empty()) {
        std::cerr << "Error: FXCM credentials required.\n";
        std::cerr << "Use --user and --pass, or set FXCM_USER and FXCM_PASS env vars.\n";
        return 1;
    }
    
    // Setup signal handler
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    // Connect to FXCM
    LOG_INFO("Connecting to FXCM (" << cfg.server << ")...");
    
    gFxcm = new FxcmSession();
    if (!gFxcm->login(cfg.user, cfg.pass, cfg.server)) {
        LOG_ERROR("Failed to connect to FXCM");
        delete gFxcm;
        return 1;
    }
    
    LOG_INFO("FXCM connected successfully");
    
    // Start live streaming
    gFxcm->startLive();
    
    // Get instrument count
    auto prices = gFxcm->getLivePrices();
    LOG_INFO("Streaming " << prices.size() << " FXCM instruments");
    
    // Setup HTTP server
    HttpServer http(cfg.httpPort);
    
    // Routes
    http.get("/", [](const HttpRequest& req, HttpResponse& res) {
        res.html(getLandingPage());
    });
    
    http.get("/api/health", [](const HttpRequest& req, HttpResponse& res) {
        res.json(getHealth());
    });
    
    http.get("/api/products", [](const HttpRequest& req, HttpResponse& res) {
        res.json(getProducts());
    });
    
    http.get("/api/prices", [](const HttpRequest& req, HttpResponse& res) {
        res.json(getAllPrices());
    });
    
    http.get("/api/account", [](const HttpRequest& req, HttpResponse& res) {
        res.json(getAccount());
    });
    
    http.get("/api/quote", [](const HttpRequest& req, HttpResponse& res) {
        // Extract symbol from path: /api/quote/XAU-MNT
        std::string path = req.path;
        size_t lastSlash = path.rfind('/');
        if (lastSlash != std::string::npos && lastSlash < path.size() - 1) {
            std::string symbol = path.substr(lastSlash + 1);
            res.json(getQuote(symbol));
        } else {
            res.error(400, "Symbol required: /api/quote/:symbol");
        }
    });
    
    // Start HTTP server
    if (!http.start()) {
        LOG_ERROR("Failed to start HTTP server");
        gFxcm->logout();
        delete gFxcm;
        return 1;
    }
    
    LOG_INFO("HTTP server running on http://localhost:" << cfg.httpPort);
    LOG_INFO("Products: " << getProductCatalog().size() << " MNT-quoted instruments");
    LOG_INFO("Press Ctrl+C to exit");
    
    // Main loop
    while (gRunning) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        // Periodically update USD/MNT rate (could fetch from external API)
        // For now using static rate
    }
    
    // Cleanup
    LOG_INFO("Shutting down...");
    http.stop();
    gFxcm->stopLive();
    gFxcm->logout();
    delete gFxcm;
    
    LOG_INFO("Goodbye!");
    return 0;
}
