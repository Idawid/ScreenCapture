#include "TrayIcon.h"
#include "OCRProcessor.h"
#include "Resource.h"

#include <windows.h>
#include <gdiplus.h>
#include <iostream>
#include <cstddef>
#pragma comment (lib, "Gdiplus.lib")

// Global variables
HWND g_hWnd;
ULONG_PTR g_gdiplusToken;
OCRProcessor* ocr;
int g_startX = 0;
int g_startY = 0;
int g_endX = 0;
int g_endY = 0;
bool g_isMouseDown = false;
TrayIcon* g_trayIcon;
HHOOK g_hHook = NULL;
UINT g_hotkeyId = 1;
bool g_isWindowVisible = false;
int bW = 1;

// Hooks
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK KeyboardHookCallback(int nCode, WPARAM wParam, LPARAM lParam);

// Function to draw the window content
void DrawWindowContent(HDC hdc, HBITMAP hBitmap, int width, int height);
// Function to capture the screen content
HBITMAP CaptureScreen(int rectX, int rectY, int rectWidth, int rectHeight);
// Function that draws a rectangle
void DrawOverlay(Gdiplus::Graphics& graphics, Gdiplus::SolidBrush& overlayBrush, int startX, int startY, int width, int height);
// Function that draws border around a rectangle
void DrawBorder(Gdiplus::Graphics& graphics, Gdiplus::Pen& borderPen, int startX, int startY, int endX, int endY);
// Function that excludes a rectangle from graphics
void ExcludeNonCoveredRegion(Gdiplus::Graphics& graphics, int startX, int startY, int endX, int endY);
// Function that registers global hotkey
void RegisterGlobalHotkey(HWND hwnd);
// Function that unregisters global hotkey
void UnregisterGlobalHotkey(HWND hwnd);
// Function that registers low-level hotkey in case global hotkey init fails
void SetLowLevelKeyboardHook();
// Function that unregisters low-level hotkey
void UnsetLowLevelKeyboardHook();
// Function to copy the image to the clipboard
void CopyImageToClipboard(HWND hWnd, HBITMAP hBitmap);
//
std::string GetLastErrorString();

struct WindowResources
{
    HBITMAP hBitmap;
    HDC hMemDC;
    HBITMAP hMemBitmap;
    int wWidth;
    int wHeight;
};

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    // Initialize GDI+
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, NULL);

    // Create the window class
    const wchar_t CLASS_NAME[] = L"ScreenCopyWindowClass";

    WNDCLASSEX wcex = { 0 };
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.lpszClassName = CLASS_NAME;
    wcex.cbSize = sizeof(WNDCLASSEX);
    // We will Set and Get pointers in this order
    wcex.cbWndExtra = sizeof(WindowResources);

    RegisterClassEx(&wcex);

    // Get the screen width and height
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    // Create the window
    g_hWnd = CreateWindowEx(
        0,                              // Optional window styles
        CLASS_NAME,                     // Window class name
        L"Screen Copy Window",          // Window title
        WS_POPUP | WS_VISIBLE,          // Window style - Fullscreen popup window
        0, 0, screenWidth, screenHeight,// Window position and dimensions
        NULL,                           // Parent window
        NULL,                           // Menu handle
        hInstance,                      // Instance handle
        NULL                            // Additional application data
    );

    if (g_hWnd == NULL)
        return 0;

    // Show unless keyboard shortcut is pressed
    ShowWindow(g_hWnd, SW_HIDE);

    // Create and add the tray icon
    g_trayIcon = new TrayIcon(g_hWnd, hInstance, WM_USER + 1, LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SCREENCAPTURE)));
    g_trayIcon->Add();

    // Register the global hotkey
    RegisterGlobalHotkey(g_hWnd);

    // Set the global keyboard hook
    g_hHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardHookCallback, hInstance, 0);

    // Set up the OCR Processor
    try {
        ocr = new OCRProcessor();
    }
    catch (const std::exception& e) {
        return 0;
    }

    // Message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Clean up OCR Processor
    delete ocr;
    // Clean up TrayIcon
    g_trayIcon->Remove();
    delete g_trayIcon;
    // Clean up hook
    if (g_hHook != NULL)
    {
        UnhookWindowsHookEx(g_hHook);
        g_hHook = NULL;
    }
    // Clean up the hotkey
    UnregisterGlobalHotkey(g_hWnd);

    // Clean up GDI+
    Gdiplus::GdiplusShutdown(g_gdiplusToken);

    return static_cast<int>(msg.wParam);
}

