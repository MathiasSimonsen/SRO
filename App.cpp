#include "App.hpp"
#include <iostream>
#include <filesystem>
#include <Windows.h>

// uhh short name lol
namespace fs = std::filesystem;

App::App() {
    if (!gui.Init("Trash Detection System - Screen Mode", 1280, 720)) {
        std::cerr << "bro gui failed wtf" << std::endl;
        exit(1);
    }
    
    // start screen grabber thing
    // tells it to output 640 so it fits ai
    // driver scales it down if big
    capturer.Init(640, 640);
    capturer.SetFastMode(fastCapture); // Apply default
    
    capturer.SetFastMode(fastCapture); // Apply default
    
    // default res is 640 dont touch
    aiResolution = 640;
    detector.SetInputResolution(aiResolution);
    
    // look for models i guess
    RefreshModelList();
    RefreshLabelList();
}

App::~App() {
    if (textureView) textureView->Release();
    if (texture) texture->Release();
}

void App::RefreshModelList() {
    modelList.clear();
    
    // where we look for stuff
    std::vector<std::string> searchPaths = {
        "C:/Users/Mathias/Desktop/AI/models", // absolute path needed
        "../models", // relative build
        "../../models", // relative idk
        "." // Current directory
    };
    
    for (const auto& path : searchPaths) {
        if (fs::exists(path) && fs::is_directory(path)) {
            for (const auto& entry : fs::directory_iterator(path)) {
                if (entry.path().extension() == ".onnx") {
                    // save path
                    modelList.push_back(entry.path().string());
                }
            }
        }
    }
    
    if (modelList.empty()) {
        std::cerr << "WARNING no models found check path" << std::endl;
    }
}

void App::RefreshLabelList() {
    labelList.clear();
    labelList.push_back("None (Default)"); // Default option
    
    // Search in labelsPath
    if (fs::exists(labelsPath) && fs::is_directory(labelsPath)) {
        for (const auto& entry : fs::directory_iterator(labelsPath)) {
            if (entry.path().extension() == ".json") {
                labelList.push_back(entry.path().string());
            }
        }
    }
}

