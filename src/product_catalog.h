#pragma once

/**
 * Central Exchange - Mongolia's First Digital Exchange
 * =====================================================
 * 
 * Product Catalog: FXCM-backed products + Mongolia-unique perpetuals
 * All products quoted in MNT (Mongolian Tugrik)
 * 
 * Categories:
 *   - FXCM_COMMODITY: Gold, Silver, Oil, Copper, Natural Gas
 *   - FXCM_FX_MAJOR: EUR/USD, GBP/USD, USD/JPY (converted to MNT)
 *   - FXCM_FX_LOCAL: USD/MNT, EUR/MNT, CNY/MNT, RUB/MNT
 *   - FXCM_INDEX: S&P 500, NASDAQ, Hang Seng, Nikkei
 *   - FXCM_CRYPTO: BTC, ETH
 *   - MN_PERPETUAL: Mongolia-unique perpetuals (no FXCM backing)
 */

#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <functional>
#include "rate_provider.h"

namespace central_exchange {

// Dynamic USD/MNT rate via RateProvider (live FXCM → env var → hardcoded fallback)
inline double get_usd_mnt_rate() {
    return RateProvider::instance().get_usd_mnt();
}

// Legacy alias — existing code that reads USD_MNT_RATE will get the live rate
// NOTE: This is a function-like macro, not a variable. Do not assign to it.
#define USD_MNT_RATE (central_exchange::get_usd_mnt_rate())

enum class ProductCategory {
    FXCM_COMMODITY,
    FXCM_FX_MAJOR,
    FXCM_FX_LOCAL,
    FXCM_INDEX,
    FXCM_CRYPTO,
    MN_PERPETUAL,  // Mongolia-unique, no FXCM hedge
    FX_PERP        // Currency perpetuals (USD/MNT etc)
};

enum class HedgeMode {
    NONE,           // No hedging (MN_PERPETUAL products)
    FULL,           // Hedge 100% to FXCM
    DELTA_NEUTRAL   // Hedge net exposure only
};

struct Product {
    std::string symbol;           // e.g., "XAU-MNT-PERP"
    std::string name;             // e.g., "Gold Perpetual (MNT)"
    std::string description;
    ProductCategory category;
    
    // FXCM Integration
    std::string fxcm_symbol;      // e.g., "XAU/USD" - empty for MN_PERPETUAL
    double usd_multiplier;        // For commodities: contract size in USD. For FX: always 1.0
    HedgeMode hedge_mode;
    bool is_inverted;             // True for USD/XXX pairs (USD/CNH, USD/RUB) where we need to divide
    
    // Trading Parameters
    double contract_size;         // Units per contract
    double tick_size;             // Minimum price increment (MNT)
    double min_order_size;        // Minimum order (contracts)
    double max_order_size;        // Maximum order (contracts)
    double margin_rate;           // Initial margin % (e.g., 0.10 = 10%)
    double maker_fee;             // Maker fee (e.g., 0.0002 = 0.02%)
    double taker_fee;             // Taker fee (e.g., 0.0005 = 0.05%)
    double spread_markup;         // Spread markup per side (e.g., 0.001 = 0.1%). 0 = fee-based model
    double min_notional_mnt{10000.0}; // Minimum notional per order in MNT
    double min_fee_mnt{10.0};     // Floor fee per trade in MNT (prevents sub-1 MNT fees)
    
    // Mark Price
    double mark_price_mnt;        // Composite mark price for liquidation/PnL (70% FXCM + 20% last trade + 10% book mid)
    double last_price_mnt{0.0};   // Last traded price (for display and execution)
    double funding_rate;          // 8-hour funding rate
    
    bool is_active;
    
    // Helpers - Calculate MNT price from FXCM price
    double fxcm_to_mnt(double fxcm_price) const {
        if (is_inverted) {
            // USD/XXX pair: XXX-MNT = USD/MNT / USD/XXX
            return USD_MNT_RATE / fxcm_price;
        } else {
            // XXX/USD pair: XXX-MNT = XXX/USD × USD/MNT × multiplier
            return fxcm_price * usd_multiplier * USD_MNT_RATE;
        }
    }
    
