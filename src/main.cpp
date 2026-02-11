/**
 * Central Exchange - Main Entry Point
 * =====================================
 * 
 * Mongolia's First Digital Derivatives Exchange
 * Build: v4.0.0 - 2026-01-27
 * 
 * Features:
 * - 18+ products: Commodities, FX, Indices, Crypto (MNT-quoted)
 * - Mongolia-unique perpetuals: Meat, Real Estate, Cashmere, Coal
 * - FXCM-backed pricing and hedging
 * - High-performance C++ matching engine
 * - REST API for Web and Mobile clients
 */

#include "product_catalog.h"
#include "matching_engine.h"
#include "position_manager.h"
#include "fxcm_feed.h"
#include "fxcm_history.h"
#include "bom_feed.h"
#include "usdmnt_market.h"
#include "http_server.h"
#include "database.h"
#include "accounting_engine.h"
#include "rate_limiter.h"

#include <iostream>
#include <csignal>
#include <cstdlib>
#include <thread>

using namespace central_exchange;

// Global server instance for signal handling
HttpServer* g_server = nullptr;

void signal_handler(int signal) {
    std::cout << "\n[SHUTDOWN] Signal " << signal << " received. Shutting down...\n";
    std::cout << "[SHUTDOWN] Flushing candle data to SQLite...\n";
    CandleStore::instance().flush_to_db();
    if (g_server) {
        g_server->stop();
    }
}

void print_banner() {
    std::cout << R"(
╔═══════════════════════════════════════════════════════════════╗
║                                                               ║
║     🇲🇳  CENTRAL EXCHANGE - MONGOLIA                          ║
║                                                               ║
║     First Digital Derivatives Exchange                        ║
║     MNT-Quoted Products | FXCM Backed | C++ Engine            ║
║                                                               ║
╚═══════════════════════════════════════════════════════════════╝
)" << std::endl;
}

void print_products() {
    std::cout << "\n=== Active Products ===\n";
    std::cout << std::left;
    
    auto print_category = [](ProductCategory cat, const char* name) {
        auto products = ProductCatalog::instance().get_by_category(cat);
        if (products.empty()) return;
        
        std::cout << "\n" << name << ":\n";
        for (const auto* p : products) {
            std::cout << "  " << std::setw(20) << p->symbol 
                      << " | " << std::setw(25) << p->name
                      << " | Mark: " << std::setw(15) << std::fixed << std::setprecision(0) << p->mark_price_mnt << " MNT"
                      << " | Margin: " << std::setprecision(0) << (p->margin_rate * 100) << "%"
                      << "\n";
        }
    };
    
    print_category(ProductCategory::FXCM_COMMODITY, "COMMODITIES (FXCM-backed)");
    print_category(ProductCategory::FXCM_FX_LOCAL, "FX PAIRS (MNT-quoted)");
    print_category(ProductCategory::FXCM_INDEX, "INDICES (FXCM-backed)");
    print_category(ProductCategory::FXCM_CRYPTO, "CRYPTO (FXCM-backed)");
    print_category(ProductCategory::MN_PERPETUAL, "MONGOLIA PERPETUALS (Unique)");
    
    std::cout << "\n";
}

int main(int argc, char* argv[]) {
    print_banner();
    
    // Parse arguments (fall back to environment variables)
    int port = 8080;
    
    // FXCM credentials from env vars or command line
    const char* env_user = std::getenv("FXCM_USER");
    const char* env_pass = std::getenv("FXCM_PASS");
    const char* env_conn = std::getenv("FXCM_CONNECTION");
    
    std::string fxcm_user = env_user ? env_user : "";
    std::string fxcm_pass = env_pass ? env_pass : "";
    std::string fxcm_connection = env_conn ? env_conn : "Demo";
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--port" && i + 1 < argc) {
            port = std::atoi(argv[++i]);
        } else if (arg == "--fxcm-user" && i + 1 < argc) {
            fxcm_user = argv[++i];
        } else if (arg == "--fxcm-pass" && i + 1 < argc) {
            fxcm_pass = argv[++i];
        } else if (arg == "--fxcm-live") {
            fxcm_connection = "Real";
        } else if (arg == "--help") {
            std::cout << "Usage: central_exchange [options]\n"
                      << "  --port <port>        HTTP server port (default: 8080)\n"
                      << "  --fxcm-user <user>   FXCM username (or FXCM_USER env)\n"
                      << "  --fxcm-pass <pass>   FXCM password (or FXCM_PASS env)\n"
                      << "  --fxcm-live          Use live FXCM connection (default: Demo)\n"
                      << "  --help               Show this help\n";
            return 0;
        }
    }
    
    // Initialize product catalog
    std::cout << "[INIT] Loading product catalog...\n";
    ProductCatalog::instance();

    // Initialize SQLite database
    std::cout << "[INIT] Initializing SQLite database...\n";
    if (!Database::instance().init()) {
        std::cerr << "[ERROR] Failed to initialize database!\n";
    } else {
        std::cout << "       Database ready at /tmp/exchange.db\n";
    }
    
    // Initialize accounting engine (dual-speed ledger)
    std::cout << "[INIT] Starting accounting engine...\n";
    AccountingEngine::instance().start("./data");
    std::cout << "       Event journal: ./data/events.dat\n";
    std::cout << "       Ledger journal: ./data/ledger.journal\n";
    
    print_products();
    
    // Initialize matching engine
    std::cout << "[INIT] Starting matching engine...\n";
    MatchingEngine::instance();
    std::cout << "       " << MatchingEngine::instance().book_count() << " order books created\n";
    
    // Connect to FXCM (or simulate)
    std::cout << "[INIT] Connecting to FXCM (" << fxcm_connection << ")...\n";
    auto& fxcm = FxcmFeed::instance();
    
    bool fxcm_connected = false;
    if (!fxcm_user.empty()) {
        fxcm_connected = fxcm.connect(fxcm_user, fxcm_pass, fxcm_connection);
        if (!fxcm_connected) {
            std::cout << "       FXCM login failed - falling back to simulation\n";
        }
    } else {
        std::cout << "       No FXCM credentials\n";
    }
    
    if (!fxcm_connected) {
        std::cout << "       Using simulated prices\n";
        fxcm.start_simulation();
    }
    
    // Subscribe to FXCM symbols for hedgeable products
    for (const auto* product : ProductCatalog::instance().get_hedgeable()) {
        if (!product->fxcm_symbol.empty()) {
            fxcm.subscribe(product->fxcm_symbol);
        }
    }
    
    // Wire FXCM price updates to CandleStore for real-time chart data
    fxcm.set_price_callback([](const PriceUpdate& update) {
        // Callback only fires when bid/ask actually changed (filtered in fxcm_feed)
        ProductCatalog::instance().for_each([&](const Product& p) {
            if (p.fxcm_symbol == update.symbol) {
                double mid = (update.bid + update.ask) / 2.0;
                double mnt_price = p.fxcm_to_mnt(mid);
                if (mnt_price > 0 && mnt_price < 1e12) {
                    for (int tf : CandleStore::TIMEFRAMES) {
                        CandleStore::instance().update(p.symbol, mnt_price, tf);
                    }
                }
            }
        });
        // Push-on-change: notify SSE clients immediately
        if (g_server) g_server->notify_price_change();
    });