LONG_PTR result;
std::string errorMessage;

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        int windowWidth = GetSystemMetrics(SM_CXSCREEN);  // width of client area
        int windowHeight = GetSystemMetrics(SM_CYSCREEN); // height of client area
        SetWindowLong(hWnd, offsetof(WindowResources, wWidth), windowWidth);
        SetWindowLong(hWnd, offsetof(WindowResources, wHeight), windowHeight);

        HBITMAP hBitmap = CaptureScreen(0, 0, windowWidth, windowHeight);
        SetWindowLongPtr(hWnd, offsetof(WindowResources, hBitmap), (LONG_PTR)hBitmap);

        // Create the compatible device context.
        HDC hdc = GetDC(hWnd);
        HDC hMemDC = CreateCompatibleDC(hdc);
        SetWindowLongPtr(hWnd, offsetof(WindowResources, hMemDC), (LONG_PTR)hMemDC);

        // Create the compatible bitmap.
        HBITMAP hMemBitmap = CreateCompatibleBitmap(hdc, windowWidth, windowHeight);
        SetWindowLongPtr(hWnd, offsetof(WindowResources, hMemBitmap), (LONG_PTR)hMemBitmap);

        ReleaseDC(hWnd, hdc);

        break;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        // Pointers instead of new objects 
        HBITMAP hBitmap = (HBITMAP)GetWindowLongPtr(hWnd, offsetof(WindowResources, hBitmap));
        HDC hMemDC = (HDC)GetWindowLongPtr(hWnd, offsetof(WindowResources, hMemDC));
        HBITMAP hMemBitmap = (HBITMAP)GetWindowLongPtr(hWnd, offsetof(WindowResources, hMemBitmap));
        
        if (hBitmap && hMemDC && hMemBitmap)
        {
            int windowWidth = GetWindowLong(hWnd, offsetof(WindowResources, wWidth));
            int windowHeight = GetWindowLong(hWnd, offsetof(WindowResources, wHeight));

            // Double buffering
            HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemDC, hMemBitmap);

            DrawWindowContent(hMemDC, hBitmap, windowWidth, windowHeight);

            Gdiplus::Graphics graphics(hMemDC);
            if (g_isMouseDown)
            {
                ExcludeNonCoveredRegion(graphics, g_startX, g_startY, g_endX, g_endY);
            }

            Gdiplus::SolidBrush overlayBrush(Gdiplus::Color(156, 0, 0, 0));
            DrawOverlay(graphics, overlayBrush, 0, 0, windowWidth, windowHeight);

            if (g_isMouseDown)
            {
                Gdiplus::Pen borderPen(Gdiplus::Color(240, 255, 255, 255));
                borderPen.SetWidth(bW);
                DrawBorder(graphics, borderPen, g_startX, g_startY, g_endX, g_endY);
            }

            BitBlt(hdc, 0, 0, windowWidth, windowHeight, hMemDC, 0, 0, SRCCOPY);

            SelectObject(hMemDC, hOldBitmap);
        }

        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_LBUTTONDOWN:
    {
        g_startX = LOWORD(lParam);
        g_startY = HIWORD(lParam);
        g_endX = g_startX;
        g_endY = g_startY;
        g_isMouseDown = true;
        InvalidateRect(hWnd, NULL, TRUE);
        break;
    }

    case WM_MOUSEMOVE:
    {
        if (g_isMouseDown)
        {
            // Whole screen, useful for debug:
            // g_endX = LOWORD(lParam);
            // g_endY = HIWORD(lParam);
            // InvalidateRect(hWnd, NULL, FALSE);
            // Or, without border:
            // SetRect(&rect, g_startX, g_startY, g_endX, g_endY);
            
            // Invalidate previous selected rectangle
            RECT rect;
            SetRect(&rect, min(g_startX, g_endX) - 2 * bW, min(g_startY, g_endY) - 2 * bW, max(g_endX, g_startX) + 2 * bW, max(g_endY, g_startY) + 2 * bW);
            InvalidateRect(hWnd, &rect, FALSE);

            // Invalidate new selected rectangle
            g_endX = LOWORD(lParam);
            g_endY = HIWORD(lParam);
            SetRect(&rect, min(g_startX, g_endX) - 2 * bW, min(g_startY, g_endY) - 2 * bW, max(g_endX, g_startX) + 2 * bW, max(g_endY, g_startY) + 2 * bW);
            InvalidateRect(hWnd, &rect, FALSE);
        }
        break;
    }

    case WM_LBUTTONUP:
    {
        g_isMouseDown = false;
        InvalidateRect(hWnd, NULL, FALSE);
        if (g_isWindowVisible == true) {
            ShowWindow(g_hWnd, SW_HIDE);
            g_isWindowVisible = false;

            // Get the selected region coordinates
            int rectX = min(g_startX, g_endX);
            int rectY = min(g_startY, g_endY);
            int rectWidth = abs(g_endX - g_startX);
            int rectHeight = abs(g_endY - g_startY);

            // Capture the image from the selected region
            HBITMAP hBitmap = CaptureScreen(rectX, rectY, rectWidth, rectHeight);

            // Perform OCR on the bitmap
            std::string ocrText = ocr->performOCR(hBitmap);

            // Open the clipboard
            if (OpenClipboard(hWnd))
            {
                // Empty the clipboard
                EmptyClipboard();

                // Set the OCR text to the clipboard
                const char* output = ocrText.c_str();
                size_t len = strlen(output) + 1;
                HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
                memcpy(GlobalLock(hMem), output, len);
                GlobalUnlock(hMem);
                SetClipboardData(CF_TEXT, hMem);

                // Close the clipboard
                CloseClipboard();
            }
            else
            {
                // Failed to open the clipboard
                MessageBox(hWnd, L"Failed to open the clipboard.", L"Error", MB_OK | MB_ICONERROR);
            }
        }
        g_startX = 0;
        g_startY = 0;
        g_endX = 0;
        g_endY = 0;

        break;
    }

    case WM_SIZE:
    {
        // Update window width and height.
        int windowWidth = LOWORD(lParam);  // width of client area
        int windowHeight = HIWORD(lParam); // height of client area
        SetWindowLong(hWnd, offsetof(WindowResources, wWidth), windowWidth);
        SetWindowLong(hWnd, offsetof(WindowResources, wHeight), windowHeight);

        break;
    }

    case WM_DESTROY:
    {
        // Clean up the resources
        HBITMAP hBitmap = (HBITMAP)GetWindowLongPtr(hWnd, offsetof(WindowResources, hBitmap));
        if (hBitmap)
        {
            DeleteObject(hBitmap);
        }
        HDC hMemDC = (HDC)GetWindowLongPtr(hWnd, offsetof(WindowResources, hMemDC));
        if (hMemDC)
        {
            DeleteDC(hMemDC);
        }
        HBITMAP hMemBitmap = (HBITMAP)GetWindowLongPtr(hWnd, offsetof(WindowResources, hMemBitmap));
        if (hMemBitmap)
        {
            DeleteObject(hMemBitmap);
        }

        PostQuitMessage(0);
        break;
    }

    case WM_USER + 1:
        switch (lParam)
        {
        case WM_RBUTTONUP:
        {
            // Show a context menu when right-clicking on the tray icon

            HMENU hPopupMenu = CreatePopupMenu();
            AppendMenu(hPopupMenu, MF_STRING, 1, L"Exit");

            POINT cursorPos;
            GetCursorPos(&cursorPos);
            SetForegroundWindow(hWnd);
            TrackPopupMenu(hPopupMenu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, cursorPos.x, cursorPos.y, 0, hWnd, NULL);
            PostMessage(hWnd, WM_NULL, 0, 0);

            DestroyMenu(hPopupMenu);
            break;
        }
        case WM_LBUTTONDBLCLK:
            // Handle left double-click on the tray icon (if needed)
            break;
        }

        return 0;

    case WM_COMMAND:
        // Handle menu commands
        switch (LOWORD(wParam))
        {
        case 1:
            // Exit menu item
            DestroyWindow(hWnd);
            break;
        }

        return 0;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }

    return 0;
}

