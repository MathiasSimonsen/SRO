#pragma once
#include <vector>
#include <string>
#include "../TrashDetector.hpp" // adjusted include path

class DistanceEstimator {
public:
    bool enabled = false;
    float scale = 1000.0f;
    bool highlightClosest = false;
    float highlightColor[4] = {1.0f, 0.0f, 0.0f, 1.0f};
    int priorityMode = 1; // 0=size 1=globaly 2=center

    int FindClosestIndex(const std::vector<Detection>& detections, int screenW, int screenH);
    std::string GetDistanceText(const Detection& det, float scaleY) const;
};