void App::WorkerLoop() {
    int sW = GetSystemMetrics(SM_CXSCREEN);
    int sH = GetSystemMetrics(SM_CYSCREEN);
    
    // keep track of old stuff for velocity
    std::vector<Detection> prevDetections;
    auto prevTime = std::chrono::high_resolution_clock::now();

    while (!shouldStop) {
        auto startWork = std::chrono::high_resolution_clock::now();
        
        // 0. update fov just in case
        // hey mark if you read this why did we enable this by default?? it breaks on my laptop
        int cx = sW / 2;
        int cy = sH / 2;
        int x = cx - (fovWidth / 2);
        int y = cy - (fovHeight / 2);
        
        if (x < 0) x = 0; if (y < 0) y = 0;
        if (fovWidth > sW) fovWidth = sW;
        if (fovHeight > sH) fovHeight = sH;
        
        capturer.SetROI(x, y, fovWidth, fovHeight);
        
        // 1. capture takes time
        auto capTime = std::chrono::high_resolution_clock::now(); // start time
        cv::Mat frame;
        capturer.Capture(frame);
        
        // 2. Inference (Very Heavy)
        // 2. inference is heavy af
        std::vector<Detection> results;
        if (!frame.empty() && detectionEnabled && detector.IsLoaded()) {
            results = detector.Detect(frame, confThreshold, nmsThreshold);
            
            // --- prediction update ---
            prediction.UpdateHistory(results);
            results = prediction.GetProcessed(); 
        }
        
        // 3. update shared data
        {
            std::lock_guard<std::mutex> lock(dataMutex);
            sharedDetections = results;
            captureTime = capTime; // share true time
            
            if (!frame.empty()) {
                sharedWidth = frame.cols;
                sharedHeight = frame.rows; // cap res like 640
                // trashdetector downscales if needed
                
                sharedFovWidth = fovWidth;   // sync fov settings
                sharedFovHeight = fovHeight; // yeah
                
                if (isMenuOpen) {
                     frame.copyTo(sharedFrame);
                     newFrameReady = true;
                }
            }
        }
        
        auto endWork = std::chrono::high_resolution_clock::now();
        double workTimeMs = std::chrono::duration<double, std::milli>(endWork - startWork).count();

        if (targetAiFps > 0) {
            double targetFrameTime = 1000.0 / targetAiFps;
            double sleepTime = targetFrameTime - workTimeMs;
            if (sleepTime > 0) std::this_thread::sleep_for(std::chrono::milliseconds((int)sleepTime));
        } else {
             if (!detectionEnabled) std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

void App::Run() {
    // Start Worker
    shouldStop = false;
    workerThread = std::thread(&App::WorkerLoop, this);

    while (!gui.ShouldClose()) {
        gui.BeginFrame();
        
        if (gui.requestMenuToggle || ImGui::IsKeyPressed(ImGuiKey_Insert)) {
            isMenuOpen = !isMenuOpen;
            gui.SetClickThrough(!isMenuOpen);
            gui.requestMenuToggle = false; 
        }

        // --- RENDER THREAD ---
        std::vector<Detection> drawDetections;
        cv::Mat drawFrame;
        bool frameUpdated = false;
        int currentSetupW = 1920; 
        int currentSetupH = 1080;
        int currentFovW = 640;
        int currentFovH = 640;
        std::chrono::high_resolution_clock::time_point capTime;
        
        {
            std::lock_guard<std::mutex> lock(dataMutex);
            drawDetections = sharedDetections;
            currentSetupW = sharedWidth;
            currentSetupH = sharedHeight;
            // capture fov active during update
            // note sharedwidth comes from captured frame 640x640
            // but we need logic fov size for drawing box
            currentFovW = sharedFovWidth; 
            currentFovH = sharedFovHeight;
            
            capTime = captureTime;
            
            if (newFrameReady && isMenuOpen) { 
                sharedFrame.copyTo(drawFrame);
                newFrameReady = false; 
                frameUpdated = true;
            }
        }
        
        if (detectionEnabled) {
            ImDrawList* drawList = ImGui::GetForegroundDrawList();
            
            float sW = (float)GetSystemMetrics(SM_CXSCREEN);
            float sH = (float)GetSystemMetrics(SM_CYSCREEN);
            
            float fovX = (sW / 2.0f) - (currentFovW / 2.0f);
            float fovY = (sH / 2.0f) - (currentFovH / 2.0f);
            
            if (showFov) {
                drawList->AddRect(ImVec2(fovX, fovY), ImVec2(fovX + currentFovW, fovY + currentFovH), IM_COL32(255, 255, 255, 100));
            }

            float scaleX = 1.0f; 
            float scaleY = 1.0f;

            // Prediction Time Delta
            // pred time delta
            // Prediction Time Delta
            auto now = std::chrono::high_resolution_clock::now();
            double timeSinceDet = std::chrono::duration<double>(now - capTime).count();
            
            // Add perf logging after detection and ultrasonic overlay in render
            auto t2 = std::chrono::high_resolution_clock::now();
            double aiLatency = std::chrono::duration<double, std::milli>(t2 - capTime).count(); // Assuming t1 is capTime
            
            // record perf metrics
            if (perfLogger.IsLogging()) {
                float avgConf = 0.0f;
                if (!drawDetections.empty()) { // using drawDetections
                    for (const auto& det : drawDetections) {
                        avgConf += det.confidence;
                    }
                    avgConf /= drawDetections.size();
                }
                perfLogger.RecordFrame(aiLatency, (int)drawDetections.size(), avgConf);
            }
            
            // predict positions
            std::vector<Detection> finalDets = prediction.Predict(drawDetections, timeSinceDet);

            // distance find closest
            int closestIdx = distanceEst.FindClosestIndex(finalDets, currentSetupW, currentSetupH);

            for (size_t i = 0; i < finalDets.size(); i++) {
                const auto& det = finalDets[i];
                
                // box geo
                float bx = det.box.x;
                float by = det.box.y;
                float bw = (float)det.box.width;
                float bh = (float)det.box.height;

                // map to screen fix fov scale
                
                // det coords are in 640 ai space
                // map 0..640 to 0..fovwidth
                // then add offset
                
                // fix det coords always in capture space
                // trashdetector remaps back
                // map currentsetupw to currentfovw
                
                float aiW = (float)currentSetupW; 
                float aiH = (float)currentSetupH;
                

                
                // scale from ai 640 to screen fov
                float frameScaleX = (float)currentFovW / aiW;
                float frameScaleY = (float)currentFovH / aiH;
                
                float x1 = (bx * frameScaleX) + fovX;
                float y1 = (by * frameScaleY) + fovY;
                float x2 = ((bx + bw) * frameScaleX) + fovX;
                float y2 = ((by + bh) * frameScaleY) + fovY;

                // Color
                float* useCol = boxColor;
                if (distanceEst.highlightClosest && (int)i == closestIdx) {
                    useCol = distanceEst.highlightColor;
                }

                ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(useCol[0], useCol[1], useCol[2], useCol[3]));
                ImU32 textCol = ImGui::ColorConvertFloat4ToU32(ImVec4(textColor[0], textColor[1], textColor[2], textColor[3]));

                if (showBox) {
                    drawList->AddRect(ImVec2(x1, y1), ImVec2(x2, y2), col, 0.0f, 0, boxThickness);
                }
                
                if (showName || showConf || distanceEst.enabled) {
                    std::string label = "";
                    if (showName) label += det.label;
                    
                    if (distanceEst.enabled) {
                        label += distanceEst.GetDistanceText(det, frameScaleY); 
                    }
                    
                    if (showConf) {
                         label += " (" + std::to_string((int)(det.confidence*100)) + "%)";
                    }
                    
                    drawList->AddText(ImVec2(x1, y1 - 20), textCol, label.c_str());
                }
            }

            // draw tracer after boxes maybe?
            // actually after for hud feel
            
            // recalc scales for tracer
            float aiW = (float)currentSetupW; 
            float aiH = (float)currentSetupH;
            float frameScaleX = (float)currentFovW / aiW;
            float frameScaleY = (float)currentFovH / aiH;

            tracer.Draw(drawList, finalDets, 
                        GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
                        frameScaleX, frameScaleY, fovX, fovY, &distanceEst, closestIdx);
        }

        // 3. update preview only if menu open
        if (isMenuOpen && frameUpdated && !drawFrame.empty()) {
            // bake boxes into preview
            for (const auto& det : drawDetections) {
                cv::Scalar bgr(boxColor[2] * 255, boxColor[1] * 255, boxColor[0] * 255);
                cv::rectangle(drawFrame, det.box, bgr, (int)(std::max)(1.0f, boxThickness));
            }
            cv::cvtColor(drawFrame, drawFrame, cv::COLOR_BGR2RGBA);
            UpdateTexture(drawFrame);
        }

        // 4. gui n fps
        if (isMenuOpen) RenderGui();
        
        if (showFps) {
            ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;
            const float PAD = 10.0f;
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - PAD, viewport->WorkPos.y + PAD), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
            ImGui::SetNextWindowBgAlpha(0.35f); 
            if (ImGui::Begin("FPS_Overlay", &showFps, flags)) {
                ImGui::Text("Overlay FPS: %.1f", ImGui::GetIO().Framerate);
                ImGui::Text("AI Latency: ~250ms"); // hardcoded lol
            }
            ImGui::End();
        }
        
        gui.EndFrame();
    }
    
    // stop worker
    shouldStop = true;
    if (workerThread.joinable()) workerThread.join();
}

// rendergui is in App_Gui.cpp now

void App::UpdateTexture(const cv::Mat& mat) {
    if (texture == nullptr || textureWidth != mat.cols || textureHeight != mat.rows) {
        if (textureView) { textureView->Release(); textureView = nullptr; }
        if (texture) { texture->Release(); texture = nullptr; }
        
        textureWidth = mat.cols;
        textureHeight = mat.rows;

        D3D11_TEXTURE2D_DESC desc;
        ZeroMemory(&desc, sizeof(desc));
        desc.Width = textureWidth;
        desc.Height = textureHeight;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = 0;

        D3D11_SUBRESOURCE_DATA subResource;
        subResource.pSysMem = mat.data;
        subResource.SysMemPitch = desc.Width * 4;
        subResource.SysMemSlicePitch = 0;
        
        gui.GetDevice()->CreateTexture2D(&desc, &subResource, &texture);
        
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
        ZeroMemory(&srvDesc, sizeof(srvDesc));
        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = desc.MipLevels;
        
        gui.GetDevice()->CreateShaderResourceView(texture, &srvDesc, &textureView);
    } else {
        ID3D11DeviceContext* ctx = gui.GetDeviceContext();
        ctx->UpdateSubresource(texture, 0, NULL, mat.data, textureWidth * 4, 0);
    }
}
