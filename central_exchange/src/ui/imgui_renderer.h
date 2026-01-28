#pragma once
/**
 * Dear ImGui Renderer - Native Desktop Trading Client
 * ====================================================
 * High-performance rendering for order books, charts, and positions.
 * 
 * Uses: GLFW + OpenGL3 + Dear ImGui
 */

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>

#include <string>
#include <functional>
#include <stdexcept>

namespace cre {
namespace ui {

/**
 * Configuration for the ImGui renderer
 */
struct RendererConfig {
    std::string title = "Central Exchange - Trading Client";
    int width = 1600;
    int height = 900;
    bool vsync = true;
    bool maximized = false;
    
    // One Half Dark theme colors (matching web UI)
    ImVec4 bg_primary{0.157f, 0.173f, 0.204f, 1.0f};     // #282c34
    ImVec4 bg_secondary{0.133f, 0.149f, 0.173f, 1.0f};   // #21252b
    ImVec4 text_primary{0.671f, 0.698f, 0.749f, 1.0f};   // #abb2bf
    ImVec4 accent{0.380f, 0.686f, 0.937f, 1.0f};         // #61afef
    ImVec4 green{0.596f, 0.765f, 0.475f, 1.0f};          // #98c379
    ImVec4 red{0.878f, 0.424f, 0.459f, 1.0f};            // #e06c75
};

/**
 * Main ImGui renderer class
 */
class ImGuiRenderer {
public:
    ImGuiRenderer() = default;
    ~ImGuiRenderer() { shutdown(); }
    
    // Disable copy
    ImGuiRenderer(const ImGuiRenderer&) = delete;
    ImGuiRenderer& operator=(const ImGuiRenderer&) = delete;
    
    /**
     * Initialize GLFW window and ImGui context
     */
    bool init(const RendererConfig& config = RendererConfig()) {
        config_ = config;
        
        // Initialize GLFW
        if (!glfwInit()) {
            return false;
        }
        
        // GL 3.3 Core profile
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
        
        if (config_.maximized) {
            glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);
        }
        
        // Create window
        window_ = glfwCreateWindow(
            config_.width, config_.height,
            config_.title.c_str(),
            nullptr, nullptr
        );
        
        if (!window_) {
            glfwTerminate();
            return false;
        }
        
        glfwMakeContextCurrent(window_);
        glfwSwapInterval(config_.vsync ? 1 : 0);
        
        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        
        // Setup Platform/Renderer backends
        ImGui_ImplGlfw_InitForOpenGL(window_, true);
        ImGui_ImplOpenGL3_Init("#version 330");
        
        // Apply custom theme
        applyTheme();
        
        initialized_ = true;
        return true;
    }
    
    /**
     * Shutdown and cleanup
     */
    void shutdown() {
        if (!initialized_) return;
        
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        
        if (window_) {
            glfwDestroyWindow(window_);
            window_ = nullptr;
        }
        glfwTerminate();
        
        initialized_ = false;
    }
    
    /**
     * Check if window should close
     */
    bool shouldClose() const {
        return window_ ? glfwWindowShouldClose(window_) : true;
    }
    
    /**
     * Begin new frame
     */
    void beginFrame() {
        glfwPollEvents();
        
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }
    
    /**
     * End frame and render
     */
    void endFrame() {
        ImGui::Render();
        
        int display_w, display_h;
        glfwGetFramebufferSize(window_, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        
        glClearColor(
            config_.bg_primary.x,
            config_.bg_primary.y,
            config_.bg_primary.z,
            config_.bg_primary.w
        );
        glClear(GL_COLOR_BUFFER_BIT);
        
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        glfwSwapBuffers(window_);
    }
    
    /**
     * Main render loop with callback
     */
    void run(std::function<void()> renderCallback) {
        while (!shouldClose()) {
            beginFrame();
            
            // Create full-window docking space
            ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());
            
            // User render callback
            if (renderCallback) {
                renderCallback();
            }
            
            endFrame();
        }
    }
    
    /**
     * Get window pointer (for advanced use)
     */
    GLFWwindow* getWindow() const { return window_; }
    
