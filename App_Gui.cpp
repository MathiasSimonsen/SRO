#include "App.hpp"
#include <imgui.h>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

void App::RenderGui() {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    ImGui::Begin("Trash Detection Hub", nullptr, window_flags);

    if (ImGui::BeginTabBar("MainTabs")) {
        
        // tab 1 settings thing 
        if (ImGui::BeginTabItem("Detection & Settings")) {
            ImGui::Columns(2, "layout");
            
            // side panel
            ImGui::Text("System Information");
            ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
            ImGui::Checkbox("Vis FPS (In-Game)", &showFps);
            
            if (ImGui::Checkbox("Fast Capture (Performance)", &fastCapture)) {
                capturer.SetFastMode(fastCapture);
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("off for quality on for fps");
            
            ImGui::Separator();
            ImGui::Text("Modeller (Models)");
            
            // model box thing
            if (ImGui::BeginCombo("Select Model", currentModel.c_str())) {
                for (const auto& modelPath : modelList) {
                    std::string filename = fs::path(modelPath).filename().string();
                    
                    bool isSelected = (currentModel == filename);
                    if (ImGui::Selectable(filename.c_str(), isSelected)) {
                        currentModel = filename;
                        lastErrorMessage = ""; 
                        std::string error; 
                        if (detector.LoadModel(modelPath, useGpu, cpuThreads, &error)) {
                            detectionEnabled = true;
                        } else {
                            currentModel += " (Error)";
                            lastErrorMessage = error;
                            detectionEnabled = false;
                        }
                    }
                    if (isSelected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            
            // label dropdown thing
            std::string labelComboPreview = fs::path(currentLabelFile).filename().string();
            if (currentLabelFile == "None (Default)") labelComboPreview = "None (Default)";

            if (ImGui::BeginCombo("Select Labels", labelComboPreview.c_str())) {
                for (const auto& lPath : labelList) {
                    std::string displayName = (lPath == "None (Default)") ? lPath : fs::path(lPath).filename().string();
                    bool isSelected = (currentLabelFile == lPath);
                    
                    if (ImGui::Selectable(displayName.c_str(), isSelected)) {
                        currentLabelFile = lPath;
                        if (currentLabelFile != "None (Default)") {
                            detector.LoadLabels(currentLabelFile);
                        } else {
                            detector.LoadLabels(""); 
                        }
                    }
                    if (isSelected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            
            if (!lastErrorMessage.empty()) {
                ImGui::TextColored(ImVec4(1, 0, 0, 1), "Error: %s", lastErrorMessage.c_str());
                ImGui::Separator();
            }
            
            if (ImGui::Button("Opdater Model Liste")) {
                RefreshModelList();
                RefreshLabelList();
            }
            
            ImGui::Separator();
            ImGui::Checkbox("Aktiver Detektion", &detectionEnabled);
            
            ImGui::Separator();
            ImGui::Separator();
            ImGui::Separator();
            ImGui::Text("Prediction (Latency Comp)");
            ImGui::Checkbox("Enable Prediction", &prediction.enabled);
            if (prediction.enabled) {
                ImGui::SliderFloat("Pred. Force", &prediction.amount, 0.0f, 2.0f, "x%.1f");
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("how strong prediction is");
                
                ImGui::SliderFloat("Smoothness", &prediction.smoothingFactor, 0.1f, 1.0f, "%.2f");
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("low smooth high jitter");
            }
            
            ImGui::Separator();
            ImGui::Text("Distance Estimation");
            ImGui::Checkbox("Show Distance", &distanceEst.enabled);
            if (distanceEst.enabled) {
                ImGui::SliderFloat("Dist. Scale", &distanceEst.scale, 100.0f, 5000.0f, "Cal: %.0f");
                ImGui::Checkbox("Highlight Closest", &distanceEst.highlightClosest);
                if (distanceEst.highlightClosest) {
                     ImGui::ColorEdit4("Highl. Color", distanceEst.highlightColor);
                     
                     const char* priors[] = { "Size (Largest)", "Ground (Lowest Y)", "Crosshair (Center)" };
                     if (ImGui::BeginCombo("Priority", priors[distanceEst.priorityMode])) {
                         for (int n = 0; n < 3; n++) {
                             bool is_selected = (distanceEst.priorityMode == n);
                             if (ImGui::Selectable(priors[n], is_selected))
                                 distanceEst.priorityMode = n;
                             if (is_selected) ImGui::SetItemDefaultFocus();
                         }
                         ImGui::EndCombo();
                     }
                     if (ImGui::IsItemHovered()) ImGui::SetTooltip("decide whats closest");
                }
            }

            ImGui::Separator();
            ImGui::Text("AI Performance Tweaks");
            
            // gpu toggle beta
            if (ImGui::Checkbox("Use GPU (Beta)", &useGpu)) {
                 std::string fullPath = "";
                 for (const auto& p : modelList) {
                     if (fs::path(p).filename().string() == currentModel) {
                         fullPath = p; break;
                     }
                 }
                 if (!fullPath.empty()) {
                     std::string error;
                     detector.LoadModel(fullPath, useGpu, cpuThreads, &error);
                 }
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("try cuda might work");

            int maxCores = (int)std::thread::hardware_concurrency();
            if (maxCores < 1) maxCores = 32; // fallback
            if (ImGui::SliderInt("CPU Threads", &cpuThreads, 1, maxCores)) {
                 // ...
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("more threads faster maybe 4-8");

            if (ImGui::Button("Apply Thread Changes")) {
                std::string fullPath = "";
                for (const auto& p : modelList) {
                     if (fs::path(p).filename().string() == currentModel) {
                         fullPath = p; break;
                     }
                }
                if (!fullPath.empty()) {
                    std::string error;
                    detector.LoadModel(fullPath, false, cpuThreads, &error);
                }
            }
            
            ImGui::SliderInt("Target AI FPS", &targetAiFps, 0, 60, "%d FPS");
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("0 is unlimited 5-10 saves cpu");
            
            // ai res
            const char* resOptions[] = { "320x320 (Fastest)", "352x352", "416x416 (Balanced)", "480x480", "512x512 (Good)", "608x608", "640x640 (High Res)" };
            const int resValues[] = { 320, 352, 416, 480, 512, 608, 640 };
            int currentResIdx = 2; // defaulted to 416
            
            // match res to index
            for (int i = 0; i < IM_ARRAYSIZE(resValues); i++) {
                if (aiResolution == resValues[i]) {
                    currentResIdx = i;
                    break;
                }
            }
            
            // locked 640x640 per request
            ImGui::TextDisabled("AI Resolution: 640x640 (Locked)");
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("locked for stability yes");
             ImGui::Checkbox("Show FPS", &showFPS);
             // removed latency cause user said it looks bad
             // ImGui::Checkbox("Show AI Latency", &showLatency);
             
             ImGui::Separator();
             ImGui::Text("Performance Analytics");
             
             if (!perfLogger.IsLogging()) {
                 if (ImGui::Button("Start Logging")) {
                     perfLogger.StartSession();
                 }
             } else {
                 if (ImGui::Button("Stop Logging")) {
                     perfLogger.StopAndExport();
                 }
                 ImGui::SameLine();
                 ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "RECORDING");
             }
             
             if (perfLogger.IsLogging() || perfLogger.GetTotalFrames() > 0) {
                 ImGui::Indent();
                 ImGui::Text("Frames: %d", perfLogger.GetTotalFrames());
                 ImGui::Text("Avg AI Time: %.2f ms", perfLogger.GetAvgInferenceMs());
                 ImGui::Text("Avg FPS: %.1f", perfLogger.GetAvgFPS());
                 ImGui::Text("Total Detections: %d", perfLogger.GetTotalDetections());
                 ImGui::Unindent();
             }
            ImGui::Checkbox("Show FOV Box", &showFov);
            ImGui::SliderInt("FOV Width (X)", &fovWidth, 100, GetSystemMetrics(SM_CXSCREEN));
            // yo why we using system metrics here ?? its slow
            ImGui::SliderInt("FOV Height (Y)", &fovHeight, 100, GetSystemMetrics(SM_CYSCREEN));

            ImGui::Separator();
            ImGui::SliderFloat("Genkendelse (Conf)", &confThreshold, 0.1f, 1.0f);
            ImGui::SliderFloat("Overlap (NMS)", &nmsThreshold, 0.1f, 1.0f);
            
            ImGui::Separator();
            ImGui::Text("Udseende (Style)");
            // because im white
            ImGui::ColorEdit4("Boks Farve", boxColor);
            ImGui::ColorEdit4("Tekst Farve", textColor);
            ImGui::SliderFloat("Tykkelse", &boxThickness, 1.0f, 10.0f);
            
            ImGui::Checkbox("Vis Boks", &showBox); ImGui::SameLine();
            ImGui::Checkbox("Vis Navn", &showName); ImGui::SameLine();
            ImGui::Checkbox("Vis %", &showConf);
            
            ImGui::NextColumn();
            // view port
            if (textureView) {
                float aspect = (float)textureWidth / (float)textureHeight;
                float regionWidth = ImGui::GetContentRegionAvail().x;
                float regionHeight = ImGui::GetContentRegionAvail().y;
                
                // aspect ratio keep
                float height = regionWidth / aspect;
                if (height > regionHeight) {
                    height = regionHeight;
                    float width = height * aspect;
                }
                ImGui::Image((void*)textureView, ImVec2(regionWidth, height));
            }

            ImGui::Columns(1);
            ImGui::EndTabItem();
        }

        // tab 2 controls
        if (ImGui::BeginTabItem("Controls")) {
             ImGui::Text("Manual Override Controls");
             ImGui::Checkbox("Show Manual Buttons", &showControls); // master toggle
             
             ImGui::Separator();
             ImGui::Text("Tracer Visuals");
             ImGui::Checkbox("Enable Tracer", &tracer.enabled);
             if (tracer.enabled) {
                 ImGui::Indent();
                 ImGui::Checkbox("Extra Info Mode", &tracer.advancedMode);
                 ImGui::ColorEdit4("Line Color", tracer.lineColor);
                 ImGui::Unindent();
             }
             ImGui::Separator();

             // 3x3 Grid of Arrow Buttons
             if (showControls) {
                 ImGui::Text("Movement Settings");
                 
                 // Speed slider
                 ImGui::Text("Speed:");
                 ImGui::SameLine();
                 ImGui::SetNextItemWidth(200);
                 ImGui::SliderInt("##Speed", &movementSpeed, 0, 255);
                 ImGui::SameLine();
                 ImGui::Text("%d", movementSpeed);
                 
                 // Duration slider
                 ImGui::Text("Duration:");
                 ImGui::SameLine();
                 ImGui::SetNextItemWidth(200);
                 ImGui::SliderFloat("##Duration", &movementDuration, 0.1f, 5.0f, "%.1f sec");
                 
                 ImGui::Separator();
                 ImGui::Text("Directional Controls");
                 
                 // Helper lambda to send movement command
                 auto sendMove = [this](const char* direction) {
                     if (esp32Client.IsConnected()) {
                         std::string cmd = std::string(direction) + ":" + 
                                         std::to_string(movementSpeed) + ":" + 
                                         std::to_string(movementDuration);
                         esp32Client.SendCommand(cmd);
                     }
                 };
                 
                 // Size of buttons
                 ImVec2 bSz(60, 60);
             
                 // Top Row
                 if (ImGui::Button("NW", bSz)) sendMove("NW"); ImGui::SameLine();
                 if (ImGui::Button("N", bSz))  sendMove("FORWARD"); ImGui::SameLine();
                 if (ImGui::Button("NE", bSz)) sendMove("NE");
                 
                 // Middle Row
                 if (ImGui::Button("W", bSz))  sendMove("MOVE_LEFT"); ImGui::SameLine();
                 if (ImGui::Button("STOP", bSz)) {
                     if (esp32Client.IsConnected()) esp32Client.SendCommand("STOP");
                 }
                 ImGui::SameLine();
                 if (ImGui::Button("E", bSz))  sendMove("MOVE_RIGHT");
             
                 // Bottom Row
                 if (ImGui::Button("SW", bSz)) sendMove("SW"); ImGui::SameLine();
                 if (ImGui::Button("S", bSz))  sendMove("BACKWARD"); ImGui::SameLine();
                 if (ImGui::Button("SE", bSz)) sendMove("SE");
             } // End showControls
             
             ImGui::Separator();
             
             // Manual Check UI
             ImGui::Checkbox("Manual Check", &manualCheck);
             if (manualCheck) {
                 ImGui::Indent();
                 
                 // YES Key
                 std::string yesLabel = (keyYes == 0) ? "None" : std::to_string(keyYes);
                 if (waitingForKeyYes) yesLabel = "...";
                 if (ImGui::Button(("Bind YES: " + yesLabel).c_str())) {
                     waitingForKeyYes = true;
                     waitingForKeyNo = false; 
                 }
                 
                 // NO Key
                 std::string noLabel = (keyNo == 0) ? "None" : std::to_string(keyNo);
                 if (waitingForKeyNo) noLabel = "...";
                 if (ImGui::Button(("Bind NO: "  + noLabel).c_str())) {
                     waitingForKeyNo = true;
                     waitingForKeyYes = false;
                 }
                 
                 // Key Listener (Simple)
                 if (waitingForKeyYes || waitingForKeyNo) {
                     for (int k = 0x08; k <= 0xFE; k++) {
                         // Check raw key state or ImGui key
                         if (GetKeyState(k) & 0x8000) { 
                             if (waitingForKeyYes) { keyYes = k; waitingForKeyYes = false; }
                             else if (waitingForKeyNo) { keyNo = k; waitingForKeyNo = false; }
                             break;
                         }
                     }
                      // Mouse Click Cancel?
                      if (ImGui::IsMouseClicked(0)) { waitingForKeyYes = false; waitingForKeyNo = false; }
                 }
                 
                 ImGui::Unindent();
             }
             
             ImGui::Separator();
             ImGui::Text("Status: %s", manualCheck ? "Manual Mode Active" : "Auto Mode");
             
             ImGui::Separator();
             ImGui::Text("ESP32 Device Connection");
             
             // IP Address input
             ImGui::Text("IP Address:");
             ImGui::SameLine();
             ImGui::SetNextItemWidth(150);
             ImGui::InputText("##ESP32IP", esp32IP, sizeof(esp32IP));
             
             // Port input
             ImGui::SameLine();
             ImGui::Text("Port:");
             ImGui::SameLine();
             ImGui::SetNextItemWidth(80);
             ImGui::InputInt("##ESP32Port", &esp32Port);
             
             if (ImGui::Button("Connect to Device")) {
                 // Build WebSocket URL
                 std::string wsUrl = "ws://" + std::string(esp32IP) + ":" + std::to_string(esp32Port) + "/";
                 esp32Client.Connect(wsUrl);
             }
             
             // Display connection status with color
             std::string status = esp32Client.GetStatus();
             ImVec4 statusColor;
             if (esp32Client.IsConnected()) {
                 statusColor = ImVec4(0.0f, 1.0f, 0.0f, 1.0f); // Green
             } else if (status.find("Error") != std::string::npos || status.find("failed") != std::string::npos) {
                 statusColor = ImVec4(1.0f, 0.0f, 0.0f, 1.0f); // Red
             } else {
                 statusColor = ImVec4(0.7f, 0.7f, 0.7f, 1.0f); // Gray
             }
             ImGui::TextColored(statusColor, "%s", status.c_str());
             
             // Display recent messages
             if (esp32Client.IsConnected()) {
                 ImGui::Separator();
                 ImGui::Text("Recent Messages:");
                 ImGui::BeginChild("ESP32Messages", ImVec2(0, 100), true);
                 
                 auto messages = esp32Client.GetRecentMessages(10);
                 for (const auto& msg : messages) {
                     ImGui::TextWrapped("%s", msg.c_str());
                 }
                 
                 // Auto-scroll to bottom
                 if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                     ImGui::SetScrollHereY(1.0f);
                 
                 ImGui::EndChild();
                 
                 // Manual command input
                 ImGui::Separator();
                 ImGui::Text("Manual Command:");
                 ImGui::SetNextItemWidth(300);
                 ImGui::InputText("##ManualCmd", manualCommand, sizeof(manualCommand));
                 ImGui::SameLine();
                 if (ImGui::Button("Send")) {
                     if (strlen(manualCommand) > 0) {
                         esp32Client.SendCommand(std::string(manualCommand));
                         manualCommand[0] = '\0'; // Clear input
                     }
                 }
                 ImGui::SameLine();
                 ImGui::TextDisabled("(e.g., FORWARD:200:2.5)");
                 
                 // Error display (red text)
                 std::string lastError = esp32Client.GetLastError();
                 if (!lastError.empty()) {
                     ImGui::Separator();
                     ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Error: %s", lastError.c_str());
                 }
                 
                 // Ultrasonic distance viewer
                 ImGui::Separator();
                 ImGui::Checkbox("Show Ultrasonic Distance", &showUltrasonicInGUI);
                 if (showUltrasonicInGUI) {
                     int distance = esp32Client.GetUltrasonicDistance();
                     ImGui::Text("Distance: %d cm", distance);
                 }
                 ImGui::Checkbox("Show Ultrasonic on Overlay", &showUltrasonicOverlay);
             }
             
             ImGui::EndTabItem();
        }
        
        ImGui::EndTabBar();
    } // end tab bar

    ImGui::End();
}
