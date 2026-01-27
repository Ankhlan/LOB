/*
 * http_server.h - Simple HTTP server for LOB Forex Service
 * 
 * Provides REST API for:
 * - Price quotes (GET /quote/:symbol)
 * - Product catalog (GET /products)
 * - Account info (GET /account)
 * - Trade execution (POST /order)
 */
#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <sstream>
#include <winsock2.h>
#include <ws2tcpip.h>

#include "logger.h"

// Simple HTTP request
struct HttpRequest {
    std::string method;
    std::string path;
    std::string query;
    std::unordered_map<std::string, std::string> headers;
    std::string body;
    
    std::string getParam(const std::string& key) const {
        // Parse query string for parameter
        size_t pos = query.find(key + "=");
        if (pos == std::string::npos) return "";
        
        pos += key.length() + 1;
        size_t end = query.find('&', pos);
        if (end == std::string::npos) end = query.length();
        
        return query.substr(pos, end - pos);
    }
};

// Simple HTTP response
struct HttpResponse {
    int statusCode = 200;
    std::string statusText = "OK";
    std::string contentType = "application/json";
    std::string body;
    
    void json(const std::string& j) {
        contentType = "application/json";
        body = j;
    }
    
    void html(const std::string& h) {
        contentType = "text/html; charset=utf-8";
        body = h;
    }
    
    void error(int code, const std::string& msg) {
        statusCode = code;
        statusText = msg;
        body = "{\"error\":\"" + msg + "\"}";
    }
    
    std::string build() const {
        std::ostringstream oss;
        oss << "HTTP/1.1 " << statusCode << " " << statusText << "\r\n";
        oss << "Content-Type: " << contentType << "\r\n";
        oss << "Content-Length: " << body.size() << "\r\n";
        oss << "Access-Control-Allow-Origin: *\r\n";
        oss << "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n";
        oss << "Access-Control-Allow-Headers: Content-Type\r\n";
        oss << "Connection: close\r\n";
        oss << "\r\n";
        oss << body;
        return oss.str();
    }
};

// Route handler type
using RouteHandler = std::function<void(const HttpRequest&, HttpResponse&)>;

// Simple HTTP server
class HttpServer {
public:
    HttpServer(int port = 8080) : mPort(port), mRunning(false), mSocket(INVALID_SOCKET) {}
    
    ~HttpServer() {
        stop();
    }
    
    // Register route handlers
    void get(const std::string& path, RouteHandler handler) {
        mRoutes["GET " + path] = handler;
    }
    
    void post(const std::string& path, RouteHandler handler) {
        mRoutes["POST " + path] = handler;
    }
    
    // Start server
    bool start() {
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            LOG_ERROR("WSAStartup failed");
            return false;
        }
        
        mSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (mSocket == INVALID_SOCKET) {
            LOG_ERROR("socket() failed");
            return false;
        }
        
        // Allow port reuse
        int opt = 1;
        setsockopt(mSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
        
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(mPort);
        
        if (bind(mSocket, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
            LOG_ERROR("bind() failed");
            closesocket(mSocket);
            return false;
        }
        
        if (listen(mSocket, SOMAXCONN) == SOCKET_ERROR) {
            LOG_ERROR("listen() failed");
            closesocket(mSocket);
            return false;
        }
        
        mRunning = true;
        mThread = std::thread(&HttpServer::acceptLoop, this);
        
        LOG_INFO("HTTP server started on port " << mPort);
        return true;
    }
    
    void stop() {
        mRunning = false;
        if (mSocket != INVALID_SOCKET) {
            closesocket(mSocket);
            mSocket = INVALID_SOCKET;
        }
        if (mThread.joinable()) {
            mThread.join();
        }
        WSACleanup();
    }
    
private:
    void acceptLoop() {
        while (mRunning) {
            sockaddr_in clientAddr{};
            int addrLen = sizeof(clientAddr);
            
            SOCKET clientSocket = accept(mSocket, (sockaddr*)&clientAddr, &addrLen);
            if (clientSocket == INVALID_SOCKET) {
                if (mRunning) {
                    LOG_ERROR("accept() failed");
                }
                continue;
            }
            
            // Handle in separate thread
            std::thread(&HttpServer::handleClient, this, clientSocket).detach();
        }
    }
    
    void handleClient(SOCKET clientSocket) {
        char buffer[8192];
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        
        if (bytesReceived <= 0) {
            closesocket(clientSocket);
            return;
        }
        
        buffer[bytesReceived] = '\0';
        
        // Parse request
        HttpRequest req = parseRequest(buffer);
        HttpResponse res;
        
        // Handle OPTIONS (CORS preflight)
        if (req.method == "OPTIONS") {
            res.statusCode = 204;
            res.body = "";
        } else {
            // Find route
            std::string routeKey = req.method + " " + extractBasePath(req.path);
            auto it = mRoutes.find(routeKey);
            
            if (it != mRoutes.end()) {
                try {
                    it->second(req, res);
                } catch (const std::exception& e) {
                    res.error(500, e.what());
                }
            } else {
                // Try wildcard route
                routeKey = req.method + " /*";
                it = mRoutes.find(routeKey);
                if (it != mRoutes.end()) {
                    it->second(req, res);
                } else {
                    res.error(404, "Not Found");
                }
            }
        }
        
        // Send response
        std::string response = res.build();
        send(clientSocket, response.c_str(), (int)response.size(), 0);
        closesocket(clientSocket);
    }
    
    HttpRequest parseRequest(const char* raw) {
        HttpRequest req;
        std::istringstream iss(raw);
        
        // First line: METHOD /path?query HTTP/1.1
        std::string line;
        std::getline(iss, line);
        
        size_t sp1 = line.find(' ');
        size_t sp2 = line.find(' ', sp1 + 1);
        
        if (sp1 != std::string::npos && sp2 != std::string::npos) {
            req.method = line.substr(0, sp1);
            std::string fullPath = line.substr(sp1 + 1, sp2 - sp1 - 1);
            
            size_t qpos = fullPath.find('?');
            if (qpos != std::string::npos) {
                req.path = fullPath.substr(0, qpos);
                req.query = fullPath.substr(qpos + 1);
            } else {
                req.path = fullPath;
            }
        }
        
        // Headers
        while (std::getline(iss, line) && line != "\r" && !line.empty()) {
            size_t colon = line.find(':');
            if (colon != std::string::npos) {
                std::string key = line.substr(0, colon);
                std::string val = line.substr(colon + 2);
                // Remove trailing \r
                if (!val.empty() && val.back() == '\r') val.pop_back();
                req.headers[key] = val;
            }
        }
        
        // Body (rest of input)
        std::ostringstream bodyStream;
        bodyStream << iss.rdbuf();
        req.body = bodyStream.str();
        
        return req;
    }
    
    std::string extractBasePath(const std::string& path) {
        // Remove trailing slash
        std::string p = path;
        if (p.size() > 1 && p.back() == '/') p.pop_back();
        
        // For paths like /quote/XAU-MNT, return /quote
        // For paths like /products, return /products
        size_t secondSlash = p.find('/', 1);
        if (secondSlash != std::string::npos) {
            return p.substr(0, secondSlash);
        }
        return p;
    }
    
    int mPort;
    std::atomic<bool> mRunning;
    SOCKET mSocket;
    std::thread mThread;
    std::unordered_map<std::string, RouteHandler> mRoutes;
};
