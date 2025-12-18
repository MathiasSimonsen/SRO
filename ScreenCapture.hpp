#pragma once

#include <opencv2/opencv.hpp>
#include <windows.h>
#include <vector>

// handling screen capture class using gdi
class ScreenCapture {
public:
    ScreenCapture();
    ~ScreenCapture();

    // start resources here
    void Init(int width, int height);

    // take screenshot put in frame
    void Capture(cv::Mat& frame);
    
    // release stuff
    void Release();
    
    // perf control
    void SetFastMode(bool enabled) { fastMode = enabled; }
    void SetROI(int x, int y, int w, int h);

private:
    int screenWidth = 0;
    int screenHeight = 0;
    int targetWidth = 0;
    int targetHeight = 0;
    
    // roi
    int roiX = 0;
    int roiY = 0;
    int roiW = 0;
    int roiH = 0;
    bool useROI = false;

    bool fastMode = true; // default fast

    HDC hScreenDC = NULL;
    HDC hMemoryDC = NULL;
    HBITMAP hBitmap = NULL;
    HBITMAP hOldBitmap = NULL;
    
    // buffer for pixels bgra
    std::vector<unsigned char> pixelBuffer;
};
