/**
 * Central Exchange - Native Trading Client
 * =========================================
 * High-performance trading client using Dear ImGui
 * 
 * Build: cmake -DBUILD_GUI_CLIENT=ON .. && make
 */

#include "ui/imgui_renderer.h"
#include <nlohmann/json.hpp>

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <thread>
#include <mutex>
#include <atomic>

using json = nlohmann::json;

// =============================================================================
// Market Data Structures
// =============================================================================

struct Quote {
    std::string symbol;
    double bid = 0;
    double ask = 0;
    double mid = 0;
    double spread = 0;
    double mark = 0;
};

struct OrderBookLevel {
    double price;
    double quantity;
};

struct OrderBook {
    std::string symbol;
    std::vector<OrderBookLevel> bids;
    std::vector<OrderBookLevel> asks;
    double best_bid = 0;
    double best_ask = 0;
};

struct Position {
    std::string symbol;
    std::string side;
    double size = 0;
    double entry_price = 0;
    double current_price = 0;
    double unrealized_pnl = 0;
    double pnl_percent = 0;
    double margin_used = 0;
};

struct Account {
    std::string user_id;
    double balance = 0;
    double equity = 0;
    double available = 0;
    double margin_used = 0;
    double unrealized_pnl = 0;
};

struct Product {
    std::string symbol;
    std::string name;
    std::string category;
    double mark_price = 0;
    double margin_rate = 0;
    double contract_size = 1;
};

// =============================================================================
// Application State
// =============================================================================

struct AppState {
    std::string server_url = "http://localhost:8080";
    std::string user_id = "demo";
    
    // Market data
    std::map<std::string, Quote> quotes;
    std::map<std::string, OrderBook> orderbooks;
    std::vector<Product> products;
    std::vector<Position> positions;
    Account account;
    
    // UI state
    std::string selected_symbol = "XAU-MNT-PERP";
    std::string trade_side = "long";
    double trade_quantity = 1.0;
    double trade_leverage = 10.0;
    
    // Connection state
    bool connected = false;
    std::atomic<bool> running{true};
    
    // Thread safety
    std::mutex data_mutex;
};

AppState g_state;

// =============================================================================
// UI Panels (to be implemented by KITSUNE)
// =============================================================================

namespace panels {

void drawMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Connect", "Ctrl+C")) {
                // TODO: Connect to server
            }
            if (ImGui::MenuItem("Disconnect")) {
                g_state.connected = false;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) {
                g_state.running = false;
            }
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Order Book", nullptr, nullptr);
            ImGui::MenuItem("Positions", nullptr, nullptr);
            ImGui::MenuItem("Trade Ticker", nullptr, nullptr);
            ImGui::EndMenu();
        }
        
        if (ImGui::BeginMenu("Account")) {
            if (ImGui::MenuItem("Deposit")) {
                // TODO: QPay integration
            }
            ImGui::MenuItem("Withdraw");
            ImGui::Separator();
            ImGui::MenuItem("Settings");
            ImGui::EndMenu();
        }
        
        // Right-aligned status
        ImGui::SameLine(ImGui::GetWindowWidth() - 200);
        if (g_state.connected) {
            ImGui::TextColored(cre::ui::colors::profit(), "● Connected");
        } else {
            ImGui::TextColored(cre::ui::colors::loss(), "● Disconnected");
        }
        
        ImGui::EndMainMenuBar();
    }
}