    double to_mnt(double usd_price) const {
        return usd_price * usd_multiplier * USD_MNT_RATE;
    }
    
    double to_usd(double mnt_price) const {
        return mnt_price / (usd_multiplier * USD_MNT_RATE);
    }
    
    bool requires_hedge() const {
        return hedge_mode != HedgeMode::NONE && !fxcm_symbol.empty();
    }
    
    bool is_spot() const {
        return symbol.find("-SPOT") != std::string::npos;
    }
};

class ProductCatalog {
public:
    static ProductCatalog& instance() {
        static ProductCatalog catalog;
        return catalog;
    }
    
    void initialize();
    
    const Product* get(const std::string& symbol) const;
    std::vector<const Product*> get_by_category(ProductCategory cat) const;
    std::vector<const Product*> get_all_active() const;
    std::vector<const Product*> get_hedgeable() const;
    
    void update_mark_price(const std::string& symbol, double mnt_price);
    void update_last_price(const std::string& symbol, double mnt_price);
    void update_composite_mark(const std::string& symbol, double fxcm_price, double book_mid);
    void update_funding_rate(const std::string& symbol, double rate);
    void update_usd_mnt_rate(double rate);
    
    // Iterate all
    void for_each(std::function<void(const Product&)> callback) const;

private:
    ProductCatalog() { initialize(); }
    std::unordered_map<std::string, Product> products_;
    
