#include "TrashDetector.hpp"
#include <iostream>
#include <algorithm>
#include <fstream>
#include <regex>
#include <opencv2/dnn.hpp>

TrashDetector::TrashDetector() 
    : env(ORT_LOGGING_LEVEL_WARNING, "TrashDetector"), session(nullptr) {
}

bool TrashDetector::LoadModel(const std::string& modelPath, bool useCUDA, int numThreads, std::string* errorMsg) {
    try {
        Ort::SessionOptions sessionOptions;
        sessionOptions.SetIntraOpNumThreads(numThreads);
        // resetting the dimensions because we are loading new model so we need to reset them
        fixedInputWidth = -1;
        fixedInputHeight = -1;
        
        // sequential execution is faster for batch 1 so we use it
        sessionOptions.SetInterOpNumThreads(1); 
        sessionOptions.SetExecutionMode(ExecutionMode::ORT_SEQUENTIAL);
        sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

        if (useCUDA) {
            // to use cuda we need libs but they are missing so we skip it for now
            // if we had dlls it would work but we dont
            // so we skip it to prevent linker errors yes
            
            // FIXME: dynamic lookup or static link required.
            // OrtSessionOptionsAppendExecutionProvider_CUDA(sessionOptions, 0); 
            std::cout << "bro gpu requested but support disabled in build rip" << std::endl;
            // wait acts i think we can enable this if we just copy the dlls right? or am i stupid
        }

#ifdef _WIN32
        std::wstring wModelPath(modelPath.begin(), modelPath.end());
        session = std::make_unique<Ort::Session>(env, wModelPath.c_str(), sessionOptions);
#else
        session = std::make_unique<Ort::Session>(env, modelPath.c_str(), sessionOptions);
#endif
        std::cout << "model loaded from path " << modelPath << std::endl;
        
        // --- dynamic input output res ---
        Ort::AllocatorWithDefaultOptions allocator;
        
        // reset lists
        inputNodeNames.clear();
        outputNodeNames.clear();
        inputNodeNamesAllocated.clear();
        outputNodeNamesAllocated.clear();

        size_t numInputNodes = session->GetInputCount();
        for (size_t i = 0; i < numInputNodes; i++) {
            auto inputName = session->GetInputNameAllocated(i, allocator);
            inputNodeNames.push_back(inputName.get());
        }
        
        size_t numOutputNodes = session->GetOutputCount();
        for (size_t i = 0; i < numOutputNodes; i++) {
            auto outputName = session->GetOutputNameAllocated(i, allocator);
            outputNodeNames.push_back(outputName.get());
        }

        // fix crash by detecting shape automatically
        try {
            auto typeInfo = session->GetInputTypeInfo(0);
            auto tensorInfo = typeInfo.GetTensorTypeAndShapeInfo();
            auto shape = tensorInfo.GetShape();
            
            // check if width height are fixed not -1
            // usually shape is 1 3 h w or -1 3 -1 -1
            if (shape.size() >= 4) {
                int64_t h = shape[2];
                int64_t w = shape[3];
                
                if (h > 0 && w > 0) {
                   // fixed res model force it
                   fixedInputWidth = (int)w;
                   fixedInputHeight = (int)h;
                   inputWidth = fixedInputWidth;   // force now
                   inputHeight = fixedInputHeight; 
                   std::cout << "model requires fixed res " << w << "x" << h << std::endl;
                } else {
                   fixedInputWidth = -1;
                   fixedInputHeight = -1;
                   std::cout << "model supports dynamic res" << std::endl;
                }
            }
        } catch (...) {
            std::cout << "could not determine shape using defaults" << std::endl;
        }
        
        // setup pointers for run
        for (const auto& name : inputNodeNames) inputNodeNamesAllocated.push_back(name.c_str());
        for (const auto& name : outputNodeNames) outputNodeNamesAllocated.push_back(name.c_str());
        
        // logs removed for performance
        // std::cout << "DEBUG: Found Input: " << (inputNodeNames.empty() ? "?" : inputNodeNames[0]) << std::endl;
        // std::cout << "DEBUG: Found Output: " << (outputNodeNames.empty() ? "?" : outputNodeNames[0]) << std::endl;

        return true;
    } catch (const Ort::Exception& e) {
        std::string err = e.what();
        std::cerr << "error loading model " << err << std::endl;
        if (errorMsg) *errorMsg = err;
        return false;
    }
}