void drawInstrumentList() {
    ImGui::Begin("Instruments");
    
    static char search[64] = "";
    ImGui::InputTextWithHint("##search", "Search...", search, sizeof(search));
    
    ImGui::Separator();
    
    if (ImGui::BeginTable("instruments", 3, ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Symbol", ImGuiTableColumnFlags_WidthFixed, 120);
        ImGui::TableSetupColumn("Bid", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("Ask", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableHeadersRow();
        
        std::lock_guard<std::mutex> lock(g_state.data_mutex);
        for (const auto& [symbol, quote] : g_state.quotes) {
            if (strlen(search) > 0 && symbol.find(search) == std::string::npos) {
                continue;
            }
            
            ImGui::TableNextRow();
            
            // Symbol column - selectable
            ImGui::TableSetColumnIndex(0);
            bool is_selected = (symbol == g_state.selected_symbol);
            if (ImGui::Selectable(symbol.c_str(), is_selected, ImGuiSelectableFlags_SpanAllColumns)) {
                g_state.selected_symbol = symbol;
            }
            
            // Bid column
            ImGui::TableSetColumnIndex(1);
            ImGui::TextColored(cre::ui::colors::bid(), "%.0f", quote.bid);
            
            // Ask column
            ImGui::TableSetColumnIndex(2);
            ImGui::TextColored(cre::ui::colors::ask(), "%.0f", quote.ask);
        }
        
        ImGui::EndTable();
    }
    
    ImGui::End();
}

void drawOrderBook() {
    ImGui::Begin("Order Book");
    
    ImGui::Text("Symbol: %s", g_state.selected_symbol.c_str());
    
    std::lock_guard<std::mutex> lock(g_state.data_mutex);
    auto it = g_state.orderbooks.find(g_state.selected_symbol);
    
    if (it != g_state.orderbooks.end()) {
        const auto& book = it->second;
        
        // Asks (reversed - lowest at bottom)
        ImGui::TextColored(cre::ui::colors::muted(), "ASKS");
        if (ImGui::BeginTable("asks", 2, ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Price", ImGuiTableColumnFlags_WidthFixed, 100);
            ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 80);
            
            for (auto rit = book.asks.rbegin(); rit != book.asks.rend(); ++rit) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextColored(cre::ui::colors::ask(), "%.0f", rit->price);
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%.4f", rit->quantity);
            }
            ImGui::EndTable();
        }
        
        // Spread
        ImGui::Separator();
        auto quote_it = g_state.quotes.find(g_state.selected_symbol);
        if (quote_it != g_state.quotes.end()) {
            ImGui::TextColored(cre::ui::colors::accent(), "Spread: %.0f MNT", quote_it->second.spread);
        }
        ImGui::Separator();
        
        // Bids
        ImGui::TextColored(cre::ui::colors::muted(), "BIDS");
        if (ImGui::BeginTable("bids", 2, ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Price", ImGuiTableColumnFlags_WidthFixed, 100);
            ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 80);
            
            for (const auto& level : book.bids) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextColored(cre::ui::colors::bid(), "%.0f", level.price);
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%.4f", level.quantity);
            }
            ImGui::EndTable();
        }
    } else {
        ImGui::TextColored(cre::ui::colors::muted(), "No order book data");
    }
    
    ImGui::End();
}

void drawTradePanel() {
    ImGui::Begin("Trade");
    
    ImGui::Text("Symbol: %s", g_state.selected_symbol.c_str());
    ImGui::Separator();
    
    // Side buttons
    ImGui::PushStyleColor(ImGuiCol_Button, 
        g_state.trade_side == "long" ? ImVec4(0.2f, 0.5f, 0.2f, 1.0f) : ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
    if (ImGui::Button("BUY", ImVec2(80, 30))) {
        g_state.trade_side = "long";
    }
    ImGui::PopStyleColor();
    
    ImGui::SameLine();
    
    ImGui::PushStyleColor(ImGuiCol_Button,
        g_state.trade_side == "short" ? ImVec4(0.5f, 0.2f, 0.2f, 1.0f) : ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
    if (ImGui::Button("SELL", ImVec2(80, 30))) {
        g_state.trade_side = "short";
    }
    ImGui::PopStyleColor();
    
    ImGui::Separator();
    
    // Quantity input
    ImGui::Text("Quantity");
    ImGui::InputDouble("##qty", &g_state.trade_quantity, 0.1, 1.0, "%.4f");
    
    // Leverage input
    ImGui::Text("Leverage");
    ImGui::InputDouble("##lev", &g_state.trade_leverage, 1.0, 5.0, "%.0fx");
    
    ImGui::Separator();
    
    // Order summary
    std::lock_guard<std::mutex> lock(g_state.data_mutex);
    auto quote_it = g_state.quotes.find(g_state.selected_symbol);
    if (quote_it != g_state.quotes.end()) {
        double price = g_state.trade_side == "long" ? quote_it->second.ask : quote_it->second.bid;
        double value = price * g_state.trade_quantity;
        double margin = value / g_state.trade_leverage;
        double fee = value * 0.0005;
        
        ImGui::Text("Entry: %.0f MNT", price);
        ImGui::Text("Value: %.0f MNT", value);
        ImGui::Text("Margin: %.0f MNT", margin);
        ImGui::TextColored(cre::ui::colors::muted(), "Fee: %.0f MNT", fee);
    }
    
    ImGui::Separator();
    
    // Submit button
    ImVec4 btn_color = g_state.trade_side == "long" 
        ? cre::ui::colors::bid() 
        : cre::ui::colors::ask();
    ImGui::PushStyleColor(ImGuiCol_Button, btn_color);
    if (ImGui::Button(g_state.trade_side == "long" ? "BUY NOW" : "SELL NOW", ImVec2(-1, 35))) {
        // TODO: Submit order
    }
    ImGui::PopStyleColor();
    
    ImGui::End();
}

void drawPositions() {
    ImGui::Begin("Positions");
    
    std::lock_guard<std::mutex> lock(g_state.data_mutex);
    
    // Account summary
    ImGui::Text("Equity: ");
    ImGui::SameLine();
    ImGui::TextColored(cre::ui::colors::accent(), "%.0f MNT", g_state.account.equity);
    
    ImGui::SameLine(150);
    ImGui::Text("Available: ");
    ImGui::SameLine();
    ImGui::Text("%.0f MNT", g_state.account.available);
    
    ImGui::SameLine(300);
    ImGui::Text("Margin: ");
    ImGui::SameLine();
    ImGui::Text("%.0f MNT", g_state.account.margin_used);
    
    ImGui::Separator();
    
    // Positions table
    if (ImGui::BeginTable("positions", 7, ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders)) {
        ImGui::TableSetupColumn("Symbol", ImGuiTableColumnFlags_WidthFixed, 120);
        ImGui::TableSetupColumn("Side", ImGuiTableColumnFlags_WidthFixed, 50);
        ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 70);
        ImGui::TableSetupColumn("Entry", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("Mark", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("P&L", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 50);
        ImGui::TableHeadersRow();
        
        for (const auto& pos : g_state.positions) {
            ImGui::TableNextRow();
            
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s", pos.symbol.c_str());
            
            ImGui::TableSetColumnIndex(1);
            ImVec4 side_color = pos.side == "long" ? cre::ui::colors::bid() : cre::ui::colors::ask();
            ImGui::TextColored(side_color, "%s", pos.side == "long" ? "LONG" : "SHORT");
            
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.4f", pos.size);
            
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%.0f", pos.entry_price);
            
            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%.0f", pos.current_price);
            
            ImGui::TableSetColumnIndex(5);
            ImVec4 pnl_color = pos.unrealized_pnl >= 0 ? cre::ui::colors::profit() : cre::ui::colors::loss();
            ImGui::TextColored(pnl_color, "%+.0f (%.2f%%)", pos.unrealized_pnl, pos.pnl_percent);
            
            ImGui::TableSetColumnIndex(6);
            ImGui::PushID(pos.symbol.c_str());
            if (ImGui::SmallButton("X")) {
                // TODO: Close position
            }
            ImGui::PopID();
        }
        
        if (g_state.positions.empty()) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextColored(cre::ui::colors::muted(), "No open positions");
        }
        
        ImGui::EndTable();
    }
    
    ImGui::End();
}

void drawStatusBar() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + viewport->Size.y - 25));
    ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, 25));
    
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoBringToFrontOnFocus;
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 4));
    if (ImGui::Begin("##StatusBar", nullptr, flags)) {
        // Connection status
        if (g_state.connected) {
            ImGui::TextColored(cre::ui::colors::profit(), "●");
        } else {
            ImGui::TextColored(cre::ui::colors::loss(), "●");
        }
        ImGui::SameLine();
        ImGui::Text("Engine: %s", g_state.connected ? "Online" : "Offline");
        
        ImGui::SameLine(200);
        ImGui::Text("USD/MNT: 3,450");
        
        ImGui::SameLine(350);
        ImGui::Text("Margin Used: %.0f MNT", g_state.account.margin_used);
        
        ImGui::SameLine(550);
        ImGui::TextColored(cre::ui::colors::profit(), "Free: %.0f MNT", g_state.account.available);
        
        // Server time (right-aligned)
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        char time_str[16];
        std::strftime(time_str, sizeof(time_str), "%H:%M:%S", std::localtime(&time));
        
        ImGui::SameLine(viewport->Size.x - 80);
        ImGui::Text("%s", time_str);
    }
    ImGui::End();
    ImGui::PopStyleVar();
}

} // namespace panels

