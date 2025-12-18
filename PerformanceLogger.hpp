#pragma once

#include <string>
#include <vector>
#include <chrono>

class PerformanceLogger {
public:
    PerformanceLogger();
    ~PerformanceLogger();

    void StartSession();
    void StopAndExport(); // stop logging and export csv
    void RecordFrame(double inferenceMs, int detectionCount, float avgConfidence);
    
    bool IsLogging() const { return isLogging; }
    void SetLogging(bool logging);
    
    // get stats for display
    double GetAvgInferenceMs() const;
    double GetAvgFPS() const;
    int GetTotalDetections() const { return totalDetections; }
    int GetTotalFrames() const { return totalFrames; }
    
private:
    bool isLogging = false;
    
    std::chrono::high_resolution_clock::time_point sessionStart;
    std::chrono::high_resolution_clock::time_point lastFrameTime;
    
    // metrics
    std::vector<double> inferenceTimes;
    std::vector<double> frameTimes;
    std::vector<float> confidences;
    std::vector<int> detectionCounts; // per frame detect counts
    int totalDetections = 0;
    int totalFrames = 0;
    int framesWithDetections = 0;
    
    // for fps calc
    double GetSessionDuration() const;
    void ExportToCSV(const std::string& filename);
};