#ifdef FXCM_ENABLED
    // Initialize FXCM Price History Manager for real historical data
    if (fxcm.is_connected()) {
        std::cout << "[INIT] Initializing FXCM Price History Manager...\n";
        if (FxcmHistoryManager::instance().initialize(fxcm.get_session())) {
            std::cout << "       Price History API initialized (may be lazily ready)\n";
            // Background thread to clear seeded flags once communicator is ready
            // This ensures real data replaces synthetic on next candle request
            std::thread([]() {
                // Wait for communicator to become ready, then clear seeded flags
                for (int attempt = 0; attempt < 10; attempt++) {
                    std::this_thread::sleep_for(std::chrono::seconds(30));
                    // Try to fetch a test instrument to see if it works
                    auto test = FxcmHistoryManager::instance().fetchHistory("EUR/USD", 900, 10);
                    if (!test.empty()) {
                        std::cout << "[FXCM History] History API confirmed working - clearing seeded flags\n";
                        CandleStore::instance().clear_seeded();
                        break;
                    }
                    std::cout << "[FXCM History] Attempt " << (attempt+1) << "/10 - not ready yet\n";
                }
            }).detach();
        } else {
            std::cout << "       WARNING: Price History API failed to create communicator\n";
        }
    }
#endif

    // Initialize hedging engine
    std::cout << "[INIT] Initializing hedging engine...\n";
    HedgingEngine::instance().set_hedge_threshold(0.05);  // 5%
    HedgingEngine::instance().set_max_position(100000);   // $100k USD max
    
    // Start Bank of Mongolia rate feed (hourly updates)
    std::cout << "[INIT] Starting Bank of Mongolia USD/MNT rate feed...\n";
    BomFeed::instance().start();
    std::cout << "       Current USD/MNT rate: " << USD_MNT_RATE << " MNT\n";
    
    // Initialize USD-MNT core market controller
    std::cout << "[INIT] Initializing USD-MNT core market controller...\n";
    UsdMntMarket::instance().initialize();
    auto [lower, upper] = UsdMntMarket::instance().get_price_limits();
    std::cout << "       USD-MNT price band: " << lower << " - " << upper << " MNT\n";
    
    // Setup signal handlers
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    // Load candle history from SQLite (restores charts across restarts)
    std::cout << "[INIT] Loading candle history from SQLite...\n";
    CandleStore::instance().load_from_db();
    
    // Start HTTP server
    std::cout << "[START] Starting HTTP server on port " << port << "...\n";
    HttpServer server(port);
    g_server = &server;
    
    server.start();
    
    std::cout << "\n";
    std::cout << "═══════════════════════════════════════════════════\n";
    std::cout << "  🚀 Central Exchange is LIVE!\n";
    std::cout << "  \n";
    std::cout << "  Web UI:    http://localhost:" << port << "\n";
    std::cout << "  API:       http://localhost:" << port << "/api\n";
    std::cout << "  \n";
    std::cout << "  Press Ctrl+C to shutdown\n";
    std::cout << "═══════════════════════════════════════════════════\n\n";
    
    // Main loop - check hedging periodically
    int loop_count = 0;
    while (server.is_running()) {
        // Run hedge check every 5 seconds
        HedgingEngine::instance().check_and_hedge();
        
        // Cleanup rate limiter every ~5 minutes (60 iterations * 5s)
        if (++loop_count % 60 == 0) {
            cre::RateLimiter::instance().cleanup(3600);
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
    
    // Cleanup - Graceful shutdown sequence
    std::cout << "[SHUTDOWN] Persisting open orders and positions...\n";
    
    // Sync event journal to disk
    std::cout << "[SHUTDOWN] Flushing event journal...\n";
    // AccountingEngine flush happens in destructor
    
    std::cout << "[SHUTDOWN] Disconnecting from FXCM...\n";
    fxcm.disconnect();
    
    std::cout << "[SHUTDOWN] Central Exchange stopped cleanly.\n";
    return 0;
}