// =============================================================================
// Main Application
// =============================================================================

void renderUI() {
    panels::drawMenuBar();
    panels::drawInstrumentList();
    panels::drawOrderBook();
    panels::drawTradePanel();
    panels::drawPositions();
    panels::drawStatusBar();
}

int main(int argc, char* argv[]) {
    std::cout << "Central Exchange - Trading Client\n";
    std::cout << "=================================\n\n";
    
    // Parse command line
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--server" && i + 1 < argc) {
            g_state.server_url = argv[++i];
        } else if (arg == "--user" && i + 1 < argc) {
            g_state.user_id = argv[++i];
        }
    }
    
    std::cout << "Server: " << g_state.server_url << "\n";
    std::cout << "User: " << g_state.user_id << "\n\n";
    
    // Initialize renderer
    cre::ui::ImGuiRenderer renderer;
    cre::ui::RendererConfig config;
    config.title = "Central Exchange - Trading Client";
    config.width = 1600;
    config.height = 900;
    config.maximized = true;
    
    if (!renderer.init(config)) {
        std::cerr << "Failed to initialize renderer!\n";
        return 1;
    }
    
    std::cout << "Renderer initialized. Starting main loop...\n";
    
    // Add some demo data
    {
        std::lock_guard<std::mutex> lock(g_state.data_mutex);
        
        g_state.quotes["XAU-MNT-PERP"] = {"XAU-MNT-PERP", 9650000, 9660000, 9655000, 10000, 9655000};
        g_state.quotes["BTC-MNT-PERP"] = {"BTC-MNT-PERP", 345000000, 345500000, 345250000, 500000, 345250000};
        g_state.quotes["EUR-MNT-PERP"] = {"EUR-MNT-PERP", 3725, 3727, 3726, 2, 3726};
        g_state.quotes["OIL-MNT-PERP"] = {"OIL-MNT-PERP", 259500, 260500, 260000, 1000, 260000};
        g_state.quotes["MN-COAL-PERP"] = {"MN-COAL-PERP", 689000, 691000, 690000, 2000, 690000};
        
        // Demo order book
        OrderBook& book = g_state.orderbooks["XAU-MNT-PERP"];
        book.symbol = "XAU-MNT-PERP";
        book.bids = {{9650000, 1.5}, {9649000, 2.0}, {9648000, 3.5}, {9647000, 1.0}, {9646000, 4.0}};
        book.asks = {{9660000, 1.2}, {9661000, 2.5}, {9662000, 1.8}, {9663000, 3.0}, {9664000, 2.0}};
        
        g_state.account = {"demo", 100000000, 100000000, 100000000, 0, 0};
    }
    
    // Main loop
    renderer.run(renderUI);
    
    g_state.running = false;
    
    std::cout << "Shutting down...\n";
    return 0;
}
