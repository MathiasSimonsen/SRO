#include "PerformanceLogger.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <ctime>
#include <iostream>
#ifdef _WIN32
    #include <Windows.h>
#else
    #include <sys/stat.h>
#endif

PerformanceLogger::PerformanceLogger() {
    sessionStart = std::chrono::high_resolution_clock::now();
    lastFrameTime = sessionStart;
}

PerformanceLogger::~PerformanceLogger() {
    // no auto export
}

void PerformanceLogger::SetLogging(bool logging) {
    if (logging && !isLogging) {
        // start logging
        StartSession();
    }
    isLogging = logging;
}

void PerformanceLogger::StartSession() {
    sessionStart = std::chrono::high_resolution_clock::now();
    lastFrameTime = sessionStart;
    inferenceTimes.clear();
    frameTimes.clear();
    confidences.clear();
    detectionCounts.clear();
    totalDetections = 0;
    totalFrames = 0;
    framesWithDetections = 0;
    isLogging = true;
}

void PerformanceLogger::StopAndExport() {
    if (!isLogging || totalFrames == 0) return;
    
    isLogging = false;
    
    // create logs dir if not exist
    std::string logsDir = "logs";
    #ifdef _WIN32
        CreateDirectoryA(logsDir.c_str(), NULL);
    #else
        mkdir(logsDir.c_str(), 0777);
    #endif
    
    // gen filename with timestamp
    auto now = std::time(nullptr);
    std::tm tm;
    localtime_s(&tm, &now);
    
    std::ostringstream filename;
    filename << logsDir << "/performance_log_" 
             << std::put_time(&tm, "%Y-%m-%d_%H-%M-%S") 
             << ".csv";
    
    ExportToCSV(filename.str());
}

void PerformanceLogger::RecordFrame(double inferenceMs, int detectionCount, float avgConfidence) {
    if (!isLogging) return;
    
    auto now = std::chrono::high_resolution_clock::now();
    double frameTime = std::chrono::duration<double, std::milli>(now - lastFrameTime).count();
    
    inferenceTimes.push_back(inferenceMs);
    frameTimes.push_back(frameTime);
    detectionCounts.push_back(detectionCount);
    if (avgConfidence > 0) {
        confidences.push_back(avgConfidence);
    }
    totalDetections += detectionCount;
    if (detectionCount > 0) {
        framesWithDetections++;
    }
    totalFrames++;
    
    lastFrameTime = now;
}

void PerformanceLogger::ExportToCSV(const std::string& filename) {
    if (inferenceTimes.empty()) return;
    
    std::ofstream file(filename);
    if (!file.is_open()) return;
    
    // calc stats
    double avgInference = std::accumulate(inferenceTimes.begin(), inferenceTimes.end(), 0.0) / inferenceTimes.size();
    double minInference = *std::min_element(inferenceTimes.begin(), inferenceTimes.end());
    double maxInference = *std::max_element(inferenceTimes.begin(), inferenceTimes.end());
    
    double avgFrameTime = std::accumulate(frameTimes.begin(), frameTimes.end(), 0.0) / frameTimes.size();
    double avgFPS = 1000.0 / avgFrameTime;
    double minFPS = 1000.0 / (*std::max_element(frameTimes.begin(), frameTimes.end()));
    double maxFPS = 1000.0 / (*std::min_element(frameTimes.begin(), frameTimes.end()));
    
    double avgConfidence = confidences.empty() ? 0.0 : 
        std::accumulate(confidences.begin(), confidences.end(), 0.0) / confidences.size();
    
    double sessionDuration = GetSessionDuration();
    double detectionRate = (double)framesWithDetections / totalFrames * 100.0;
    double avgDetectionsPerFrame = (double)totalDetections / totalFrames;
    
    // get session start time
    auto sessionStartTime = std::chrono::system_clock::now() - 
        std::chrono::duration_cast<std::chrono::system_clock::duration>(
            std::chrono::high_resolution_clock::now() - sessionStart);
    std::time_t startTime = std::chrono::system_clock::to_time_t(sessionStartTime);
    std::tm tm;
    localtime_s(&tm, &startTime);
    
    // write csv header more metrics
    // standard trick needed for excel separator
    file << "sep=;\n"; 
    file << "Session Start;Session Duration (sec);Total Frames;Frames w/ Detections;Detection Rate (%);"
         << "Avg AI Time (ms);Min AI Time (ms);Max AI Time (ms);"
         << "Avg FPS;Min FPS;Max FPS;"
         << "Total Detections;Avg Detections/Frame;Avg Confidence\n";
    
    // write data each value own cell semicolons
    file << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << ";"
         << std::fixed << std::setprecision(2)
         << sessionDuration << ";"
         << totalFrames << ";"
         << framesWithDetections << ";"
         << detectionRate << ";"
         << avgInference << ";"
         << minInference << ";"
         << maxInference << ";"
         << avgFPS << ";"
         << minFPS << ";"
         << maxFPS << ";"
         << totalDetections << ";"
         << avgDetectionsPerFrame << ";"
         << avgConfidence << "\n";
    
    file.close();
    std::cout << "Performance log exported to: " << filename << std::endl;
}

double PerformanceLogger::GetAvgInferenceMs() const {
    if (inferenceTimes.empty()) return 0.0;
    return std::accumulate(inferenceTimes.begin(), inferenceTimes.end(), 0.0) / inferenceTimes.size();
}

double PerformanceLogger::GetAvgFPS() const {
    if (frameTimes.empty()) return 0.0;
    double avgFrameTime = std::accumulate(frameTimes.begin(), frameTimes.end(), 0.0) / frameTimes.size();
    return 1000.0 / avgFrameTime;
}

double PerformanceLogger::GetSessionDuration() const {
    auto now = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double>(now - sessionStart).count();
}
