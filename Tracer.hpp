#pragma once

#include "../TrashDetector.hpp" // for detection struct
#include <vector>
#include <imgui.h>

// forward decl
class DistanceEstimator;

class Tracer {
public:
    bool enabled = false;
    bool advancedMode = false;

    // config for visuals
    float lineColor[4] = { 0.0f, 1.0f, 1.0f, 1.0f }; // cyan default
    float thickness = 2.0f;

    void Draw(ImDrawList* drawList, const std::vector<Detection>& detections, 
              int screenWidth, int screenHeight, 
              float fovScaleX, float fovScaleY, 
              float fovOffsetX, float fovOffsetY,
              const DistanceEstimator* distEst,
              int closestIndex); // index of highlighted object from distance estimator

private:
    const Detection* FindClosestToCenter(const std::vector<Detection>& detections, 
                                       float centerX, float centerY,
                                       float scaleX, float scaleY,
                                       float offX, float offY);
    
    // helper to draw sci fi brackets
    void DrawBracket(ImDrawList* dl, float x, float y, float w, float h, ImU32 col);
};
