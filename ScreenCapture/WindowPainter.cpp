#include "WindowPainter.h"

void WindowPainter::drawContent()
{
    HDC memoryDeviceContext = CreateCompatibleDC(windowData->deviceContext);
    HBITMAP oldBitmap = (HBITMAP)SelectObject(memoryDeviceContext, windowData->bitmap);

    BitBlt(windowData->deviceContext, 0, 0, windowData->windowWidth, windowData->windowHeight, memoryDeviceContext, 0, 0, SRCCOPY);

    SelectObject(memoryDeviceContext, oldBitmap);
    DeleteDC(memoryDeviceContext);
}

void WindowPainter::drawOverlay()
{
    Gdiplus::Graphics graphics(windowData->deviceContext);
    selectedRect.Inflate(-borderWidth, -borderWidth);
    graphics.ExcludeClip(selectedRect);
    graphics.FillRectangle(&overlayBrush, 0, 0, windowData->windowWidth, windowData->windowHeight);
    graphics.DrawRectangle(&this->borderPen, selectedRect);
}

WindowPainter::WindowPainter(HWND windowHandle, Gdiplus::Color overlayColor, Gdiplus::Color borderColor, int borderWidth)
    :overlayBrush(overlayColor), borderPen(borderColor, borderWidth), borderWidth(borderWidth)
{
    selectedRect = { 0, 0, 0 ,0 };
    startingPoint = { 0, 0 };
    windowData = reinterpret_cast<WindowData*>(GetWindowLongPtr(windowHandle, 0));
}

void WindowPainter::handlePaint(HDC deviceContext)
{
    if (windowData->bitmap && windowData->deviceContext && windowData->memoryBitmap)
    {
        HBITMAP oldBitmap = (HBITMAP)SelectObject(windowData->deviceContext, windowData->memoryBitmap);

        drawContent();

        drawOverlay();

        BitBlt(deviceContext, 0, 0, windowData->windowWidth, windowData->windowHeight, windowData->deviceContext, 0, 0, SRCCOPY);
        SelectObject(windowData->deviceContext, oldBitmap);
    }
}

void WindowPainter::updateSelectedRect(int currentX, int currentY)
{
    int width = abs(currentX - startingPoint.X);
    int height = abs(currentY - startingPoint.Y);

    int left = (currentX - startingPoint.X) < 0 ? currentX : startingPoint.X;
    int top = (currentY - startingPoint.Y) < 0 ? currentY : startingPoint.Y;

    selectedRect = Gdiplus::Rect(left, top, width, height);
}

void WindowPainter::createSelectedRect(int newX, int newY)
{
    startingPoint.X = newX;
    startingPoint.Y = newY;
    selectedRect = Gdiplus::Rect(newX, newY, 0, 0);
}

const Gdiplus::Rect& WindowPainter::getRect() const
{
    return selectedRect;
}

int WindowPainter::getBorderWidth()
{
    return borderWidth;
}

