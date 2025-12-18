#pragma once

#include "features/Prediction.hpp"
#include "features/DistanceEstimator.hpp"
#include "features/Tracer.hpp" // added tracer
#include "communication/ESP32Client.hpp" // organized in comm folder
#include "analytics/PerformanceLogger.hpp" // analytics stuff
#include "GuiLayer.hpp"
#include "TrashDetector.hpp"
#include "ScreenCapture.hpp"
#include <opencv2/opencv.hpp>
#include <vector>
#include <d3d11.h>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>

class App {
public:
    App();
    ~App();

    void Run();

private:
    void RenderGui();
    void UpdateTexture(const cv::Mat& mat);
    
    // scan for models in ai folder
    void RefreshModelList();
    void RefreshLabelList();

    GuiLayer gui;
    TrashDetector detector;
    ScreenCapture capturer;
    
    // FEATURES NEW
    Prediction prediction;
    DistanceEstimator distanceEst;
    Tracer tracer; // tracer thing
    ESP32Client esp32Client; // esp client
    PerformanceLogger perfLogger; // analytics
    // removed lastdetect time we use capturetime now

    // Threading
    std::thread workerThread;
    std::atomic<bool> shouldStop = false;
    std::mutex dataMutex;
    
    // Shared Data (mutex protected)
    std::vector<Detection> sharedDetections;
    cv::Mat sharedFrame;
    int sharedWidth = 1920;
    int sharedHeight = 1080;
    int sharedFovWidth = 640;  // new
    int sharedFovHeight = 640; // new
    std::chrono::high_resolution_clock::time_point captureTime; // true time 0
    bool newFrameReady = false;
    
    // Worker Function
    void WorkerLoop();

    float confThreshold = 0.5f;
    float nmsThreshold = 0.45f;
    int cpuThreads = std::thread::hardware_concurrency() > 0 ? std::thread::hardware_concurrency() : 8; // default max threads
    int aiResolution = 416; // input res (320 416 512 640)
    int targetAiFps = 0; // 0 is unlimited
    bool detectionEnabled = true;
    bool isMenuOpen = true; // menu starts open
    bool showFPS = false;           // show fps
    bool showLatency = false;       // show latency
    
    // Manual Check
    bool manualCheck = false; 
    int keyYes = 0; 
    int keyNo = 0;
    bool waitingForKeyYes = false;
    bool waitingForKeyNo = false;
    bool showControls = false; // master toggle manual controls
    
    // Performance
    bool showFps = true;
    bool fastCapture = true; // default on for igpu
    bool useGpu = false;
    // FOV Settings
    bool showFov = true;
    int fovWidth = 640;
    int fovHeight = 640;
    
    // customization
    float boxColor[4] = {0.0f, 1.0f, 0.0f, 1.0f}; // Green box
    float textColor[4] = {0.0f, 1.0f, 0.0f, 1.0f}; // Green text
    float boxThickness = 2.0f;
    bool showBox = true;       // new
    bool showName = true;      // new
    bool showConf = true;      // new
    
    // Model Management
    std::vector<std::string> modelList;
    std::vector<std::string> labelList; // new
    std::string currentModel = "Select Model...";
    std::string currentLabelFile = "None (Default)";
    std::string lastErrorMessage;
    std::string modelsPath = "C:/Users/Mathias/Desktop/AI/models"; // default path
    std::string labelsPath = "C:/Users/Mathias/Desktop/AI/models/labels"; // new
    
    // ESP32 Connection
    char esp32IP[64] = "192.168.4.1";  // default ip
    int esp32Port = 81;                 // default port
    int movementSpeed = 200;            // speed 0-255
    float movementDuration = 1.0f;      // duration sec
    char manualCommand[256] = "";       // manual buffer
    
    // Feature toggles
    bool showUltrasonicInGUI = false;   // show ultrasonic gui
    bool showUltrasonicOverlay = false; // show ultrasonic overlay
    
    ID3D11Texture2D* texture = nullptr;
    ID3D11ShaderResourceView* textureView = nullptr;
    int textureWidth = 0;
    int textureHeight = 0;
};
