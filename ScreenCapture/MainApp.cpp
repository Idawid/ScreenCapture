#include "TrayIcon.h"
#include "OCRProcessor.h"
#include "WindowData.h"
#include "Resource.h"

#include <windows.h>
#include <gdiplus.h>
#include <iostream>
#include <cstddef>
#include "WindowPainter.h"
#pragma comment (lib, "Gdiplus.lib")

// Global variables
bool g_isMouseDown = false;
WindowPainter* painter = nullptr;

// Hooks
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK KeyboardHookCallback(int nCode, WPARAM wParam, LPARAM lParam);

// Function to capture the screen content
HBITMAP CaptureScreen(int rectX, int rectY, int rectWidth, int rectHeight);
// Function to initialize WindowData and set it in the window's extra bytes
void InitializeWindowResources(HWND hWnd);
// Function to deallocate WindowData and associated resources
void DeallocateWindowResources(HWND hWnd);

std::string GetLastErrorString();

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    // Initialize GDI+
    ULONG_PTR g_gdiplusToken;
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, NULL);

    // Create the window class
    const wchar_t CLASS_NAME[] = L"ScreenCopyWindowClass";

    WNDCLASSEX wcex = { 0 };
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.lpszClassName = CLASS_NAME;
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.cbWndExtra = sizeof(WindowData);

    RegisterClassEx(&wcex);

    // Get the screen width and height
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    // Create the window
    HWND hWnd = CreateWindowEx(
        0,                              // Optional window styles
        CLASS_NAME,                     // Window class name
        L"Screen Copy Window",          // Window title
        WS_POPUP,                       // Window style - Fullscreen popup window
        0, 0, screenWidth, screenHeight,// Window position and dimensions
        NULL,                           // Parent window
        NULL,                           // Menu handle
        hInstance,                      // Instance handle
        NULL                            // Additional application data
    );

    if (hWnd == NULL)
        return 0;

    // Create and add the tray icon
    TrayIcon trayIcon(hWnd, hInstance, WM_USER + 1, LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SMALL)));
    trayIcon.Add();

    // Set the global keyboard hook
    HHOOK hHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardHookCallback, hInstance, 0);

    // Create the painting handler
    painter = new WindowPainter(hWnd, { 156, 0, 0, 0 }, { 240, 255, 255, 255 }, 2);

    // Message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Clean up TrayIcon
    trayIcon.Remove();
    // Clean up hook
    if (hHook != NULL)
    {
        UnhookWindowsHookEx(hHook);
        hHook = NULL;
    }
    // Clean up the painter
    if (painter != NULL) {
        delete painter;
    }
    // Clean up GDI+
    Gdiplus::GdiplusShutdown(g_gdiplusToken);

    return static_cast<int>(msg.wParam);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{

    switch (message)
    {
    case WM_CREATE:
    {
        InitializeWindowResources(hWnd);
        break;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        painter->handlePaint(hdc);

        EndPaint(hWnd, &ps);
        return 0;
    }

    case WM_LBUTTONDOWN:
    {
        painter->createSelectedRect(LOWORD(lParam), HIWORD(lParam));
        g_isMouseDown = true;
        InvalidateRect(hWnd, NULL, TRUE);
        break;
    }

    case WM_MOUSEMOVE:
    {
        if (g_isMouseDown)
        {
            RECT rect;
            // Invalidate previous selected rectangle
            Gdiplus::Rect selectedRect = painter->getRect();
            selectedRect.Inflate(2 * painter->getBorderWidth(), 2 * painter->getBorderWidth());     // We want to clear the border
            SetRect(&rect, selectedRect.X, selectedRect.Y, selectedRect.X + selectedRect.Width, selectedRect.Y + selectedRect.Height);
            InvalidateRect(hWnd, &rect, FALSE);

            // Set the new selected rectangle
            int newX = LOWORD(lParam);
            int newY = HIWORD(lParam);
            painter->updateSelectedRect(LOWORD(lParam), HIWORD(lParam));

            // Invalidate new selected rectangle
            selectedRect = painter->getRect();
            selectedRect.Inflate(2 * painter->getBorderWidth(), 2 * painter->getBorderWidth());
            SetRect(&rect, selectedRect.X, selectedRect.Y, selectedRect.X + selectedRect.Width, selectedRect.Y + selectedRect.Height);
            InvalidateRect(hWnd, &rect, FALSE);
        }
        break;
    }

    case WM_LBUTTONUP:
    {
        g_isMouseDown = false;
        InvalidateRect(hWnd, NULL, FALSE);
        WindowData* windowRes = reinterpret_cast<WindowData*>(GetWindowLongPtr(hWnd, 0));
        if (windowRes && windowRes->isWindowVisible) {
            ShowWindow(hWnd, SW_HIDE);
            windowRes->isWindowVisible = false;

            // Get the selected region coordinates
            Gdiplus::Rect selectedRectangle = painter->getRect();
            // Capture the image from the selected region
            HBITMAP hBitmap = CaptureScreen(selectedRectangle.X, selectedRectangle.Y, selectedRectangle.Width, selectedRectangle.Height);

            // Perform OCR on the bitmap
            WindowData* windowRes = reinterpret_cast<WindowData*>(GetWindowLongPtr(hWnd, 0));
            std::string ocrText = windowRes->ocr->performOCR(hBitmap);

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
        painter->createSelectedRect(0, 0);

        break;
    }

    case WM_SIZE:
    {
        WindowData* windowRes = reinterpret_cast<WindowData*>(GetWindowLongPtr(hWnd, 0));

        // Update window width and height.
        int windowWidth = LOWORD(lParam);  // width of client area
        int windowHeight = HIWORD(lParam); // height of client area
        windowRes->windowWidth = windowWidth;
        windowRes->windowHeight = windowHeight;
        break;
    }

    case WM_DESTROY:
    {
        // Clean up the resources
        DeallocateWindowResources(hWnd);

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
            HWND hWnd = FindWindow(L"ScreenCopyWindowClass", NULL);
            if (hWnd != NULL)
            {
                // Get the OCRProcessor object from the window's extra bytes
                WindowData* windowRes = reinterpret_cast<WindowData*>(GetWindowLongPtr(hWnd, 0));
                if (windowRes && !windowRes->isWindowVisible)
                {
                    painter->createSelectedRect(0, 0);
                    HBITMAP hBitmap = CaptureScreen(0, 0, windowRes->windowWidth, windowRes->windowHeight);
                    windowRes->bitmap = hBitmap;

                    ShowWindow(hWnd, SW_SHOW);
                    windowRes->isWindowVisible = true;
                }
                else if (windowRes)
                {
                    ShowWindow(hWnd, SW_HIDE);
                    windowRes->isWindowVisible = false;
                }
            }
            return 1;
        }
        else if (pKeyboardStruct->vkCode == VK_ESCAPE)
        {
            HWND hWnd = FindWindow(L"ScreenCopyWindowClass", NULL);
            if (hWnd != NULL)
            {
                WindowData* windowRes = reinterpret_cast<WindowData*>(GetWindowLongPtr(hWnd, 0));
                if (windowRes && windowRes->isWindowVisible)
                {
                    painter->createSelectedRect(0, 0);

                    ShowWindow(hWnd, SW_HIDE);
                    windowRes->isWindowVisible = false;
                }
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

// Function to initialize WindowData and set it in the window's extra bytes
void InitializeWindowResources(HWND hWnd)
{
    WindowData* windowRes = new WindowData();
    try {
        windowRes->ocr = new OCRProcessor();
    }
    catch (const std::exception& e) {
        delete windowRes;
        throw; // Rethrow the exception
    }

    int windowWidth = GetSystemMetrics(SM_CXSCREEN);  // width of client area
    int windowHeight = GetSystemMetrics(SM_CYSCREEN); // height of client area
    windowRes->windowWidth = windowWidth;
    windowRes->windowHeight = windowHeight;

    HDC hdc = GetDC(hWnd);

    windowRes->bitmap = CaptureScreen(0, 0, windowWidth, windowHeight);
    windowRes->deviceContext = CreateCompatibleDC(hdc);
    windowRes->memoryBitmap = CreateCompatibleBitmap(hdc, windowWidth, windowHeight);
    ReleaseDC(hWnd, hdc);

    SetWindowLongPtr(hWnd, 0, reinterpret_cast<LONG_PTR>(windowRes));
}

// Function to deallocate WindowData and associated resources
void DeallocateWindowResources(HWND hWnd)
{
    WindowData* windowRes = reinterpret_cast<WindowData*>(GetWindowLongPtr(hWnd, 0));
    if (windowRes)
    {
        delete windowRes->ocr;

        if (windowRes->bitmap)
            DeleteObject(windowRes->bitmap);
        if (windowRes->deviceContext)
            DeleteDC(windowRes->deviceContext);
        if (windowRes->memoryBitmap)
            DeleteObject(windowRes->memoryBitmap);

        delete windowRes;
    }
}
