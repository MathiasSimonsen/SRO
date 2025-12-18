#include "Prediction.hpp"

void Prediction::UpdateHistory(const std::vector<Detection>& currentDetections) {
    auto currTime = std::chrono::high_resolution_clock::now();
    
    if (firstRun) {
        prevDetections = currentDetections;
        prevTime = currTime;
        firstRun = false;
        return;
    }

    double dt = std::chrono::duration<double>(currTime - prevTime).count();
    // safety zero division
    if (dt < 0.0001) dt = 0.0001; 
    
    if (!enabled) {
        // fix dont clear history when disabled need smooth prevent blink
        // just dont apply prediction in predict func
        // keep processing detects for smooth track
    } 

    // copy so we can mod velocities
    std::vector<Detection> processed = currentDetections;

    // match and calc velocity
    static int nextTrackingId = 0;
    
    // 1. predict where old objects should be now
    for (auto& prev : prevDetections) {
        float px = prev.velocity.x * (float)dt;
        float py = prev.velocity.y * (float)dt;
        prev.box.x += (int)px;
        prev.box.y += (int)py;
    }

    // 2. match current to prev greedy with iou distance
    // we want best match not just first one
    std::vector<bool> matchedPrev(prevDetections.size(), false);
    std::vector<bool> matchedCurr(currentDetections.size(), false);
    
    std::vector<Detection> finalDetections;
    
    // simple greedy match iter current find closest prev
    // ideally iou or hungarian but distance center center fast
    
    struct Match {
        int curIdx;
        int prevIdx;
        float dist;
    };
    std::vector<Match> matches;
    
    for (size_t i = 0; i < currentDetections.size(); i++) {
        cv::Point2f cCenter(currentDetections[i].box.x + currentDetections[i].box.width/2.0f, 
                            currentDetections[i].box.y + currentDetections[i].box.height/2.0f);
                            
        for (size_t j = 0; j < prevDetections.size(); j++) {
            if (prevDetections[j].classId != currentDetections[i].classId) continue;
            
            cv::Point2f pCenter(prevDetections[j].box.x + prevDetections[j].box.width/2.0f, 
                                prevDetections[j].box.y + prevDetections[j].box.height/2.0f);
                                
            float dist = (float)cv::norm(cCenter - pCenter);
            
            // strict threshold 100px. if farther likely new object
            // fix increased to 250px catch fast moving objects drift
            if (dist < 250.0f) {
                matches.push_back({(int)i, (int)j, dist});
            }
        }
    }
    
    // sort matches by distance small first
    std::sort(matches.begin(), matches.end(), [](const Match& a, const Match& b){
        return a.dist < b.dist;
    });
    
    // assign matches
    for (const auto& m : matches) {
        if (matchedCurr[m.curIdx] || matchedPrev[m.prevIdx]) continue;
        
        matchedCurr[m.curIdx] = true;
        matchedPrev[m.prevIdx] = true;
        
        // this match
        Detection& cur = const_cast<Detection&>(currentDetections[m.curIdx]);
        const Detection& prev = prevDetections[m.prevIdx];
        
        // id inheritance
        if (prev.trackingId == -1) cur.trackingId = nextTrackingId++;
        else cur.trackingId = prev.trackingId;
        
        cur.persistenceFrames = 10; // reset persistence
        
        // velocity calc
        cv::Point2f curCenter(cur.box.x + cur.box.width/2.0f, cur.box.y + cur.box.height/2.0f);
        cv::Point2f prevCenter(prev.box.x + prev.box.width/2.0f, prev.box.y + prev.box.height/2.0f);
        
        cv::Point2f offset = curCenter - prevCenter; // note prevcenter is predicted pos step 1 
        // logic check step 1 moved prev by velocity so cur should be close to prev
        // velocity update based on cur oldprev 
        // but mod prev step 1 
        // lets rely on simple curpos prevposlastframe dt
        // need un predicted pos prev for velocity calc
        // revert predict for velocity calc
        float px = prev.velocity.x * (float)dt;
        float py = prev.velocity.y * (float)dt;
        cv::Point2f oldPrevCenter = prevCenter - cv::Point2f(px, py);
        
        cv::Point2f rawVelocity = (curCenter - oldPrevCenter) * (1.0f / (float)dt);

        // cap n damp
        float speed = (float)cv::norm(rawVelocity);
        if (speed > 2000.0f) rawVelocity *= (2000.0f / speed); // max cap
        
        float alpha = smoothingFactor;
        if (speed < 50.0f) { alpha = 0.2f; rawVelocity *= 0.5f; } // friction

        cur.velocity = rawVelocity * alpha + prev.velocity * (1.0f - alpha);
        
        // fix velocity hysteresis anti wobble
        float dot = rawVelocity.x * prev.velocity.x + rawVelocity.y * prev.velocity.y;
        if (dot < 0) rawVelocity = {0, 0}; 

        // fix anti drift reverted relaxed per user feedback
        // only hard stop if motionless prevent snap back lag feeling
        float instantSpeed = (float)cv::norm(rawVelocity);
        if (instantSpeed < 2.0f) { // tuned relaxed 10.0 -> 2.0
             cur.velocity = {0, 0};
        } else {
             cur.velocity = rawVelocity * alpha + prev.velocity * (1.0f - alpha);
        }

        // fix pos deadzone static objects
        // if object truly static very low velocity lock pos completely
        float currentSpeed = (float)cv::norm(cur.velocity);
        bool isStatic = (currentSpeed < 15.0f); // tuned increased thresh static detection
        
        if (isStatic) {
            // object static lock pos prevent jitter
            float dx = std::abs((float)cur.box.x - prev.smoothBox.x);
            float dy = std::abs((float)cur.box.y - prev.smoothBox.y);
            
            if (dx < 3.0f) cur.box.x = (int)prev.smoothBox.x; // lock x
            if (dy < 3.0f) cur.box.y = (int)prev.smoothBox.y; // lock y
        }

        // fix size deadzone lock tiny changes
        // tuned increased 5.0f request
        float dw = std::abs((float)cur.box.width - prev.smoothBox.width);
        float dh = std::abs((float)cur.box.height - prev.smoothBox.height);
        
        float targetW = (float)cur.box.width;
        float targetH = (float)cur.box.height;
        
        if (dw < 5.0f) targetW = prev.smoothBox.width; // lock
        if (dh < 5.0f) targetH = prev.smoothBox.height; // lock
        
        // smooth pos normal alpha
        cv::Rect2f targetP((float)cur.box.x, (float)cur.box.y, targetW, targetH);
        
        cur.smoothBox.x = prev.smoothBox.x * (1.0f - alpha) + targetP.x * alpha;
        cur.smoothBox.y = prev.smoothBox.y * (1.0f - alpha) + targetP.y * alpha;
        
        // smooth size heavy damp 0.05 but applied locked target
        // if locked target == prev so result == prev perfect lock
        float sizeAlpha = 0.05f;
        cur.smoothBox.width = prev.smoothBox.width * (1.0f - sizeAlpha) + targetW * sizeAlpha;
        cur.smoothBox.height = prev.smoothBox.height * (1.0f - sizeAlpha) + targetH * sizeAlpha;
        
        // write back int box
        cur.box.x = (int)cur.smoothBox.x;
        cur.box.y = (int)cur.smoothBox.y;
        cur.box.width = (int)cur.smoothBox.width;
        cur.box.height = (int)cur.smoothBox.height;
        
        finalDetections.push_back(cur);
    }
    
    // 3. handle unmatched current new objevts
    for (size_t i = 0; i < currentDetections.size(); i++) {
        if (!matchedCurr[i]) {
            Detection d = currentDetections[i];
            d.trackingId = nextTrackingId++;
            d.persistenceFrames = 10;
            d.velocity = {0,0};
            // init smoothbox
            d.smoothBox = cv::Rect2f((float)d.box.x, (float)d.box.y, (float)d.box.width, (float)d.box.height);
            finalDetections.push_back(d);
        }
    }
    
    // 4. handle unmatched prev coast persist
    // user request removed entirely prevent ghosts flying objects
    // if not matched gone immediately
    /*
    for (size_t i = 0; i < prevDetections.size(); i++) {
        if (!matchedPrev[i]) {
            Detection d = prevDetections[i];
            if (d.persistenceFrames > 0) {
                 // ... coast logic removed ...
            }
        }
    }
    */

    prevDetections = finalDetections;
    prevTime = currTime;
}

