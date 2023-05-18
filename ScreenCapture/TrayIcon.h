#pragma once
// TrayIcon.h

#include <Windows.h>

class TrayIcon {
public:
    TrayIcon(HWND hwnd, HINSTANCE hInstance, UINT uCallbackMessage, HICON hIcon);
    ~TrayIcon();

    bool Add();
    void Remove();
    void ShowBalloonTip(const wchar_t* title, const wchar_t* message, DWORD iconType);

private:
    HWND hwnd_;
    HINSTANCE hInstance_;
    UINT uCallbackMessage_;
    HICON hIcon_;
    NOTIFYICONDATAW nid_;
};
