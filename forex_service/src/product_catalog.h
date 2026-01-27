/*
 * product_catalog.h - Product definitions for Mongolia market
 * 
 * Maps FXCM instruments to retail products with markup
 * for resale to Mongolian traders.
 */
#pragma once

#include <string>
#include <vector>
#include <unordered_map>

// Product category
enum class ProductCategory {
    FX_MAJOR,
    FX_MINOR,
    FX_EXOTIC,
    COMMODITY,
    INDEX,
    CRYPTO
};

// Product definition
struct Product {
    std::string symbol;         // LOB symbol (e.g., "XAU-MNT")
    std::string fxcmSymbol;     // FXCM underlying (e.g., "XAU/USD")
    std::string name;           // Display name
    std::string description;    // Product description
    ProductCategory category;
    
    double contractSize;        // Units per contract
    double tickSize;            // Minimum price movement
    double spreadMarkup;        // Additional spread in pips
    double minOrderSize;        // Minimum order in contracts
    double maxOrderSize;        // Maximum order in contracts
    double marginRate;          // Initial margin requirement
    
    bool isActive;              // Trading enabled
    bool isMNTQuoted;           // Quoted in Mongolian Tugrik
    
    // Trading hours (UTC)
    int openHour;
    int closeHour;
};

// Get product catalog
inline std::vector<Product> getProductCatalog() {
    return {
        // ═══════════════════════════════════════════════════════════════
        // COMMODITIES - Primary focus for Mongolia
        // ═══════════════════════════════════════════════════════════════
        {
            "XAU-MNT",          // Gold in Tugrik
            "XAU/USD",
            "Gold",
            "Gold spot price quoted in MNT",
            ProductCategory::COMMODITY,
            1.0,                // 1 oz per contract
            10.0,               // 10 MNT tick
            2.0,                // 2 pip markup
            0.01,               // Min 0.01 oz
            100.0,              // Max 100 oz
            0.05,               // 5% margin
            true,
            true,
            0, 24               // 24/5 trading
        },
        {
            "XAG-MNT",          // Silver in Tugrik
            "XAG/USD",
            "Silver",
            "Silver spot price quoted in MNT",
            ProductCategory::COMMODITY,
            50.0,               // 50 oz per contract
            1.0,                // 1 MNT tick
            3.0,                // 3 pip markup
            0.1,                // Min 0.1 contract
            200.0,              // Max 200 contracts
            0.05,               // 5% margin
            true,
            true,
            0, 24
        },
        {
            "COPPER-MNT",       // Copper in Tugrik
            "Copper",
            "Copper",
            "Copper price quoted in MNT (Mongolia is major producer)",
            ProductCategory::COMMODITY,
            25.0,               // 25 lbs per contract
            1.0,
            3.0,
            0.1,
            100.0,
            0.05,
            true,
            true,
            0, 24
        },
        {
            "COAL-MNT",         // Thermal coal
            "UKOil",            // Proxy using oil correlation
            "Coal",
            "Thermal coal index (Mongolia export)",
            ProductCategory::COMMODITY,
            100.0,
            10.0,
            5.0,
            1.0,
            50.0,
            0.10,
            true,
            true,
            0, 24
        },
        {
            "OIL-MNT",          // Crude oil
            "USOil",
            "Crude Oil",
            "WTI crude oil quoted in MNT",
            ProductCategory::COMMODITY,
            10.0,               // 10 barrels
            100.0,
            2.0,
            0.1,
            100.0,
            0.05,
            true,
            true,
            0, 24
        },
        {
            "NATGAS-MNT",       // Natural gas
            "NGAS",
            "Natural Gas",
            "Natural gas quoted in MNT",
            ProductCategory::COMMODITY,
            1000.0,             // 1000 MMBtu
            10.0,
            3.0,
            0.1,
            50.0,
            0.10,
            true,
            true,
            0, 24
        },
        
        // ═══════════════════════════════════════════════════════════════
        // FX MAJORS - High liquidity pairs
        // ═══════════════════════════════════════════════════════════════
        {
            "USD-MNT",
            "USD/CNH",          // Use CNH as proxy (highly correlated)
            "US Dollar",
            "USD/MNT exchange rate",
            ProductCategory::FX_MAJOR,
            100000.0,           // 100K USD
            0.01,
            1.0,
            0.01,
            100.0,
            0.02,               // 2% margin (50:1)
            true,
            true,
            0, 24
        },
        {
            "EUR-MNT",
            "EUR/USD",
            "Euro",
            "EUR/MNT exchange rate",
            ProductCategory::FX_MAJOR,
            100000.0,
            0.01,
            1.0,
            0.01,
            100.0,
            0.02,
            true,
            true,
            0, 24
        },
        {
            "CNY-MNT",
            "USD/CNH",
            "Chinese Yuan",
            "CNY/MNT exchange rate (key trade partner)",
            ProductCategory::FX_MAJOR,
            100000.0,
            0.01,
            1.5,
            0.01,
            100.0,
            0.02,
            true,
            true,
            0, 24
        },
        {
            "RUB-MNT",
            "USD/RUB",
            "Russian Ruble",
            "RUB/MNT exchange rate (key trade partner)",
            ProductCategory::FX_EXOTIC,
            1000000.0,          // 1M RUB
            0.001,
            5.0,
            0.1,
            50.0,
            0.05,               // Higher margin for exotic
            true,
            true,
            0, 24
        },
        {
            "JPY-MNT",
            "USD/JPY",
            "Japanese Yen",
            "JPY/MNT exchange rate",
            ProductCategory::FX_MAJOR,
            10000000.0,         // 10M JPY
            0.0001,
            1.0,
            0.01,
            100.0,
            0.02,
            true,
            true,
            0, 24
        },
        {
            "KRW-MNT",
            "USD/KRW",
            "Korean Won",
            "KRW/MNT exchange rate (investment partner)",
            ProductCategory::FX_EXOTIC,
            100000000.0,        // 100M KRW
            0.00001,
            3.0,
            0.1,
            50.0,
            0.05,
            true,
            true,
            0, 24
        },
        
        // ═══════════════════════════════════════════════════════════════
        // STOCK INDICES
        // ═══════════════════════════════════════════════════════════════
        {
            "SPX-MNT",
            "SPX500",
            "S&P 500",
            "US stock market index in MNT",
            ProductCategory::INDEX,
            1.0,
            1000.0,
            1.0,
            0.1,
            100.0,
            0.05,
            true,
            true,
            14, 21              // US market hours (UTC)
        },
        {
            "NDX-MNT",
            "NAS100",
            "NASDAQ 100",
            "US tech index in MNT",
            ProductCategory::INDEX,
            1.0,
            1000.0,
            1.0,
            0.1,
            100.0,
            0.05,
            true,
            true,
            14, 21
        },
        {
            "HSI-MNT",
            "HKG33",
            "Hang Seng",
            "Hong Kong index in MNT",
            ProductCategory::INDEX,
            1.0,
            100.0,
            2.0,
            0.1,
            100.0,
            0.05,
            true,
            true,
            1, 8                // HK market hours (UTC)
        },
        {
            "NKY-MNT",
            "JPN225",
            "Nikkei 225",
            "Japan index in MNT",
            ProductCategory::INDEX,
            1.0,
            1000.0,
            2.0,
            0.1,
            100.0,
            0.05,
            true,
            true,
            0, 6                // Japan hours (UTC)
        },
        
        // ═══════════════════════════════════════════════════════════════
        // CRYPTO (if available on FXCM)
        // ═══════════════════════════════════════════════════════════════
        {
            "BTC-MNT",
            "BTC/USD",
            "Bitcoin",
            "Bitcoin in MNT",
            ProductCategory::CRYPTO,
            1.0,
            100000.0,
            5.0,
            0.001,
            10.0,
            0.20,               // 20% margin (5:1)
            true,
            true,
            0, 24               // 24/7
        },
        {
            "ETH-MNT",
            "ETH/USD",
            "Ethereum",
            "Ethereum in MNT",
            ProductCategory::CRYPTO,
            1.0,
            10000.0,
            5.0,
            0.01,
            100.0,
            0.20,
            true,
            true,
            0, 24
        }
    };
}

// Get product by symbol
inline const Product* findProduct(const std::string& symbol) {
    static auto catalog = getProductCatalog();
    static std::unordered_map<std::string, const Product*> index;
    
    if (index.empty()) {
        for (const auto& p : catalog) {
            index[p.symbol] = &p;
        }
    }
    
    auto it = index.find(symbol);
    return it != index.end() ? it->second : nullptr;
}

// Get all symbols for a category
inline std::vector<std::string> getSymbolsByCategory(ProductCategory cat) {
    std::vector<std::string> symbols;
    for (const auto& p : getProductCatalog()) {
        if (p.category == cat && p.isActive) {
            symbols.push_back(p.symbol);
        }
    }
    return symbols;
}

// Get FXCM symbols we need to subscribe to
inline std::vector<std::string> getFxcmSymbols() {
    std::vector<std::string> symbols;
    std::unordered_map<std::string, bool> seen;
    
    for (const auto& p : getProductCatalog()) {
        if (p.isActive && !seen[p.fxcmSymbol]) {
            symbols.push_back(p.fxcmSymbol);
            seen[p.fxcmSymbol] = true;
        }
    }
    return symbols;
}