LRESULT CALLBACK KeyboardHookCallback(int nCode, WPARAM wParam, LPARAM lParam)
{
    // Check if the hook can process the event
    if (nCode == HC_ACTION)
    {
        KBDLLHOOKSTRUCT* pKeyboardStruct = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);

        // Check for your desired keyboard shortcut
        if (pKeyboardStruct->vkCode == 'S' &&
            (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0 &&
            (GetAsyncKeyState(VK_LWIN) & 0x8000) != 0)
        {
            if (!g_isWindowVisible)
            {
                // Capture the screen
                int windowWidth = GetWindowLong(g_hWnd, offsetof(WindowResources, wWidth));
                int windowHeight = GetWindowLong(g_hWnd, offsetof(WindowResources, wHeight));
                HBITMAP hBitmap = CaptureScreen(0, 0, windowWidth, windowHeight);
                SetWindowLongPtr(g_hWnd, offsetof(WindowResources, hBitmap), (LONG_PTR)hBitmap);

                // Show window
                ShowWindow(g_hWnd, SW_SHOW);
                g_isWindowVisible = true;
            }
            else
            {
                ShowWindow(g_hWnd, SW_HIDE);
                g_isWindowVisible = false;
            }
            return 1;
        }
        else if (pKeyboardStruct->vkCode == VK_ESCAPE)
        {
            if (g_isWindowVisible)
            {
                ShowWindow(g_hWnd, SW_HIDE);
                g_isWindowVisible = false;
            }
            return 1;
        }
    }

    // Call the next hook in the hook chain
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

HBITMAP CaptureScreen(int rectX, int rectY, int rectWidth, int rectHeight)
{
    HDC hScreenDC = GetDC(NULL);
    HDC hMemDC = CreateCompatibleDC(hScreenDC);

    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, rectWidth, rectHeight);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemDC, hBitmap);

    BitBlt(hMemDC, 0, 0, rectWidth, rectHeight, hScreenDC, rectX, rectY, SRCCOPY);

    SelectObject(hMemDC, hOldBitmap);
    DeleteDC(hMemDC);
    ReleaseDC(NULL, hScreenDC);

    return hBitmap;
}