    /**
     * Get config
     */
    const RendererConfig& getConfig() const { return config_; }

private:
    /**
     * Apply One Half Dark theme
     */
    void applyTheme() {
        ImGuiStyle& style = ImGui::GetStyle();
        ImVec4* colors = style.Colors;
        
        // Main
        colors[ImGuiCol_WindowBg] = config_.bg_primary;
        colors[ImGuiCol_ChildBg] = config_.bg_secondary;
        colors[ImGuiCol_PopupBg] = config_.bg_secondary;
        colors[ImGuiCol_Border] = ImVec4(0.243f, 0.267f, 0.314f, 1.0f); // #3e4451
        
        // Text
        colors[ImGuiCol_Text] = config_.text_primary;
        colors[ImGuiCol_TextDisabled] = ImVec4(0.4f, 0.4f, 0.45f, 1.0f);
        
        // Headers
        colors[ImGuiCol_Header] = ImVec4(0.2f, 0.22f, 0.27f, 1.0f);
        colors[ImGuiCol_HeaderHovered] = config_.accent;
        colors[ImGuiCol_HeaderActive] = config_.accent;
        
        // Buttons
        colors[ImGuiCol_Button] = ImVec4(0.2f, 0.22f, 0.27f, 1.0f);
        colors[ImGuiCol_ButtonHovered] = config_.accent;
        colors[ImGuiCol_ButtonActive] = ImVec4(0.3f, 0.58f, 0.82f, 1.0f);
        
        // Frame (inputs, sliders)
        colors[ImGuiCol_FrameBg] = config_.bg_secondary;
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.24f, 0.29f, 1.0f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.25f, 0.27f, 0.32f, 1.0f);
        
        // Tabs
        colors[ImGuiCol_Tab] = config_.bg_secondary;
        colors[ImGuiCol_TabHovered] = config_.accent;
        colors[ImGuiCol_TabActive] = ImVec4(0.2f, 0.4f, 0.6f, 1.0f);
        colors[ImGuiCol_TabUnfocused] = config_.bg_secondary;
        colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.18f, 0.35f, 0.52f, 1.0f);
        
        // Title bar
        colors[ImGuiCol_TitleBg] = config_.bg_secondary;
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.17f, 0.21f, 1.0f);
        colors[ImGuiCol_TitleBgCollapsed] = config_.bg_secondary;
        
        // Scrollbar
        colors[ImGuiCol_ScrollbarBg] = config_.bg_primary;
        colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.3f, 0.32f, 0.37f, 1.0f);
        colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.35f, 0.37f, 0.42f, 1.0f);
        colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.4f, 0.42f, 0.47f, 1.0f);
        
        // Separator
        colors[ImGuiCol_Separator] = ImVec4(0.243f, 0.267f, 0.314f, 1.0f);
        
        // Resize grip
        colors[ImGuiCol_ResizeGrip] = ImVec4(0.3f, 0.3f, 0.35f, 0.6f);
        colors[ImGuiCol_ResizeGripHovered] = config_.accent;
        colors[ImGuiCol_ResizeGripActive] = config_.accent;
        
        // Docking
        colors[ImGuiCol_DockingPreview] = config_.accent;
        colors[ImGuiCol_DockingEmptyBg] = config_.bg_secondary;
        
        // Tables
        colors[ImGuiCol_TableHeaderBg] = config_.bg_secondary;
        colors[ImGuiCol_TableBorderStrong] = ImVec4(0.243f, 0.267f, 0.314f, 1.0f);
        colors[ImGuiCol_TableBorderLight] = ImVec4(0.2f, 0.22f, 0.27f, 1.0f);
        colors[ImGuiCol_TableRowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        colors[ImGuiCol_TableRowBgAlt] = ImVec4(0.1f, 0.1f, 0.1f, 0.1f);
        
        // Style tweaks
        style.WindowRounding = 4.0f;
        style.ChildRounding = 4.0f;
        style.FrameRounding = 2.0f;
        style.PopupRounding = 4.0f;
        style.ScrollbarRounding = 4.0f;
        style.GrabRounding = 2.0f;
        style.TabRounding = 4.0f;
        
        style.WindowPadding = ImVec2(10, 10);
        style.FramePadding = ImVec2(8, 4);
        style.ItemSpacing = ImVec2(8, 4);
        
        style.WindowBorderSize = 1.0f;
        style.FrameBorderSize = 0.0f;
        style.PopupBorderSize = 1.0f;
    }
    
    GLFWwindow* window_ = nullptr;
    RendererConfig config_;
    bool initialized_ = false;
};

// =============================================================================
// Trading UI Colors (for convenience)
// =============================================================================

namespace colors {
    inline ImVec4 bid() { return ImVec4(0.596f, 0.765f, 0.475f, 1.0f); }   // green
    inline ImVec4 ask() { return ImVec4(0.878f, 0.424f, 0.459f, 1.0f); }   // red
    inline ImVec4 profit() { return ImVec4(0.596f, 0.765f, 0.475f, 1.0f); }
    inline ImVec4 loss() { return ImVec4(0.878f, 0.424f, 0.459f, 1.0f); }
    inline ImVec4 accent() { return ImVec4(0.380f, 0.686f, 0.937f, 1.0f); }
    inline ImVec4 muted() { return ImVec4(0.5f, 0.5f, 0.55f, 1.0f); }
}

} // namespace ui
} // namespace cre