bool TrashDetector::LoadLabels(const std::string& labelPath) {
    customLabels.clear();
    std::ifstream file(labelPath);
    if (!file.is_open()) return false;
    
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    
    // Simple regex to find "id": "name" pattern
    std::regex pattern(R"(\"(\d+)\"\s*:\s*\"([^\"]+)\")");
    std::smatch matches;
    
    std::string::const_iterator searchStart(content.cbegin());
    while (std::regex_search(searchStart, content.cend(), matches, pattern)) {
        int id = std::stoi(matches[1].str());
        std::string name = matches[2].str();
        
        if (id >= customLabels.size()) {
            customLabels.resize(id + 1);
        }
        customLabels[id] = name;
        
        searchStart = matches.suffix().first;
    }
    
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    
    // simple regex to find id name pattern
    std::regex pattern(R"(\"(\d+)\"\s*:\s*\"([^\"]+)\")");
    return !customLabels.empty();
}

std::string TrashDetector::GetLabel(int classId) {
    if (!customLabels.empty()) {
        if (classId >= 0 && classId < customLabels.size() && !customLabels[classId].empty()) {
            return customLabels[classId];
        }
        return "Class " + std::to_string(classId);
    }

    switch (classId) {
        case 39: return "Bottle";
        case 41: return "Cup";
        case 45: return "Bowl";
        case 44: return "Spoon";
        case 0:  return "Person (Debug)"; 
        default: return "Object " + std::to_string(classId);
    }
}

bool TrashDetector::IsTrash(int classId) {
    return true; 
}