    void add_product(Product p) {
        products_[p.symbol] = std::move(p);
    }
};

// ========== PRODUCT DEFINITIONS ==========

inline void ProductCatalog::initialize() {
    products_.clear();
    
    // === COMMODITIES (FXCM-backed) ===
    add_product({
        .symbol = "XAU-MNT-PERP",
        .name = "Gold Perpetual",
        .description = "Gold vs MNT - backed by FXCM XAU/USD",
        .category = ProductCategory::FXCM_COMMODITY,
        .fxcm_symbol = "XAU/USD",
        .usd_multiplier = 1.0,
        .hedge_mode = HedgeMode::DELTA_NEUTRAL,
        .is_inverted = false,
        .contract_size = 0.001,     // 0.001 oz per contract (~$2.6 ≈ 9,200 MNT notional)
        .tick_size = 10.0,          // 10 MNT tick
        .min_order_size = 1.0,      // 1 micro contract min
        .max_order_size = 10000.0,  // 10000 micro contracts max
        .margin_rate = 0.05,        // 5% margin (20x)
        .maker_fee = 0.0002,
        .taker_fee = 0.0005,
        .spread_markup = 0.001,
        .mark_price_mnt = 2650.0 * USD_MNT_RATE,  // ~$2650/oz gold
        .funding_rate = 0.0001,
        .is_active = true
    });
    
    add_product({
        .symbol = "XAG-MNT-PERP",
        .name = "Silver Perpetual",
        .description = "Silver vs MNT - backed by FXCM XAG/USD",
        .category = ProductCategory::FXCM_COMMODITY,
        .fxcm_symbol = "XAG/USD",
        .usd_multiplier = 1.0,
        .hedge_mode = HedgeMode::DELTA_NEUTRAL,
        .is_inverted = false,
        .contract_size = 0.1,       // 0.1 oz per contract (~$3.1 ≈ 11,000 MNT notional)
        .tick_size = 1.0,
        .min_order_size = 1.0,     // 1 micro contract min
        .max_order_size = 10000.0,
        .margin_rate = 0.05,
        .maker_fee = 0.0002,
        .taker_fee = 0.0005,
        .spread_markup = 0.001,
        .mark_price_mnt = 31.0 * USD_MNT_RATE,  // ~$31/oz silver
        .funding_rate = 0.0001,
        .is_active = true
    });
    
    add_product({
        .symbol = "OIL-MNT-PERP",
        .name = "Crude Oil Perpetual",
        .description = "WTI Crude Oil vs MNT",
        .category = ProductCategory::FXCM_COMMODITY,
        .fxcm_symbol = "USOil",
        .usd_multiplier = 1.0,
        .hedge_mode = HedgeMode::DELTA_NEUTRAL,
        .is_inverted = false,
        .contract_size = 0.1,      // 0.1 barrel (~$7.5 ≈ 26,700 MNT notional)
        .tick_size = 5.0,
        .min_order_size = 1.0,     // 1 micro contract min
        .max_order_size = 100000.0,
        .margin_rate = 0.10,
        .maker_fee = 0.0002,
        .taker_fee = 0.0005,
        .spread_markup = 0.001,
        .mark_price_mnt = 75.0 * USD_MNT_RATE,  // ~$75/bbl WTI
        .funding_rate = 0.0001,
        .is_active = true
    });
    
    add_product({
        .symbol = "COPPER-MNT-PERP",
        .name = "Copper Perpetual",
        .description = "Copper vs MNT",
        .category = ProductCategory::FXCM_COMMODITY,
        .fxcm_symbol = "Copper",
        .usd_multiplier = 1.0,
        .hedge_mode = HedgeMode::DELTA_NEUTRAL,
        .is_inverted = false,
        .contract_size = 1.0,      // 1 lb (~$4.5 ≈ 16,000 MNT notional)
        .tick_size = 1.0,
        .min_order_size = 1.0,    // 1 lb min
        .max_order_size = 100000.0,
        .margin_rate = 0.10,
        .maker_fee = 0.0002,
        .taker_fee = 0.0005,
        .spread_markup = 0.001,
        .mark_price_mnt = 4.5 * USD_MNT_RATE,  // ~$4.5/lb copper
        .funding_rate = 0.0001,
        .is_active = true
    });
    
    add_product({
        .symbol = "NATGAS-MNT-PERP",
        .name = "Natural Gas Perpetual",
        .description = "Natural Gas vs MNT",
        .category = ProductCategory::FXCM_COMMODITY,
        .fxcm_symbol = "NatGas",
        .usd_multiplier = 1.0,
        .hedge_mode = HedgeMode::DELTA_NEUTRAL,
        .is_inverted = false,
        .contract_size = 1.0,      // 1 MMBtu (~$3 ≈ 10,700 MNT notional)
        .tick_size = 0.1,
        .min_order_size = 1.0,     // 1 MMBtu min
        .max_order_size = 100000.0,
        .margin_rate = 0.15,
        .maker_fee = 0.0002,
        .taker_fee = 0.0005,
        .spread_markup = 0.001,
        .mark_price_mnt = 3.0 * USD_MNT_RATE,  // ~$3/MMBtu natgas
        .funding_rate = 0.0001,
        .is_active = true
    });
    
    // === FX PAIRS (FXCM-backed, converted to MNT) ===
    add_product({
        .symbol = "USD-MNT-PERP",
        .name = "USD/MNT Perpetual",
        .description = "US Dollar vs Mongolian Tugrik - Currency hedge instrument",
        .category = ProductCategory::FX_PERP,  // FX perpetual category
        .fxcm_symbol = "",  // Internal pricing - WE are the source
        .usd_multiplier = 1.0,
        .hedge_mode = HedgeMode::NONE,
        .is_inverted = false,  // Can't hedge - we ARE the USD/MNT source
        .contract_size = 1.0,    // 1 USD notional per contract (≈3,564 MNT — retail accessible)
        .tick_size = 0.1,         // 0.1 MNT minimum increment
        .min_order_size = 1.0,    // 1 contract min (≈3,564 MNT)
        .max_order_size = 100000.0,
        .margin_rate = 0.05,      // 5% margin = 20x leverage (Conductor spec)
        .maker_fee = 0.0001,
        .taker_fee = 0.0003,
        .spread_markup = 0.0005,
        .mark_price_mnt = USD_MNT_RATE,  // Current Bank of Mongolia rate
        .funding_rate = 0.0001,   // Daily funding based on interest rate diff
        .is_active = true
    });
    
    add_product({
        .symbol = "EUR-MNT-PERP",
        .name = "EUR/MNT Perpetual",
        .description = "Euro vs Mongolian Tugrik",
        .category = ProductCategory::FXCM_FX_LOCAL,
        .fxcm_symbol = "EUR/USD",
        .usd_multiplier = 1.0,    // FX pairs: multiplier is 1.0, FXCM price IS the rate
        .hedge_mode = HedgeMode::DELTA_NEUTRAL,
        .is_inverted = false,
        .contract_size = 1.0,    // 1 EUR notional per contract
        .tick_size = 0.1,
        .min_order_size = 1.0,
        .max_order_size = 100000.0,
        .margin_rate = 0.02,
        .maker_fee = 0.0001,
        .taker_fee = 0.0003,
        .spread_markup = 0.0005,
        .mark_price_mnt = 1.08 * USD_MNT_RATE,  // EUR/USD × USD/MNT
        .funding_rate = 0.0001,
        .is_active = true
    });
    
    add_product({
        .symbol = "CNY-MNT-PERP",
        .name = "CNY/MNT Perpetual",
        .description = "Chinese Yuan vs Mongolian Tugrik",
        .category = ProductCategory::FXCM_FX_LOCAL,
        .fxcm_symbol = "USD/CNH",
        .usd_multiplier = 1.0,    // Not used for inverted pairs
        .hedge_mode = HedgeMode::DELTA_NEUTRAL,
        .is_inverted = true,      // USD/CNH is inverted: CNY-MNT = USD-MNT / USD-CNH
        .contract_size = 100.0,  // ¥100 notional (~$14 ≈ 50,000 MNT)
        .tick_size = 0.01,
        .min_order_size = 1.0,
        .max_order_size = 100000.0,
        .margin_rate = 0.03,
        .maker_fee = 0.0001,
        .taker_fee = 0.0003,
        .spread_markup = 0.0005,
        .mark_price_mnt = USD_MNT_RATE / 7.25,  // USD/MNT ÷ USD/CNH
        .funding_rate = 0.0001,
        .is_active = true
    });
    
    add_product({
        .symbol = "RUB-MNT-PERP",
        .name = "RUB/MNT Perpetual",
        .description = "Russian Ruble vs Mongolian Tugrik",
        .category = ProductCategory::FXCM_FX_LOCAL,
        .fxcm_symbol = "USD/RUB",
        .usd_multiplier = 1.0,    // Not used for inverted pairs
        .hedge_mode = HedgeMode::DELTA_NEUTRAL,
        .is_inverted = true,      // USD/RUB is inverted: RUB-MNT = USD-MNT / USD-RUB
        .contract_size = 1000.0,  // ₽1000 notional (~$11 ≈ 39,000 MNT)
        .tick_size = 0.001,
        .min_order_size = 1.0,
        .max_order_size = 100000.0,
        .margin_rate = 0.05,
        .maker_fee = 0.0001,
        .taker_fee = 0.0003,
        .spread_markup = 0.0005,
        .mark_price_mnt = USD_MNT_RATE / 90.0,  // USD/MNT ÷ USD/RUB
        .funding_rate = 0.0001,
        .is_active = true
    });
    
    add_product({
        .symbol = "JPY-MNT-SPOT",
        .name = "JPY/MNT Spot",
        .description = "Japanese Yen vs Mongolian Tugrik",
        .category = ProductCategory::FXCM_FX_LOCAL,
        .fxcm_symbol = "USD/JPY",
        .usd_multiplier = 1.0,
        .hedge_mode = HedgeMode::DELTA_NEUTRAL,
        .is_inverted = true,      // USD/JPY is inverted: JPY-MNT = USD-MNT / USD-JPY
        .contract_size = 1000.0,  // ¥1000 notional (~$6.7 ≈ 24,000 MNT)
        .tick_size = 0.001,
        .min_order_size = 1.0,
        .max_order_size = 1000000.0,
        .margin_rate = 1.0,       // 100% margin = no leverage (spot cash)
        .maker_fee = 0.0,
        .taker_fee = 0.0,
        .spread_markup = 0.0015,  // 0.15% spread (revenue from spread, not fees)
        .mark_price_mnt = USD_MNT_RATE / 150.0,  // USD/MNT ÷ USD/JPY (~150)
        .funding_rate = 0.0,      // No funding on spot
        .is_active = true
    });
    
    add_product({
        .symbol = "KRW-MNT-SPOT",
        .name = "KRW/MNT Spot",
        .description = "Korean Won vs Mongolian Tugrik",
        .category = ProductCategory::FXCM_FX_LOCAL,
        .fxcm_symbol = "USD/KRW",
        .usd_multiplier = 1.0,
        .hedge_mode = HedgeMode::DELTA_NEUTRAL,
        .is_inverted = true,      // USD/KRW is inverted: KRW-MNT = USD-MNT / USD-KRW
        .contract_size = 10000.0,  // ₩10000 notional (~$7 ≈ 25,000 MNT)
        .tick_size = 0.0001,
        .min_order_size = 1.0,
        .max_order_size = 1000000.0,
        .margin_rate = 1.0,       // 100% margin = no leverage (spot cash)
        .maker_fee = 0.0,
        .taker_fee = 0.0,
        .spread_markup = 0.0015,  // 0.15% spread (revenue from spread, not fees)
        .mark_price_mnt = USD_MNT_RATE / 1400.0,  // USD/MNT ÷ USD/KRW (~1400)
        .funding_rate = 0.0,      // No funding on spot
        .is_active = true
    });
    
    // === INDICES (FXCM-backed) ===
    add_product({
        .symbol = "SPX-MNT-PERP",
        .name = "S&P 500 Perpetual",
        .description = "S&P 500 Index vs MNT",
        .category = ProductCategory::FXCM_INDEX,
        .fxcm_symbol = "SPX500",
        .usd_multiplier = 1.0,
        .hedge_mode = HedgeMode::DELTA_NEUTRAL,
        .is_inverted = false,
        .contract_size = 0.001,   // 0.001 per index point ($6 ≈ 21,000 MNT notional for SPX ~6000)
        .tick_size = 10.0,
        .min_order_size = 1.0,
        .max_order_size = 10000.0,
        .margin_rate = 0.05,
        .maker_fee = 0.0002,
        .taker_fee = 0.0005,
        .spread_markup = 0.001,
        .mark_price_mnt = 6000.0 * USD_MNT_RATE,  // ~SPX 6000
        .funding_rate = 0.0001,
        .is_active = true
    });
    
    add_product({
        .symbol = "NDX-MNT-PERP",
        .name = "NASDAQ 100 Perpetual",
        .description = "NASDAQ 100 Index vs MNT",
        .category = ProductCategory::FXCM_INDEX,
        .fxcm_symbol = "NAS100",
        .usd_multiplier = 1.0,
        .hedge_mode = HedgeMode::DELTA_NEUTRAL,
        .is_inverted = false,
        .contract_size = 0.001,   // 0.001 per index point ($21 ≈ 75,000 MNT notional for NDX ~21000)
        .tick_size = 10.0,
        .min_order_size = 1.0,
        .max_order_size = 10000.0,
        .margin_rate = 0.05,
        .maker_fee = 0.0002,
        .taker_fee = 0.0005,
        .spread_markup = 0.001,
        .mark_price_mnt = 21000.0 * USD_MNT_RATE,  // ~NDX 21000
        .funding_rate = 0.0001,
        .is_active = true
    });
    
    add_product({
        .symbol = "HSI-MNT-PERP",
        .name = "Hang Seng Perpetual",
        .description = "Hong Kong Hang Seng Index vs MNT",
        .category = ProductCategory::FXCM_INDEX,
        .fxcm_symbol = "HKG33",
        .usd_multiplier = 1.0,
        .hedge_mode = HedgeMode::DELTA_NEUTRAL,
        .is_inverted = false,
        .contract_size = 0.001,   // 0.001 per index point ($20 ≈ 71,000 MNT notional for HSI ~20000)
        .tick_size = 10.0,
        .min_order_size = 1.0,
        .max_order_size = 10000.0,
        .margin_rate = 0.05,
        .maker_fee = 0.0002,
        .taker_fee = 0.0005,
        .spread_markup = 0.001,
        .mark_price_mnt = 20000.0 * USD_MNT_RATE,  // ~HSI 20000
        .funding_rate = 0.0001,
        .is_active = true
    });
    
    add_product({
        .symbol = "NKY-MNT-PERP",
        .name = "Nikkei 225 Perpetual",
        .description = "Japan Nikkei 225 Index vs MNT",
        .category = ProductCategory::FXCM_INDEX,
        .fxcm_symbol = "JPN225",
        .usd_multiplier = 1.0,
        .hedge_mode = HedgeMode::DELTA_NEUTRAL,
        .is_inverted = false,
        .contract_size = 0.001,   // 0.001 per index point ($38 ≈ 135,000 MNT notional for NKY ~38000)
        .tick_size = 10.0,
        .min_order_size = 1.0,
        .max_order_size = 10000.0,
        .margin_rate = 0.05,
        .maker_fee = 0.0002,
        .taker_fee = 0.0005,
        .spread_markup = 0.001,
        .mark_price_mnt = 38000.0 * USD_MNT_RATE,  // ~NKY 38000
        .funding_rate = 0.0001,
        .is_active = true
    });
    
    // === CRYPTO (FXCM-backed) ===
    add_product({
        .symbol = "BTC-MNT-PERP",
        .name = "Bitcoin Perpetual",
        .description = "Bitcoin vs MNT",
        .category = ProductCategory::FXCM_CRYPTO,
        .fxcm_symbol = "BTC/USD",
        .usd_multiplier = 1.0,
        .hedge_mode = HedgeMode::DELTA_NEUTRAL,
        .is_inverted = false,
        .contract_size = 0.0001,  // 0.0001 BTC per contract (~$10.3 ≈ 37,000 MNT notional)
        .tick_size = 100.0,
        .min_order_size = 1.0,    // 1 micro contract min
        .max_order_size = 100000.0,
        .margin_rate = 0.10,
        .maker_fee = 0.0002,
        .taker_fee = 0.0005,
        .spread_markup = 0.001,
        .mark_price_mnt = 103000.0 * USD_MNT_RATE,  // ~$103k BTC
        .funding_rate = 0.0001,
        .is_active = true
    });
    
    add_product({
        .symbol = "ETH-MNT-PERP",
        .name = "Ethereum Perpetual",
        .description = "Ethereum vs MNT",
        .category = ProductCategory::FXCM_CRYPTO,
        .fxcm_symbol = "ETH/USD",
        .usd_multiplier = 1.0,
        .hedge_mode = HedgeMode::DELTA_NEUTRAL,
        .is_inverted = false,
        .contract_size = 0.001,   // 0.001 ETH per contract (~$3.3 ≈ 12,000 MNT notional)
        .tick_size = 10.0,
        .min_order_size = 1.0,    // 1 micro contract min
        .max_order_size = 100000.0,
        .margin_rate = 0.10,
        .maker_fee = 0.0002,
        .taker_fee = 0.0005,
        .spread_markup = 0.001,
        .mark_price_mnt = 3300.0 * USD_MNT_RATE,  // ~$3300 ETH
        .funding_rate = 0.0001,
        .is_active = true
    });
    
    // =========================================================
    // === MONGOLIA-UNIQUE PERPETUALS (No FXCM hedge needed) ===
    // =========================================================
    
    add_product({
        .symbol = "MN-MEAT-PERP",
        .name = "Mongolia Meat Futures",
        .description = "Mongolian Livestock/Meat Price Index",
        .category = ProductCategory::MN_PERPETUAL,
        .fxcm_symbol = "",  // No FXCM backing
        .usd_multiplier = 0.0,
        .hedge_mode = HedgeMode::NONE,
        .is_inverted = false,
        .contract_size = 0.001,   // 0.001 index unit (≈1,000 MNT notional)
        .tick_size = 1.0,
        .min_order_size = 10.0,   // min ~10,000 MNT notional
        .max_order_size = 1000000.0,
        .margin_rate = 0.20,      // 20% margin (5x) - more volatile
        .maker_fee = 0.0005,
        .taker_fee = 0.001,
        .spread_markup = 0.002,
        .mark_price_mnt = 1'000'000.0,  // Base index = 1M MNT
        .funding_rate = 0.0003,
        .is_active = true
    });
    
    add_product({
        .symbol = "MN-REALESTATE-PERP",
        .name = "Mongolia Real Estate Index",
        .description = "Ulaanbaatar Real Estate Price Index",
        .category = ProductCategory::MN_PERPETUAL,
        .fxcm_symbol = "",
        .usd_multiplier = 0.0,
        .hedge_mode = HedgeMode::NONE,
        .is_inverted = false,
        .contract_size = 0.001,   // 0.001 sqm index unit (≈5,000 MNT notional)
        .tick_size = 1.0,
        .min_order_size = 10.0,   // min ~50,000 MNT notional
        .max_order_size = 100000.0,
        .margin_rate = 0.25,      // 25% margin (4x)
        .maker_fee = 0.0005,
        .taker_fee = 0.001,
        .spread_markup = 0.002,
        .mark_price_mnt = 5'000'000.0,  // 5M MNT per sqm index
        .funding_rate = 0.0003,
        .is_active = true
    });
    
    add_product({
        .symbol = "MN-CASHMERE-PERP",
        .name = "Mongolia Cashmere Futures",
        .description = "Mongolian Cashmere Wool Price Index",
        .category = ProductCategory::MN_PERPETUAL,
        .fxcm_symbol = "",
        .usd_multiplier = 0.0,
        .hedge_mode = HedgeMode::NONE,
        .is_inverted = false,
        .contract_size = 0.1,     // 0.1 kg (~$4.3 ≈ 15,300 MNT notional)
        .tick_size = 10.0,
        .min_order_size = 1.0,    // min ~15,300 MNT notional
        .max_order_size = 100000.0,
        .margin_rate = 0.15,
        .maker_fee = 0.0005,
        .taker_fee = 0.001,
        .spread_markup = 0.002,
        .mark_price_mnt = 43.0 * USD_MNT_RATE,  // ~$43/kg cashmere
        .funding_rate = 0.0003,
        .is_active = true
    });
    
    add_product({
        .symbol = "MN-COAL-PERP",
        .name = "Mongolia Coal Futures",
        .description = "Mongolian Coal Price Index (Tavan Tolgoi benchmark)",
        .category = ProductCategory::MN_PERPETUAL,
        .fxcm_symbol = "",
        .usd_multiplier = 0.0,
        .hedge_mode = HedgeMode::NONE,
        .is_inverted = false,
        .contract_size = 0.01,    // 0.01 tonne (10 kg, ~$2 ≈ 7,100 MNT notional)
        .tick_size = 1.0,
        .min_order_size = 1.0,    // min ~7,100 MNT notional
        .max_order_size = 1000000.0,
        .margin_rate = 0.15,
        .maker_fee = 0.0005,
        .taker_fee = 0.001,
        .spread_markup = 0.002,
        .mark_price_mnt = 200.0 * USD_MNT_RATE,  // ~$200/tonne coal
        .funding_rate = 0.0002,
        .is_active = true
    });
    
    add_product({
        .symbol = "MN-COPPER-PERP",
        .name = "Mongolia Copper Concentrate",
        .description = "Oyu Tolgoi Copper Concentrate Price",
        .category = ProductCategory::MN_PERPETUAL,
        .fxcm_symbol = "",  // Could optionally hedge with FXCM Copper
        .usd_multiplier = 0.0,
        .hedge_mode = HedgeMode::NONE,
        .is_inverted = false,
        .contract_size = 0.001,   // 0.001 tonne (1 kg, ~$8 ≈ 28,500 MNT notional)
        .tick_size = 10.0,
        .min_order_size = 1.0,    // min ~28,500 MNT notional
        .max_order_size = 100000.0,
        .margin_rate = 0.15,
        .maker_fee = 0.0005,
        .taker_fee = 0.001,
        .spread_markup = 0.002,
        .mark_price_mnt = 8000.0 * USD_MNT_RATE,  // ~$8000/tonne copper conc
        .funding_rate = 0.0002,
        .is_active = true
    });
}

inline const Product* ProductCatalog::get(const std::string& symbol) const {
    auto it = products_.find(symbol);
    return it != products_.end() ? &it->second : nullptr;
}

inline std::vector<const Product*> ProductCatalog::get_by_category(ProductCategory cat) const {
    std::vector<const Product*> result;
    for (const auto& [sym, prod] : products_) {
        if (prod.category == cat && prod.is_active) {
            result.push_back(&prod);
        }
    }
    return result;
}

inline std::vector<const Product*> ProductCatalog::get_all_active() const {
    std::vector<const Product*> result;
    for (const auto& [sym, prod] : products_) {
        if (prod.is_active) {
            result.push_back(&prod);
        }
    }
    return result;
}

inline std::vector<const Product*> ProductCatalog::get_hedgeable() const {
    std::vector<const Product*> result;
    for (const auto& [sym, prod] : products_) {
        if (prod.is_active && prod.requires_hedge()) {
            result.push_back(&prod);
        }
    }
    return result;
}

inline void ProductCatalog::update_mark_price(const std::string& symbol, double mnt_price) {
    auto it = products_.find(symbol);
    if (it != products_.end()) {
        it->second.mark_price_mnt = mnt_price;
    }
}

inline void ProductCatalog::update_last_price(const std::string& symbol, double mnt_price) {
    auto it = products_.find(symbol);
    if (it != products_.end()) {
        it->second.last_price_mnt = mnt_price;
    }
}

// Composite mark price: 70% FXCM feed + 20% last trade + 10% order book mid
inline void ProductCatalog::update_composite_mark(const std::string& symbol, double fxcm_price, double book_mid) {
    auto it = products_.find(symbol);
    if (it == products_.end()) return;
    
    auto& prod = it->second;
    double last = prod.last_price_mnt > 0 ? prod.last_price_mnt : fxcm_price;
    double mid = book_mid > 0 ? book_mid : fxcm_price;
    
    // Weighted composite: prevents single-source manipulation
    prod.mark_price_mnt = fxcm_price * 0.70 + last * 0.20 + mid * 0.10;
}

inline void ProductCatalog::update_funding_rate(const std::string& symbol, double rate) {
    auto it = products_.find(symbol);
    if (it != products_.end()) {
        it->second.funding_rate = rate;
    }
}

inline void ProductCatalog::update_usd_mnt_rate(double rate) {
    RateProvider::instance().update_rate("USD/MNT", rate);
}

inline void ProductCatalog::for_each(std::function<void(const Product&)> callback) const {
    for (const auto& [sym, prod] : products_) {
        callback(prod);
    }
}

} // namespace central_exchange
