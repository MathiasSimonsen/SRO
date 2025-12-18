#pragma once
#include <opencv2/opencv.hpp>
#include <vector>
#include <chrono>
#include "../TrashDetector.hpp" // adjusted path if needed assume features subdir

class Prediction {
public:
    bool enabled = true;
    float amount = 1.0f;          // was predictionAmount
    float smoothingFactor = 0.6f; // was predictionSmoothing

    void UpdateHistory(const std::vector<Detection>& currentDetections);
    std::vector<Detection> Predict(const std::vector<Detection>& detections, double latencySec);
    std::vector<Detection> GetProcessed() { return prevDetections; } // added

private:
    std::vector<Detection> prevDetections;
    std::chrono::high_resolution_clock::time_point prevTime;
    bool firstRun = true;
};
