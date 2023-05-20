#pragma once

#include <Windows.h>
#include <leptonica/allheaders.h>
#include <tesseract/baseapi.h>
#include <stdexcept>
#include <string>

class OCRProcessor
{
public:
    OCRProcessor();
    ~OCRProcessor();

    std::string performOCR(HBITMAP hBitmap);
    PIX* ConvertHBITMAPToPIX(HBITMAP hBitmap);

private:
    tesseract::TessBaseAPI* api;
};
