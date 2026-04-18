#pragma once

#include <windows.h>
#include <shellapi.h>

#define ID_TRAY_ICON 1001
#define WM_TRAY_ICON (WM_USER + 1)

#define IDM_TOGGLE_PROXY 2001
#define IDM_SETTINGS 2002
#define IDM_AUTOSTART 2003
#define IDM_EXIT 2004
#define IDM_OPEN_WEBUI 2005
#define IDM_RESTART_MIHOMO 2006
#define IDM_UPDATE_MIHOMO 2007
#define IDM_OPEN_CONFIG_DIR 2008
#define IDM_TOGGLE_TUN 2009
#define IDM_OPEN_KERNEL_DIR 2010
#define IDM_DOWNLOAD_LATEST_KERNEL 2011
#define IDM_KERNEL_BASE 2200
#define IDM_KERNEL_MAX 2299

class TrayIcon {
public:
    TrayIcon(HWND hwnd);
    ~TrayIcon();

    bool Add();
    bool Remove();
    bool Update(bool proxyEnabled);
    void SetTunEnabled(bool tunEnabled);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

private:
    HWND m_hwnd;
    NOTIFYICONDATAA m_nid;
    HICON m_iconEnabled;
    HICON m_iconDisabled;
    bool m_tunEnabled;

    bool LoadIcons();
    void ShowContextMenu();
};
