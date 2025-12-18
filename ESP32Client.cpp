#include "ESP32Client.hpp"
#include <ixwebsocket/IXWebSocket.h>
#include <iostream>

ESP32Client::ESP32Client() {
    webSocket = std::make_unique<ix::WebSocket>();
}

ESP32Client::~ESP32Client() {
    Disconnect();
}

void ESP32Client::Connect(const std::string& url) {
    if (connected) {
        Disconnect();
    }

    {
        std::lock_guard<std::mutex> lock(statusMutex);
        statusMessage = "Connecting...";
    }

    webSocket->setUrl(url);
    
    // setup callbacks
    webSocket->setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Message) {
            OnMessage(msg->str);
        }
        else if (msg->type == ix::WebSocketMessageType::Open) {
            OnConnected();
        }
        else if (msg->type == ix::WebSocketMessageType::Close) {
            OnDisconnected();
        }
        else if (msg->type == ix::WebSocketMessageType::Error) {
            OnError(msg->errorInfo.reason);
        }
    });

    // start connection
    webSocket->start();
}

void ESP32Client::Disconnect() {
    if (webSocket) {
        webSocket->stop();
    }
    connected = false;
    
    std::lock_guard<std::mutex> lock(statusMutex);
    statusMessage = "Disconnected";
}

void ESP32Client::SendCommand(const std::string& command) {
    if (webSocket && connected) {
        webSocket->send(command);
        std::cout << "Sent command: " << command << std::endl;
    } else {
        std::cout << "Cannot send command - not connected" << std::endl;
    }
}

std::string ESP32Client::GetStatus() const {
    std::lock_guard<std::mutex> lock(statusMutex);
    return statusMessage;
}

std::vector<std::string> ESP32Client::GetRecentMessages(int count) {
    std::lock_guard<std::mutex> lock(messageMutex);
    
    std::vector<std::string> result;
    int start = std::max(0, (int)messageQueue.size() - count);
    
    for (int i = start; i < (int)messageQueue.size(); i++) {
        result.push_back(messageQueue[i]);
    }
    
    return result;
}

void ESP32Client::OnMessage(const std::string& message) {
    std::cout << "ESP32: " << message << std::endl;
    
    // parse json msgs for distance and errors
    // simple json parsing specific patterns
    if (message.find(R"({"distance":)") != std::string::npos) {
        // extract distance value
        size_t pos = message.find(R"("distance":)");
        if (pos != std::string::npos) {
            size_t start = pos + 12; // length of "distance":
            size_t end = message.find_first_of(",}", start);
            if (end != std::string::npos) {
                std::string distStr = message.substr(start, end - start);
                ultrasonicDistance = std::stoi(distStr);
            }
        }
    }
    
    if (message.find("\"status\":\"error\"") != std::string::npos) {
        // extract error msg
        size_t pos = message.find("\"message\":\"");
        if (pos != std::string::npos) {
            size_t start = pos + 11; // length of "message":
            size_t end = message.find("\"", start);
            if (end != std::string::npos) {
                std::lock_guard<std::mutex> lock(errorMutex);
                lastError = message.substr(start, end - start);
            }
        }
    }
    
    std::lock_guard<std::mutex> lock(messageMutex);
    messageQueue.push_back(message);
    
    // keep only last max msgs
    while (messageQueue.size() > MAX_MESSAGES) {
        messageQueue.pop_front();
    }
}

std::string ESP32Client::GetLastError() const {
    std::lock_guard<std::mutex> lock(errorMutex);
    return lastError;
}

void ESP32Client::OnError(const std::string& error) {
    std::lock_guard<std::mutex> lock(statusMutex);
    statusMessage = "Error: " + error;
    connected = false;
}

void ESP32Client::OnConnected() {
    connected = true;
    
    std::lock_guard<std::mutex> lock(statusMutex);
    statusMessage = "WebSocket initialized";
    
    std::cout << "Connected to ESP32!" << std::endl;
}

void ESP32Client::OnDisconnected() {
    connected = false;
    
    std::lock_guard<std::mutex> lock(statusMutex);
    if (statusMessage != "Disconnected") {
        statusMessage = "Connection lost";
    }
}
