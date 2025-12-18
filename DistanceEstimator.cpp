#include "DistanceEstimator.hpp"
#include <cmath>
#include <algorithm>

int DistanceEstimator::FindClosestIndex(const std::vector<Detection>& detections, int screenW, int screenH) {
    if (!highlightClosest || detections.empty()) return -1;

    int closestIdx = -1;
    float bestScore = -100000.0f; 
    float minScore = 100000.0f;   

    float centerX = (float)screenW / 2.0f;
    float centerY = (float)screenH / 2.0f;

    for (size_t i = 0; i < detections.size(); i++) {
        float val = 0.0f;
        const auto& d = detections[i];

        if (priorityMode == 0) { // size height
            val = (float)d.box.height;
            if (val > bestScore) { bestScore = val; closestIdx = (int)i; }
        }
        else if (priorityMode == 1) { // vertical bottom y
            val = (float)(d.box.y + d.box.height);
            if (val > bestScore) { bestScore = val; closestIdx = (int)i; }
        }
        else if (priorityMode == 2) { // crosshair
            float bx = (float)d.box.x + d.box.width / 2.0f;
            float by = (float)d.box.y + d.box.height / 2.0f;
            // smaller dist better
            float dist = std::sqrt(std::pow(bx - centerX, 2) + std::pow(by - centerY, 2));
            if (dist < minScore) { minScore = dist; closestIdx = (int)i; }
        }
    }
    return closestIdx;
}

std::string DistanceEstimator::GetDistanceText(const Detection& det, float scaleY) const {
    if (!enabled) return "";
    
    // simple metric height in pixels
    float pixelHeight = (float)det.box.height * scaleY;
    if (pixelHeight > 1.0f) {
        float dist = scale / pixelHeight;
        return " [" + std::to_string((int)dist) + "m]";
    }
    return "";
}