std::vector<Detection> TrashDetector::Detect(const cv::Mat& rawFrame, float confThreshold, float nmsThreshold) {
    std::vector<Detection> detections;
    if (!session) return detections; 

    // 1. Preprocessing letterbox
    int originalW = rawFrame.cols;
    int originalH = rawFrame.rows;
    
    // 1. preprocessing (letterbox)
    // fix ensure we use fixed res if model demands it
    // user request removed enforcement user accepts crash risk for speed
    int useW = inputWidth; 
    int useH = inputHeight;
    // int useW = (fixedInputWidth > 0) ? fixedInputWidth : inputWidth;
    // int useH = (fixedInputHeight > 0) ? fixedInputHeight : inputHeight;
    
    // originalW/H already declared above
    
    float ratio = std::min((float)useW / originalW, (float)useH / originalH);
    int newW = (int)(originalW * ratio);
    int newH = (int)(originalH * ratio);
    
    cv::Mat resized;
    cv::resize(rawFrame, resized, cv::Size(newW, newH));
    
    cv::Mat frame(useH, useW, CV_8UC3, cv::Scalar(114, 114, 114));
    
    int padX = (useW - newW) / 2;
    int padY = (useH - newH) / 2;
    
    resized.copyTo(frame(cv::Rect(padX, padY, newW, newH)));
    
    // optimized use blobfromimage for fast preprocessing simd
    // resizes if needed but we already did letterbox
    // swaps bgr rgb swaprb true
    // normalizes 1/255
    // chw layout
    cv::Mat blob;
    // Note: We use 'frame' which is the letterboxed image. 
    // We already resized/padded it to useW/useH manually to keep aspect ratio.
    // So we pass 'false' for crop.
    cv::dnn::blobFromImage(frame, blob, 1.0/255.0, cv::Size(useW, useH), cv::Scalar(0,0,0), true, false);

    // Blob is 1x3xHxW contiguous float buffer
    size_t inputTensorSize = 1 * 3 * useH * useW;
    std::vector<int64_t> inputShape = {1, 3, useH, useW};

    auto memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    
    // note we use frame which is letterboxed
    // we already resized padded it to useW useH manually to keep aspect ratio
    // so we pass false for crop
    cv::dnn::blobFromImage(frame, blob, 1.0/255.0, cv::Size(useW, useH), cv::Scalar(0,0,0), true, false);

    // blob is 1x3xhxw contiguous float buffer
    Ort::Value inputTensor = Ort::Value::CreateTensor<float>(
        memoryInfo, blob.ptr<float>(), inputTensorSize, inputShape.data(), inputShape.size()
    );

    if (inputNodeNamesAllocated.empty() || outputNodeNamesAllocated.empty()) return detections;

    const char* const* inputNames = inputNodeNamesAllocated.data();
    const char* const* outputNames = outputNodeNamesAllocated.data();
    
    try {
        auto outputTensors = session->Run(Ort::RunOptions{nullptr}, inputNames, &inputTensor, 1, outputNames, 1);
        
        float* floatData = outputTensors.front().GetTensorMutableData<float>();
        auto typeInfo = outputTensors.front().GetTensorTypeAndShapeInfo();
        auto shape = typeInfo.GetShape();
        
        int channelsNum = 0;
        int anchorsNum = 0;
        
        if (shape.size() == 3) {
             channelsNum = (int)shape[1];
             anchorsNum = (int)shape[2];
        } else {
             std::cerr << "unexpected output shape size " << shape.size() << std::endl;
             return detections;
        }

        static bool shapeLogged = false;
        if (!shapeLogged) {
            // std::cout << "[DEBUG] Output Shape: [" << shape[0] << ", " << shape[1] << ", " << shape[2] << "]" << std::endl;
            shapeLogged = true;
        }

        int classesNum = channelsNum - 4;
        
        std::vector<int> classIds;
        std::vector<float> confidences;
        std::vector<cv::Rect> boxes;
        
        for (int i = 0; i < anchorsNum; i++) {
            float maxScore = -1.0f;
            int maxClassId = -1;
            
            for (int c = 0; c < classesNum; c++) {
                float score = floatData[(4 + c) * anchorsNum + i];
                if (score > maxScore) {
                    maxScore = score;
                    maxClassId = c;
                }
            }
            
            if (maxScore > confThreshold) {
                if (!IsTrash(maxClassId)) continue;
                
                float cx = floatData[0 * anchorsNum + i];
                float cy = floatData[1 * anchorsNum + i];
                float w = floatData[2 * anchorsNum + i];
                float h = floatData[3 * anchorsNum + i];
                
                bool isNormalized = (w < 1.0f && h < 1.0f && cx < 1.0f && cy < 1.0f);
                if (isNormalized) {
                    cx *= useW;
                    cy *= useH;
                    w *= useW;
                    h *= useH;
                }
                
                static bool rawLogged = false;
                if (!rawLogged) {
                    // std::cout << "[DEBUG] Raw Detection: " << cx << "," << cy << " " << w << "x" << h 
                    //           << " (Norm: " << (isNormalized ? "Yes" : "No") << ")" << std::endl;
                    rawLogged = true;
                }
                
                float x = cx - w / 2.0f;
                float y = cy - h / 2.0f;
                
                float x_original = (x - padX) / ratio;
                float y_original = (y - padY) / ratio;
                float w_original = w / ratio;
                float h_original = h / ratio;
                
                int left = std::max(0, std::min((int)x_original, originalW));
                int top = std::max(0, std::min((int)y_original, originalH));
                int width = std::min((int)w_original, originalW - left);
                int height = std::min((int)h_original, originalH - top);
                
                boxes.push_back(cv::Rect(left, top, width, height));
                confidences.push_back(maxScore);
                classIds.push_back(maxClassId);
            }
        }
        
        std::vector<int> indices;
        cv::dnn::NMSBoxes(boxes, confidences, confThreshold, nmsThreshold, indices);
        
        for (int idx : indices) {
            Detection det;
            det.box = boxes[idx];
            det.confidence = confidences[idx];
            det.classId = classIds[idx];
            det.label = GetLabel(det.classId);
            detections.push_back(det);
            
            /*
            std::cout << "[DETECT] Found: " << det.label 
                      << " (ID: " << det.classId << ") " 
                      << " | Conf: " << (int)(det.confidence * 100) << "%" 
                      << " | Box: [" << det.box.x << "," << det.box.y << "]" 
                      << std::endl;
            */
        }
    } catch (const Ort::Exception& e) {
        std::cerr << "runtime error during detect " << e.what() << std::endl;
    }
    
    return detections;
}
