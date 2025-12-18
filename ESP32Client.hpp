#pragma once

#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <deque>

// forward decl avoid include ixwebsocket headers
namespace ix {
    class WebSocket;
}

class ESP32Client {
public:
    ESP32Client();
    ~ESP32Client();

    void Connect(const std::string& url);
    void Disconnect();
    bool IsConnected() const { return connected; }
    
    void SendCommand(const std::string& command); // send command esp32
    
    std::string GetStatus() const;
    std::vector<std::string> GetRecentMessages(int count = 10);
    
    // error and sensor data
    std::string GetLastError() const;
    int GetUltrasonicDistance() const { return ultrasonicDistance; }
    
private:
    void OnMessage(const std::string& message);
    void OnError(const std::string& error);
    void OnConnected();
    void OnDisconnected();
    
    std::unique_ptr<ix::WebSocket> webSocket;
    std::atomic<bool> connected{false};
    std::string statusMessage;
    std::string lastError;
    std::atomic<int> ultrasonicDistance{0};
    
    std::deque<std::string> messageQueue;
    mutable std::mutex messageMutex;
    mutable std::mutex statusMutex;
    mutable std::mutex errorMutex;
    
    const size_t MAX_MESSAGES = 50; // keep last 50 msgs
};