void DrawOverlay(Gdiplus::Graphics& graphics, Gdiplus::SolidBrush& overlayBrush, int startX, int startY, int width, int height)
{
    graphics.FillRectangle(&overlayBrush, 0, 0, width, height);
}

void DrawBorder(Gdiplus::Graphics& graphics, Gdiplus::Pen& borderPen, int startX, int startY, int endX, int endY) {
    int rectX = min(startX, endX) - bW;
    int rectY = min(startY, endY) - bW;
    int rectWidth = std::abs(endX - startX) + bW;
    int rectHeight = std::abs(endY - startY) + bW;

    graphics.DrawRectangle(&borderPen, rectX, rectY, rectWidth, rectHeight);

    //Gdiplus::Color color(255, 255, 255);
    //Gdiplus::Rect rect(rectX, rectY, rectWidth, rectHeight);

    //const int glowSize = 10;
    //for (int i = 0; i < glowSize; ++i)
    //{
    //    float opacity = 1.0f - static_cast<float>(i) / glowSize;
    //    Gdiplus::Color glowingColor(static_cast<BYTE>(255 * opacity), color.GetRed(), color.GetGreen(), color.GetBlue());
    //    //Gdiplus::Pen pen(glowingColor, i * 2.0f + 1.0f);
    //    Gdiplus::Pen pen(glowingColor, 1.0f);

    //    graphics.DrawRectangle(&pen, rect);
    //    rect.Inflate(-1, -1); // shrink the rectangle for the next iteration
    //}
}