std::vector<Detection> Prediction::Predict(const std::vector<Detection>& detections, double latencySec) {
    if (!enabled) return detections; // fix return unmod if disabled
    if (latencySec > 0.25) latencySec = 0.25; // cap predict time

    std::vector<Detection> predicted = detections;
    for (auto& det : predicted) {
        // fix dont predict static objects velocity near zero
        float speed = std::sqrt(det.velocity.x * det.velocity.x + det.velocity.y * det.velocity.y);
        if (speed < 15.0f) { // tuned increased 5.0 to 15.0 aggressive static detect
            // object essentially static dont apply prediction avoid jitter
            continue;
        }
        
        float shiftX = det.velocity.x * (float)latencySec * amount;
        float shiftY = det.velocity.y * (float)latencySec * amount;

        // tuned increased cap 100 to 500 prevent falling behind fast objects
        const float MAX_SHIFT = 500.0f;
        if (shiftX > MAX_SHIFT) shiftX = MAX_SHIFT;
        if (shiftX < -MAX_SHIFT) shiftX = -MAX_SHIFT;
        if (shiftY > MAX_SHIFT) shiftY = MAX_SHIFT;
        if (shiftY < -MAX_SHIFT) shiftY = -MAX_SHIFT;

        det.box.x += (int)shiftX;
        det.box.y += (int)shiftY;
    }
    return predicted;
}
