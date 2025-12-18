#include "ScreenCapture.hpp"
#include <iostream>

ScreenCapture::ScreenCapture() {
    // get screen dimensions from windows yes
    screenWidth = GetSystemMetrics(SM_CXSCREEN);
    screenHeight = GetSystemMetrics(SM_CYSCREEN);
}

ScreenCapture::~ScreenCapture() {
    Release();
}

void ScreenCapture::Init(int width, int height) {
    targetWidth = width > 0 ? width : screenWidth;
    targetHeight = height > 0 ? height : screenHeight;
    
    // setup handles for graphic
    hScreenDC = GetDC(NULL); // whole screen
    hMemoryDC = CreateCompatibleDC(hScreenDC);
    
    // we use full screen grab and resize later maybe
    // or use stretchblt directly here easier
    
    hBitmap = CreateCompatibleBitmap(hScreenDC, targetWidth, targetHeight);
    hOldBitmap = (HBITMAP)SelectObject(hMemoryDC, hBitmap);
}

void ScreenCapture::Release() {
    if (hOldBitmap) SelectObject(hMemoryDC, hOldBitmap);
    if (hBitmap) DeleteObject(hBitmap);
    if (hMemoryDC) DeleteDC(hMemoryDC);
    if (hScreenDC) ReleaseDC(NULL, hScreenDC);
    
    hBitmap = NULL;
    hMemoryDC = NULL;
    hScreenDC = NULL;
}

void ScreenCapture::SetROI(int x, int y, int w, int h) {
    roiX = x;
    roiY = y;
    roiW = w;
    roiH = h;
    useROI = true;
}

void ScreenCapture::Capture(cv::Mat& frame) {
    // 1. determine capture size
    int capW = useROI ? roiW : screenWidth;
    int capH = useROI ? roiH : screenHeight;
    int capX = useROI ? roiX : 0;
    int capY = useROI ? roiY : 0;
    
    // update screen metrics just in case
    int sW = GetSystemMetrics(SM_CXSCREEN);
    int sH = GetSystemMetrics(SM_CYSCREEN);
    screenWidth = sW;
    screenHeight = sH;

    // safety checks
    if (capW <= 0) capW = 1; 
    if (capH <= 0) capH = 1;
    
    // re make bitmap if size is not correct yes
    // if fov slider changed we need new bitmap
    // targetwidth is what we want output to be
    // so we assume init called with desired res
    
    // if bitmap no match desired res re init
    bool needReinit = (!hMemoryDC || hBitmap == NULL); // simple check
    
    // force 1:1 if target dim are 0 
    // user want optimization so keep targetwidth fixed
    
    if (needReinit) {
        if (hMemoryDC) Release();
        Init(targetWidth, targetHeight);
    }

    // Use HALFTONE for better quality or COLORONCOLOR for speed
    // use halftone for quality or coloroncolor speed
    if (fastMode) {
        SetStretchBltMode(hMemoryDC, COLORONCOLOR);
    } else {
        SetStretchBltMode(hMemoryDC, HALFTONE);
        POINT pt;
        SetBrushOrgEx(hMemoryDC, 0, 0, &pt);
    }

    // optimization capture and resize one step stretchblt
    // stretch source region to dest bitmap
    // if match its copy if different scale
    
    StretchBlt(hMemoryDC, 0, 0, targetWidth, targetHeight, 
               hScreenDC, capX, capY, capW, capH, SRCCOPY);
               
    // 2. get bits for opencv format
    BITMAPINFOHEADER bi;
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = targetWidth; // actual size
    bi.biHeight = -targetHeight; // negative top down
    bi.biPlanes = 1;
    bi.biBitCount = 32; // bgra
    bi.biCompression = BI_RGB;
    bi.biSizeImage = 0;
    bi.biXPelsPerMeter = 0;
    bi.biYPelsPerMeter = 0;
    bi.biClrUsed = 0;
    bi.biClrImportant = 0;

    // make cv mat wrapper
    frame.create(targetHeight, targetWidth, CV_8UC4);
    
    // copy direct to frame data
    GetDIBits(hMemoryDC, hBitmap, 0, targetHeight, frame.data, (BITMAPINFO*)&bi, DIB_RGB_COLORS);
    
    // opencv uses bgr but windows gdi gave bgra
    cv::cvtColor(frame, frame, cv::COLOR_BGRA2BGR);
}
