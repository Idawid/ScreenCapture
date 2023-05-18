#include "TrayIcon.h"
#include "Resource.h"

#include <windows.h>
#include <gdiplus.h>
#include <iostream>
#pragma comment (lib, "Gdiplus.lib")

// Global variables
HWND g_hWnd;
ULONG_PTR g_gdiplusToken;
int g_startX = 0;
int g_startY = 0;
int g_endX = 0;
int g_endY = 0;
bool g_isMouseDown = false;
TrayIcon* g_trayIcon;
HHOOK g_hHook = NULL;
UINT g_hotkeyId = 1;
bool g_isWindowVisible = false;

// Hooks
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK KeyboardHookCallback(int nCode, WPARAM wParam, LPARAM lParam);

// Function to draw the window content
void DrawWindowContent(HDC hdc, HBITMAP hBitmap, int width, int height);
// Function to capture the screen content
HBITMAP CaptureScreen(int rectX, int rectY, int rectWidth, int rectHeight);
// Function that draws a rectangle
void DrawOverlay(Gdiplus::Graphics& graphics, Gdiplus::SolidBrush& overlayBrush, int width, int height);
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

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
{
    // Initialize GDI+
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    Gdiplus::GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, NULL);

    // Create the window class
    const wchar_t CLASS_NAME[] = L"ScreenCopyWindowClass";

    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;

    RegisterClass(&wc);

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

    // Create and add the tray icon
    g_trayIcon = new TrayIcon(g_hWnd, hInstance, WM_USER + 1, LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SCREENCAPTURE)));
    g_trayIcon->Add();

    // Register the global hotkey
    RegisterGlobalHotkey(g_hWnd);

    // Set the global keyboard hook
    g_hHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardHookCallback, hInstance, 0);

    // Hide unless keyboard shortcut is pressed
    ShowWindow(g_hWnd, SW_HIDE);

    // Message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

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

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        HBITMAP hBitmap = CaptureScreen(0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN));
        SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)hBitmap);

        break;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        HBITMAP hBitmap = (HBITMAP)GetWindowLongPtr(hWnd, GWLP_USERDATA);

        if (hBitmap)
        {
            RECT clientRect;
            GetClientRect(hWnd, &clientRect);

            int windowWidth = clientRect.right - clientRect.left;
            int windowHeight = clientRect.bottom - clientRect.top;

            HDC hMemDC = CreateCompatibleDC(hdc);
            HBITMAP hMemBitmap = CreateCompatibleBitmap(hdc, windowWidth, windowHeight);
            HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemDC, hMemBitmap);

            DrawWindowContent(hMemDC, hBitmap, windowWidth, windowHeight);

            Gdiplus::Graphics graphics(hMemDC);
            if (g_isMouseDown)
            {
                ExcludeNonCoveredRegion(graphics, g_startX, g_startY, g_endX, g_endY);
            }
            Gdiplus::SolidBrush overlayBrush(Gdiplus::Color(128, 0, 0, 0));
            DrawOverlay(graphics, overlayBrush, windowWidth, windowHeight);

            BitBlt(hdc, 0, 0, windowWidth, windowHeight, hMemDC, 0, 0, SRCCOPY);

            SelectObject(hMemDC, hOldBitmap);
            DeleteDC(hMemDC);
            DeleteObject(hMemBitmap);
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
        InvalidateRect(hWnd, NULL, FALSE);
        break;
    }

    case WM_MOUSEMOVE:
    {
        if (g_isMouseDown)
        {
            g_endX = LOWORD(lParam);
            g_endY = HIWORD(lParam);
            InvalidateRect(hWnd, NULL, FALSE);
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

            // Get the excluded region coordinates
            int rectX = min(g_startX, g_endX);
            int rectY = min(g_startY, g_endY);
            int rectWidth = abs(g_endX - g_startX);
            int rectHeight = abs(g_endY - g_startY);

            // Capture the image from the excluded region
            HBITMAP hBitmap = CaptureScreen(rectX, rectY, rectWidth, rectHeight);

            // Open the clipboard
            if (OpenClipboard(hWnd))
            {
                // Empty the clipboard
                EmptyClipboard();

                // Set the image data to the clipboard
                SetClipboardData(CF_BITMAP, hBitmap);

                // Close the clipboard
                CloseClipboard();
            }
            else
            {
                // Failed to open the clipboard
                MessageBox(hWnd, L"Failed to open the clipboard.", L"Error", MB_OK | MB_ICONERROR);
            }
        }
        break;
    }

    case WM_DESTROY:
    {
        // Clean up the captured bitmap handle
        HBITMAP hBitmap = (HBITMAP)GetWindowLongPtr(hWnd, GWLP_USERDATA);
        if (hBitmap)
            DeleteObject(hBitmap);

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
    }

    // Call the next hook in the hook chain
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

void DrawOverlay(Gdiplus::Graphics& graphics, Gdiplus::SolidBrush& overlayBrush, int width, int height)
{
    graphics.FillRectangle(&overlayBrush, 0, 0, width, height);
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

void DrawWindowContent(HDC hdc, HBITMAP hBitmap, int width, int height)
{
    HDC hMemDC = CreateCompatibleDC(hdc);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemDC, hBitmap);

    BitBlt(hdc, 0, 0, width, height, hMemDC, 0, 0, SRCCOPY);

    SelectObject(hMemDC, hOldBitmap);
    DeleteDC(hMemDC);
}

void ExcludeNonCoveredRegion(Gdiplus::Graphics& graphics, int startX, int startY, int endX, int endY)
{
    int rectX = min(startX, endX);
    int rectY = min(startY, endY);
    int rectWidth = std::abs(endX - startX);
    int rectHeight = std::abs(endY - startY);

    Gdiplus::Region nonCoveredRegion(Gdiplus::Rect(rectX, rectY, rectWidth, rectHeight));
    graphics.ExcludeClip(&nonCoveredRegion);
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