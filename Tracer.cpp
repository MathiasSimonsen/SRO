#include "Tracer.hpp"
#include "DistanceEstimator.hpp"
#include <cmath>
#include <string>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

void Tracer::Draw(ImDrawList* dl, const std::vector<Detection>& detections, 
                  int screenWidth, int screenHeight, 
                  float fovScaleX, float fovScaleY, 
                  float fovOffsetX, float fovOffsetY,
                  const DistanceEstimator* distEst,
                  int closestIndex) 
{
    if (!enabled || detections.empty()) return;

    float centerX = screenWidth / 2.0f;
    float centerY = screenHeight / 2.0f;

    // fix use highlighted object from distanceestimator instead closest center
    const Detection* target = nullptr;
    if (closestIndex >= 0 && closestIndex < (int)detections.size()) {
        target = &detections[closestIndex];
    } else if (!detections.empty()) {
        // fallback closest center no highlight
        target = FindClosestToCenter(detections, centerX, centerY, fovScaleX, fovScaleY, fovOffsetX, fovOffsetY);
    }
    
    if (target) {
        ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(lineColor[0], lineColor[1], lineColor[2], lineColor[3]));
        
        // calc screen coords target box
        float bx = target->box.x;
        float by = target->box.y;
        float bw = target->box.width;
        float bh = target->box.height;

        float tX = (bx * fovScaleX) + fovOffsetX;
        float tY = (by * fovScaleY) + fovOffsetY;
        float tW = bw * fovScaleX;
        float tH = bh * fovScaleY;
        
        float targetCenterX = tX + tW / 2.0f;
        float targetCenterY = tY + tH / 2.0f;

        // draw main tracer line
        dl->AddLine(ImVec2(centerX, centerY), ImVec2(targetCenterX, targetCenterY), col, thickness);

        if (advancedMode) {
            // extra info mode
            
            // center origin circle
            dl->AddCircle(ImVec2(centerX, centerY), 5.0f, col, 12, 1.0f);
            
            // target bracket corners only
            DrawBracket(dl, tX, tY, tW, tH, col);

            // dist and angle calculation
            float dx = targetCenterX - centerX;
            float dy = targetCenterY - centerY;
            float distPx = std::sqrt(dx*dx + dy*dy);
            float angle = std::atan2(dy, dx) * 180.0f / M_PI;

            // data text near target
            std::string distText;
            if (distEst && distEst->enabled) {
                // use real dist meters distanceestimator
                distText = distEst->GetDistanceText(*target, fovScaleY);
            } else {
                // fallback pixels
                distText = "DIST: " + std::to_string((int)distPx) + "px";
            }
            std::string angleText = "ANG: " + std::to_string((int)angle) + " deg";
            
            // draw small tag line off bracket
            dl->AddLine(ImVec2(tX + tW, tY), ImVec2(tX + tW + 20, tY - 20), col, 1.0f);
            dl->AddLine(ImVec2(tX + tW + 20, tY - 20), ImVec2(tX + tW + 80, tY - 20), col, 1.0f);
            
            dl->AddText(ImVec2(tX + tW + 25, tY - 35), col, distText.c_str());
            dl->AddText(ImVec2(tX + tW + 25, tY - 20), col, angleText.c_str());

            // connecting nodes on line
            // draw small circle 33% 66% way
            dl->AddCircleFilled(ImVec2(centerX + dx * 0.33f, centerY + dy * 0.33f), 3.0f, col);
            dl->AddCircleFilled(ImVec2(centerX + dx * 0.66f, centerY + dy * 0.66f), 3.0f, col);
        }
    }
}

const Detection* Tracer::FindClosestToCenter(const std::vector<Detection>& detections, 
                                           float centerX, float centerY,
                                           float scaleX, float scaleY,
                                           float offX, float offY) 
{
    const Detection* closest = nullptr;
    float minDistSq = 999999999.0f;

    for (const auto& det : detections) {
        // map to screen
        float tX = (det.box.x * scaleX) + offX;
        float tY = (det.box.y * scaleY) + offY;
        float tW = det.box.width * scaleX;
        float tH = det.box.height * scaleY;
        
        float tCX = tX + tW / 2.0f;
        float tCY = tY + tH / 2.0f;

        float dx = tCX - centerX;
        float dy = tCY - centerY;
        float distSq = dx*dx + dy*dy;

        if (distSq < minDistSq) {
            minDistSq = distSq;
            closest = &det;
        }
    }
    return closest;
}

void Tracer::DrawBracket(ImDrawList* dl, float x, float y, float w, float h, ImU32 col) {
    float len = w * 0.2f; // length of corner arms
    if (len > 20.0f) len = 20.0f; // cap length

    // top left
    dl->AddLine(ImVec2(x, y), ImVec2(x + len, y), col, 2.0f);
    dl->AddLine(ImVec2(x, y), ImVec2(x, y + len), col, 2.0f);

    // top right
    dl->AddLine(ImVec2(x + w, y), ImVec2(x + w - len, y), col, 2.0f);
    dl->AddLine(ImVec2(x + w, y), ImVec2(x + w, y + len), col, 2.0f);

    // bottom left
    dl->AddLine(ImVec2(x, y + h), ImVec2(x + len, y + h), col, 2.0f);
    dl->AddLine(ImVec2(x, y + h), ImVec2(x, y + h - len), col, 2.0f);

    // bottom right
    dl->AddLine(ImVec2(x + w, y + h), ImVec2(x + w - len, y + h), col, 2.0f);
    dl->AddLine(ImVec2(x + w, y + h), ImVec2(x + w, y + h - len), col, 2.0f);
}
