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

namespace central_exchange {

// USD/MNT exchange rate (updated from FXCM)
inline double USD_MNT_RATE = 3450.0;

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
    double usd_multiplier;        // Convert FXCM price to USD notional
    HedgeMode hedge_mode;
    
    // Trading Parameters
    double contract_size;         // Units per contract
    double tick_size;             // Minimum price increment (MNT)
    double min_order_size;        // Minimum order (contracts)
    double max_order_size;        // Maximum order (contracts)
    double margin_rate;           // Initial margin % (e.g., 0.10 = 10%)
    double maker_fee;             // Maker fee (e.g., 0.0002 = 0.02%)
    double taker_fee;             // Taker fee (e.g., 0.0005 = 0.05%)
    
    // Mark Price
    double mark_price_mnt;        // Current mark price in MNT
    double funding_rate;          // 8-hour funding rate
    
    bool is_active;
    
    // Helpers
    double to_mnt(double usd_price) const {
        return usd_price * usd_multiplier * USD_MNT_RATE;
    }
    
    double to_usd(double mnt_price) const {
        return mnt_price / (usd_multiplier * USD_MNT_RATE);
    }
    
    bool requires_hedge() const {
        return hedge_mode != HedgeMode::NONE && !fxcm_symbol.empty();
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
        .contract_size = 1.0,       // 1 oz per contract
        .tick_size = 100.0,         // 100 MNT tick
        .min_order_size = 0.01,
        .max_order_size = 100.0,
        .margin_rate = 0.05,        // 5% margin (20x)
        .maker_fee = 0.0002,
        .taker_fee = 0.0005,
        .mark_price_mnt = 7'000'000.0,  // ~$2,029 * 3450
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
        .contract_size = 100.0,     // 100 oz per contract
        .tick_size = 10.0,
        .min_order_size = 0.1,
        .max_order_size = 1000.0,
        .margin_rate = 0.05,
        .maker_fee = 0.0002,
        .taker_fee = 0.0005,
        .mark_price_mnt = 105'000.0,  // ~$30 * 3450
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
        .contract_size = 100.0,     // 100 barrels
        .tick_size = 50.0,
        .min_order_size = 0.1,
        .max_order_size = 500.0,
        .margin_rate = 0.10,
        .maker_fee = 0.0002,
        .taker_fee = 0.0005,
        .mark_price_mnt = 260'000.0,  // ~$75 * 3450
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
        .contract_size = 1000.0,    // 1000 lbs
        .tick_size = 10.0,
        .min_order_size = 0.1,
        .max_order_size = 500.0,
        .margin_rate = 0.10,
        .maker_fee = 0.0002,
        .taker_fee = 0.0005,
        .mark_price_mnt = 15'500.0,  // ~$4.5 * 3450
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
        .contract_size = 10000.0,   // 10,000 MMBtu
        .tick_size = 1.0,
        .min_order_size = 0.1,
        .max_order_size = 500.0,
        .margin_rate = 0.15,
        .maker_fee = 0.0002,
        .taker_fee = 0.0005,
        .mark_price_mnt = 10'350.0,  // ~$3 * 3450
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
        .hedge_mode = HedgeMode::NONE,  // Can't hedge - we ARE the USD/MNT source
        .contract_size = 1000.0,  // $1000 USD notional per contract
        .tick_size = 1.0,         // 1 MNT minimum increment
        .min_order_size = 0.1,
        .max_order_size = 10000.0,
        .margin_rate = 0.05,      // 5% margin = 20x leverage (Conductor spec)
        .maker_fee = 0.0001,
        .taker_fee = 0.0003,
        .mark_price_mnt = 3450.0, // Current Bank of Mongolia rate
        .funding_rate = 0.0001,   // Daily funding based on interest rate diff
        .is_active = true
    });
    
    add_product({
        .symbol = "EUR-MNT-PERP",
        .name = "EUR/MNT Perpetual",
        .description = "Euro vs Mongolian Tugrik",
        .category = ProductCategory::FXCM_FX_LOCAL,
        .fxcm_symbol = "EUR/USD",
        .usd_multiplier = 1.08,   // EUR/USD rate
        .hedge_mode = HedgeMode::DELTA_NEUTRAL,
        .contract_size = 1000.0,  // €1000 notional
        .tick_size = 1.0,
        .min_order_size = 0.1,
        .max_order_size = 10000.0,
        .margin_rate = 0.02,
        .maker_fee = 0.0001,
        .taker_fee = 0.0003,
        .mark_price_mnt = 3726.0,  // 3450 * 1.08
        .funding_rate = 0.0001,
        .is_active = true
    });
    
    add_product({
        .symbol = "CNY-MNT-PERP",
        .name = "CNY/MNT Perpetual",
        .description = "Chinese Yuan vs Mongolian Tugrik",
        .category = ProductCategory::FXCM_FX_LOCAL,
        .fxcm_symbol = "USD/CNH",
        .usd_multiplier = 0.138,  // ~1/7.25
        .hedge_mode = HedgeMode::DELTA_NEUTRAL,
        .contract_size = 10000.0, // ¥10000 notional
        .tick_size = 0.1,
        .min_order_size = 1.0,
        .max_order_size = 100000.0,
        .margin_rate = 0.03,
        .maker_fee = 0.0001,
        .taker_fee = 0.0003,
        .mark_price_mnt = 476.0,  // 3450 / 7.25
        .funding_rate = 0.0001,
        .is_active = true
    });
    
    add_product({
        .symbol = "RUB-MNT-PERP",
        .name = "RUB/MNT Perpetual",
        .description = "Russian Ruble vs Mongolian Tugrik",
        .category = ProductCategory::FXCM_FX_LOCAL,
        .fxcm_symbol = "USD/RUB",
        .usd_multiplier = 0.011,  // ~1/90
        .hedge_mode = HedgeMode::DELTA_NEUTRAL,
        .contract_size = 100000.0, // ₽100000 notional
        .tick_size = 0.01,
        .min_order_size = 1.0,
        .max_order_size = 1000000.0,
        .margin_rate = 0.05,
        .maker_fee = 0.0001,
        .taker_fee = 0.0003,
        .mark_price_mnt = 38.0,   // 3450 / 90
        .funding_rate = 0.0001,
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
        .contract_size = 1.0,     // 1 index point = $1
        .tick_size = 100.0,
        .min_order_size = 0.1,
        .max_order_size = 100.0,
        .margin_rate = 0.05,
        .maker_fee = 0.0002,
        .taker_fee = 0.0005,
        .mark_price_mnt = 17'250'000.0,  // ~5000 * 3450
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
        .contract_size = 1.0,
        .tick_size = 100.0,
        .min_order_size = 0.1,
        .max_order_size = 100.0,
        .margin_rate = 0.05,
        .maker_fee = 0.0002,
        .taker_fee = 0.0005,
        .mark_price_mnt = 60'000'000.0,  // ~17400 * 3450
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
        .contract_size = 1.0,
        .tick_size = 50.0,
        .min_order_size = 0.1,
        .max_order_size = 100.0,
        .margin_rate = 0.05,
        .maker_fee = 0.0002,
        .taker_fee = 0.0005,
        .mark_price_mnt = 70'000'000.0,
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
        .contract_size = 0.001,   // 0.001 BTC per contract
        .tick_size = 1000.0,
        .min_order_size = 0.001,
        .max_order_size = 10.0,
        .margin_rate = 0.10,
        .maker_fee = 0.0002,
        .taker_fee = 0.0005,
        .mark_price_mnt = 345'000'000.0,  // ~$100k * 3450
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
        .contract_size = 0.01,    // 0.01 ETH per contract
        .tick_size = 100.0,
        .min_order_size = 0.01,
        .max_order_size = 100.0,
        .margin_rate = 0.10,
        .maker_fee = 0.0002,
        .taker_fee = 0.0005,
        .mark_price_mnt = 10'350'000.0,  // ~$3000 * 3450
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
        .contract_size = 1.0,     // 1 index unit
        .tick_size = 100.0,
        .min_order_size = 1.0,
        .max_order_size = 10000.0,
        .margin_rate = 0.20,      // 20% margin (5x) - more volatile
        .maker_fee = 0.0005,
        .taker_fee = 0.001,
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
        .contract_size = 1.0,
        .tick_size = 1000.0,
        .min_order_size = 0.1,
        .max_order_size = 1000.0,
        .margin_rate = 0.25,      // 25% margin (4x)
        .maker_fee = 0.0005,
        .taker_fee = 0.001,
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
        .contract_size = 1.0,     // 1 kg
        .tick_size = 100.0,
        .min_order_size = 10.0,
        .max_order_size = 100000.0,
        .margin_rate = 0.15,
        .maker_fee = 0.0005,
        .taker_fee = 0.001,
        .mark_price_mnt = 150'000.0,  // ~$43/kg * 3450
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
        .contract_size = 1.0,     // 1 tonne
        .tick_size = 100.0,
        .min_order_size = 100.0,
        .max_order_size = 1000000.0,
        .margin_rate = 0.15,
        .maker_fee = 0.0005,
        .taker_fee = 0.001,
        .mark_price_mnt = 690'000.0,  // ~$200/tonne * 3450
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
        .contract_size = 1.0,     // 1 tonne
        .tick_size = 100.0,
        .min_order_size = 10.0,
        .max_order_size = 100000.0,
        .margin_rate = 0.15,
        .maker_fee = 0.0005,
        .taker_fee = 0.001,
        .mark_price_mnt = 27'600'000.0,  // ~$8000/tonne * 3450
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

inline void ProductCatalog::update_funding_rate(const std::string& symbol, double rate) {
    auto it = products_.find(symbol);
    if (it != products_.end()) {
        it->second.funding_rate = rate;
    }
}

inline void ProductCatalog::update_usd_mnt_rate(double rate) {
    USD_MNT_RATE = rate;
}

inline void ProductCatalog::for_each(std::function<void(const Product&)> callback) const {
    for (const auto& [sym, prod] : products_) {
        callback(prod);
    }
}

} // namespace central_exchange
