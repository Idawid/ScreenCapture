// OCRProcessor.cpp
#include "OCRProcessor.h"

OCRProcessor::OCRProcessor() {
    api = new tesseract::TessBaseAPI();

    // Initialize tesseract-ocr with English
    if (api->Init(NULL, "eng")) {
        // Could not initialize API
        throw std::runtime_error("Failed to initialize API.");
    }
}

OCRProcessor::~OCRProcessor() {
    // Destroy used object and release memory
    api->End();
    delete api;
}

std::string OCRProcessor::performOCR(HBITMAP hBitmap) {
    std::string outText;

    // Set image data
    Pix* image = ConvertHBITMAPToPIX(hBitmap);
    api->SetImage(image);

    // Get OCR result
    outText = api->GetUTF8Text();

    pixDestroy(&image);

    return outText;
}

PIX* OCRProcessor::ConvertHBITMAPToPIX(HBITMAP hBitmap) {
    BITMAP bitmap;
    GetObject(hBitmap, sizeof(BITMAP), &bitmap);

    int nWidth = bitmap.bmWidth;
    int nHeight = bitmap.bmHeight;
    int nBitCount = bitmap.bmBitsPixel;

    BITMAPINFOHEADER bi;
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = nWidth;
    bi.biHeight = -nHeight;
    bi.biPlanes = 1;
    bi.biBitCount = nBitCount;
    bi.biCompression = BI_RGB;
    bi.biSizeImage = 0;
    bi.biXPelsPerMeter = 0;
    bi.biYPelsPerMeter = 0;
    bi.biClrUsed = 0;
    bi.biClrImportant = 0;

    HDC hdc = GetDC(NULL);
    BYTE* pBits;
    HBITMAP hDib = CreateDIBSection(hdc, (BITMAPINFO*)&bi, DIB_RGB_COLORS, (void**)&pBits, NULL, 0);
    ReleaseDC(NULL, hdc);

    HDC hdcMem = CreateCompatibleDC(NULL); //for dib
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hDib);
    HDC hdcSrc = CreateCompatibleDC(NULL); //for src bitmap
    SelectObject(hdcSrc, hBitmap);
    BitBlt(hdcMem, 0, 0, nWidth, nHeight, hdcSrc, 0, 0, SRCCOPY);
    SelectObject(hdcMem, hOldBitmap);


    // Convert the DIB to a PIX*
    PIX* pix = pixCreate(nWidth, nHeight, nBitCount);
    if (!pix) {
        DeleteDC(hdcMem);
        DeleteDC(hdcSrc);
        DeleteObject(hDib);
        return NULL;
    }

    for (int y = 0; y < nHeight; ++y) {
        for (int x = 0; x < nWidth; ++x) {
            COLORREF color = *(COLORREF*)(pBits + y * ((nWidth * nBitCount + 7) / 8) + x * (nBitCount / 8));

            // Fucking windows creates the DIB in BGR color format
            uint8_t red = GetBValue(color);
            uint8_t green = GetGValue(color);
            uint8_t blue = GetRValue(color);

            // Construct a new color in RGB order
            pixSetRGBPixel(pix, x, y, red, green, blue);
        }
    }

    DeleteDC(hdcMem);
    DeleteDC(hdcSrc);
    DeleteObject(hDib);
    return pix;
}
