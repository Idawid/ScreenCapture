#pragma once

#include <Windows.h>
#include "OCRProcessor.h"

struct WindowData
{
    HBITMAP bitmap;
    HDC deviceContext;
    HBITMAP memoryBitmap;
    int windowWidth;
    int windowHeight;
    OCRProcessor* ocr;
    bool isWindowVisible;
};