void ExcludeNonCoveredRegion(Gdiplus::Graphics& graphics, int startX, int startY, int endX, int endY)
{
    int rectX = min(startX, endX);
    int rectY = min(startY, endY);
    int rectWidth = std::abs(endX - startX) - bW;
    int rectHeight = std::abs(endY - startY) - bW;

    Gdiplus::Region nonCoveredRegion(Gdiplus::Rect(rectX, rectY, rectWidth, rectHeight));
    graphics.ExcludeClip(&nonCoveredRegion);
}

void DrawWindowContent(HDC hdc, HBITMAP hBitmap, int width, int height)
{
    HDC hMemDC = CreateCompatibleDC(hdc);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemDC, hBitmap);

    BitBlt(hdc, 0, 0, width, height, hMemDC, 0, 0, SRCCOPY);

    SelectObject(hMemDC, hOldBitmap);
    DeleteDC(hMemDC);
}

void RegisterGlobalHotkey(HWND hwnd)
{
    // Register the hotkey
    if (!RegisterHotKey(hwnd, g_hotkeyId, MOD_CONTROL | MOD_WIN, 'S'))
    {
        //// Print error
        //DWORD error = GetLastError();
        //wchar_t errorMessage[256];

        //switch (error)
        //{
        //case ERROR_HOTKEY_ALREADY_REGISTERED:
        //    wcscpy_s(errorMessage, L"The hotkey is already registered by another application.");
        //    break;
        //case ERROR_INVALID_WINDOW_HANDLE:
        //    wcscpy_s(errorMessage, L"The window handle is invalid.");
        //    break;
        //case ERROR_INVALID_PARAMETER:
        //    wcscpy_s(errorMessage, L"The parameters passed to RegisterHotKey are invalid.");
        //    break;
        //default:
        //    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, error, 0, errorMessage, 256, NULL);
        //    break;
        //}

        // Set the low-level keyboard hook to intercept the key combination
        SetLowLevelKeyboardHook();
    }
}

void UnregisterGlobalHotkey(HWND hwnd)
{
    // Unregister the hotkey
    UnregisterHotKey(hwnd, g_hotkeyId);

    // Unset the low-level keyboard hook
    UnsetLowLevelKeyboardHook();
}

void SetLowLevelKeyboardHook()
{
    // Set the low-level keyboard hook
    g_hHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardHookCallback, NULL, 0);
}

void UnsetLowLevelKeyboardHook()
{
    // Unset the low-level keyboard hook
    UnhookWindowsHookEx(g_hHook);
}

void CopyImageToClipboard(HWND hWnd, HBITMAP hBitmap)
{
    if (OpenClipboard(hWnd))
    {
        EmptyClipboard();
        SetClipboardData(CF_BITMAP, hBitmap);
        CloseClipboard();
    }
    else
    {
        MessageBox(hWnd, L"Failed to open the clipboard.", L"Error", MB_OK | MB_ICONERROR);
    }
}

std::string GetLastErrorString()
{
    DWORD errorCode = GetLastError();

    LPWSTR messageBuffer = nullptr;
    DWORD messageLength = FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPWSTR)&messageBuffer,
        0,
        NULL);

    if (messageLength == 0) {
        // Failed to retrieve the error message
        return "Unknown error occurred.";
    }

    std::wstring wideErrorMessage(messageBuffer);
    std::string errorMessage(wideErrorMessage.begin(), wideErrorMessage.end());

    // Free the buffer allocated by FormatMessage
    LocalFree(messageBuffer);

    return errorMessage;
}