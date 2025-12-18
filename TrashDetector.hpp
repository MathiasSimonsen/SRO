#pragma once

#include <opencv2/opencv.hpp>
#include <onnxruntime_cxx_api.h>
#include <vector>
#include <string>
#include <optional>

// detection struct for data
struct Detection {
    cv::Rect box;
    float confidence;
    int classId;
    std::string label;
    
    // new sub pixel smooth
    cv::Rect2f smoothBox; 
    
    // prediction data
    cv::Point2f velocity = {0,0};
    int trackingId = -1;       // unique id for track
    int persistenceFrames = 0; // frames to keep alive lost
};

// handling loading of onnx model
class TrashDetector {
public:
    TrashDetector();
    
    // load new model from disk return true if success
    bool LoadModel(const std::string& modelPath, bool useCUDA = false, int numThreads = 4, std::string* errorMsg = nullptr);
    bool LoadLabels(const std::string& labelPath);
    bool IsLoaded() const { return session != nullptr; }
    
    // detect on image
    std::vector<Detection> Detect(const cv::Mat& frame, float confThreshold = 0.5f, float nmsThreshold = 0.45f);

    // new dynamic res for performance
    void SetInputResolution(int size) {
        inputWidth = size;
        inputHeight = size;
    }
    
    // check if model forces res
    bool IsFixedResolution() const { return fixedInputWidth > 0 && fixedInputHeight > 0; }
    int GetFixedResolution() const { return fixedInputWidth; }

private:
    // ort resources
    Ort::Env env;
    std::unique_ptr<Ort::Session> session;
    
    // dynamic io names
    std::vector<std::string> inputNodeNames;
    std::vector<std::string> outputNodeNames;
    std::vector<const char*> inputNodeNamesAllocated;
    std::vector<const char*> outputNodeNamesAllocated;

    int inputWidth = 640; // fix default 640 accuracy
    int inputHeight = 640;
    
    int fixedInputWidth = -1; // if > 0 overrides inputWidth
    int fixedInputHeight = -1;
    
    // custom labels
    std::vector<std::string> customLabels;

    // help functs
    std::string GetLabel(int classId);
    bool IsTrash(int classId);
};
