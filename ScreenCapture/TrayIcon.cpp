// TrayIcon.cpp
#include "TrayIcon.h"

TrayIcon::TrayIcon(HWND hwnd, HINSTANCE hInstance, UINT uCallbackMessage, HICON hIcon)
    : hwnd_(hwnd), hInstance_(hInstance), uCallbackMessage_(uCallbackMessage), hIcon_(hIcon)
{
    ZeroMemory(&nid_, sizeof(nid_));
    nid_.cbSize = sizeof(nid_);
    nid_.hWnd = hwnd_;
    nid_.uID = 1;
    nid_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid_.uCallbackMessage = uCallbackMessage_;
    nid_.hIcon = hIcon_;
    wcscpy_s(nid_.szTip, L"Screen Capture");
}

TrayIcon::~TrayIcon()
{
    Remove();
}

bool TrayIcon::Add()
{
    return Shell_NotifyIconW(NIM_ADD, &nid_) != FALSE;
}

void TrayIcon::Remove()
{
    Shell_NotifyIconW(NIM_DELETE, &nid_);
}

void TrayIcon::ShowBalloonTip(const wchar_t* title, const wchar_t* message, DWORD iconType)
{
    ZeroMemory(&nid_, sizeof(nid_));
    nid_.cbSize = sizeof(nid_);
    nid_.hWnd = hwnd_;
    nid_.uID = 1;
    nid_.uFlags = NIF_INFO;
    nid_.dwInfoFlags = NIIF_INFO;
    wcscpy_s(nid_.szInfoTitle, title);
    wcscpy_s(nid_.szInfo, message);
    Shell_NotifyIconW(NIM_MODIFY, &nid_);
}