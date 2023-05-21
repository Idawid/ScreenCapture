#pragma once

#include "WindowData.h"
#include <gdiplus.h>

class WindowPainter
{
private:
    WindowData* windowData;
    Gdiplus::Point startingPoint;  // Starting mouse position
    Gdiplus::Rect selectedRect;  // Rectangle based on mouse position
    int borderWidth;
    Gdiplus::Pen borderPen;
    Gdiplus::SolidBrush overlayBrush;

    void drawContent();
    void drawOverlay();
public:
    WindowPainter(HWND windowHandle, Gdiplus::Color overlayColor, Gdiplus::Color borderColor, int borderWidth);
    void handlePaint(HDC hdc);
    void updateSelectedRect(int currentX, int currentY);
    void createSelectedRect(int newX, int newY);
    const Gdiplus::Rect& getRect() const;
    int getBorderWidth();
};
