#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <string>

class SettingsDialog {
public:
    static bool Show(HWND hwnd, std::string& server, int& port, std::string& bypass);

private:
    static INT_PTR CALLBACK DialogProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    static BOOL OnInitDialog(HWND hwnd);
    static void OnOK(HWND hwnd);
    static void OnCancel(HWND hwnd);
    static void OnAddBypass(HWND hwnd);
    static void OnRemoveBypass(HWND hwnd);
    static void OnClearBypass(HWND hwnd);
};
