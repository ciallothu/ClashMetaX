#include "tray_icon.h"
#include "resource.h"
#include "mihomo_manager.h"
#include <stdio.h>
#include <string>
#include <vector>

extern MihomoManager* g_mihomoManager;

TrayIcon::TrayIcon(HWND hwnd) : m_hwnd(hwnd), m_iconEnabled(NULL), m_iconDisabled(NULL), m_tunEnabled(false) {
    ZeroMemory(&m_nid, sizeof(m_nid));
    LoadIcons();
}

TrayIcon::~TrayIcon() {
    Remove();
    if (m_iconEnabled) DestroyIcon(m_iconEnabled);
    if (m_iconDisabled) DestroyIcon(m_iconDisabled);
}

bool TrayIcon::LoadIcons() {
    m_iconEnabled = LoadIconA(GetModuleHandle(NULL), MAKEINTRESOURCEA(IDI_PROXY_ON));
    m_iconDisabled = LoadIconA(GetModuleHandle(NULL), MAKEINTRESOURCEA(IDI_PROXY_OFF));

    if (m_iconEnabled == NULL) {
        printf("Warning: Failed to load IDI_PROXY_ON, using system icon\n");
        m_iconEnabled = LoadIcon(NULL, IDI_INFORMATION);
    }
    if (m_iconDisabled == NULL) {
        printf("Warning: Failed to load IDI_PROXY_OFF, using system icon\n");
        m_iconDisabled = LoadIcon(NULL, IDI_ERROR);
    }

    return (m_iconEnabled != NULL) && (m_iconDisabled != NULL);
}

bool TrayIcon::Add() {
    m_nid.cbSize = sizeof(NOTIFYICONDATAA);
    m_nid.hWnd = m_hwnd;
    m_nid.uID = ID_TRAY_ICON;
    m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    m_nid.uCallbackMessage = WM_TRAY_ICON;
    m_nid.hIcon = m_iconDisabled;
    strcpy_s(m_nid.szTip, "ClashMetaX - 已关闭");

    return Shell_NotifyIconA(NIM_ADD, &m_nid) != FALSE;
}

bool TrayIcon::Remove() {
    m_nid.uFlags = 0;
    return Shell_NotifyIconA(NIM_DELETE, &m_nid) != FALSE;
}

bool TrayIcon::Update(bool proxyEnabled) {
    m_nid.uFlags = NIF_ICON | NIF_TIP;
    m_nid.hIcon = proxyEnabled ? m_iconEnabled : m_iconDisabled;
    strcpy_s(m_nid.szTip, proxyEnabled ? "ClashMetaX - 代理已开启" : "ClashMetaX - 代理已关闭");

    return Shell_NotifyIconA(NIM_MODIFY, &m_nid) != FALSE;
}

void TrayIcon::SetTunEnabled(bool tunEnabled) {
    m_tunEnabled = tunEnabled;
}

void TrayIcon::ShowContextMenu() {
    POINT pt;
    GetCursorPos(&pt);

    HMENU hMenu = CreatePopupMenu();
    AppendMenuA(hMenu, MF_STRING, IDM_TOGGLE_PROXY, "切换代理");
    AppendMenuA(hMenu, MF_STRING | (m_tunEnabled ? MF_CHECKED : MF_UNCHECKED), IDM_TOGGLE_TUN, "TUN Mode");
    AppendMenuA(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(hMenu, MF_STRING, IDM_SETTINGS, "设置...");
    AppendMenuA(hMenu, MF_STRING, IDM_AUTOSTART, "开机自启动");
    AppendMenuA(hMenu, MF_SEPARATOR, 0, NULL);

    HMENU kernelMenu = CreatePopupMenu();
    std::string currentVersion = "未安装";
    if (g_mihomoManager) {
        KernelMetadata current = g_mihomoManager->GetCurrentKernel();
        if (!current.version.empty()) {
            currentVersion = current.version;
        }
        std::vector<KernelMetadata> kernels = g_mihomoManager->GetInstalledKernels();
        for (size_t i = 0; i < kernels.size() && i < (IDM_KERNEL_MAX - IDM_KERNEL_BASE + 1); ++i) {
            UINT flags = MF_STRING;
            if (kernels[i].id == current.id) {
                flags |= MF_CHECKED;
            }
            std::string item = kernels[i].version + " (" + kernels[i].id + ")";
            AppendMenuA(kernelMenu, flags, IDM_KERNEL_BASE + static_cast<UINT>(i), item.c_str());
        }
        if (kernels.empty()) {
            AppendMenuA(kernelMenu, MF_STRING | MF_GRAYED, IDM_KERNEL_BASE, "暂无已安装内核");
        }
    }

    std::string currentText = "当前内核: " + currentVersion;
    AppendMenuA(kernelMenu, MF_STRING | MF_GRAYED, 0, currentText.c_str());
    AppendMenuA(kernelMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(kernelMenu, MF_STRING, IDM_DOWNLOAD_LATEST_KERNEL, "下载最新稳定版内核");
    AppendMenuA(kernelMenu, MF_STRING, IDM_OPEN_KERNEL_DIR, "打开内核目录");
    AppendMenuA(hMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(kernelMenu), "内核");

    AppendMenuA(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(hMenu, MF_STRING, IDM_OPEN_WEBUI, "打开面板");
    AppendMenuA(hMenu, MF_STRING, IDM_RESTART_MIHOMO, "重启 Mihomo");
    AppendMenuA(hMenu, MF_STRING, IDM_OPEN_CONFIG_DIR, "打开配置文件");
    AppendMenuA(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuA(hMenu, MF_STRING, IDM_EXIT, "退出");

    SetForegroundWindow(m_hwnd);
    TrackPopupMenu(hMenu, TPM_BOTTOMALIGN | TPM_RIGHTALIGN, pt.x, pt.y, 0, m_hwnd, NULL);
    DestroyMenu(hMenu);
}

LRESULT TrayIcon::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_TRAY_ICON:
        if (lParam == WM_RBUTTONUP) {
            ShowContextMenu();
        } else if (lParam == WM_LBUTTONDOWN) {
            PostMessage(m_hwnd, WM_COMMAND, IDM_OPEN_WEBUI, 0);
        } else if (lParam == WM_LBUTTONDBLCLK) {
            PostMessage(m_hwnd, WM_COMMAND, IDM_TOGGLE_PROXY, 0);
        }
        break;

    case WM_COMMAND:
        PostMessage(m_hwnd, WM_COMMAND, wParam, lParam);
        break;
    }
    return 0;
}
