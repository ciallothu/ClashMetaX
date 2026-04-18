#include "proxy_manager.h"
#include "tray_icon.h"
#include "settings_dialog.h"
#include "http_server.h"
#include "mihomo_manager.h"
#include <string>
#include <vector>

TrayIcon* g_trayIcon = nullptr;
HTTPServer* g_httpServer = nullptr;
MihomoManager* g_mihomoManager = nullptr;
std::string g_proxyServer = "127.0.0.1";
int g_proxyPort = 7890;
std::string g_proxyBypass = "localhost;127.*;<local>";

bool IsRunningAsAdmin() {
    BOOL isAdmin = FALSE;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    PSID adminGroup = NULL;

    if (AllocateAndInitializeSid(&ntAuthority, 2,
                                 SECURITY_BUILTIN_DOMAIN_RID,
                                 DOMAIN_ALIAS_RID_ADMINS,
                                 0, 0, 0, 0, 0, 0,
                                 &adminGroup)) {
        CheckTokenMembership(NULL, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }

    return isAdmin == TRUE;
}

bool RelaunchAsAdmin() {
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);

    HINSTANCE result = ShellExecuteA(NULL, "runas", exePath, NULL, NULL, SW_SHOWNORMAL);
    return reinterpret_cast<INT_PTR>(result) > 32;
}

void UpdateTrayState() {
    if (!g_trayIcon) {
        return;
    }

    g_trayIcon->Update(ProxyManager::IsProxyEnabled());
    if (g_mihomoManager) {
        g_trayIcon->SetTunEnabled(g_mihomoManager->IsTunEnabled());
    }
}

bool SetAutoStart(bool enable) {
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_WRITE, &hKey) != ERROR_SUCCESS) {
        return false;
    }

    if (enable) {
        char exePath[MAX_PATH];
        GetModuleFileNameA(NULL, exePath, MAX_PATH);

        if (RegSetValueExA(hKey, "ClashMetaX", 0, REG_SZ, (const BYTE*)exePath, strlen(exePath) + 1) != ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return false;
        }
    } else {
        RegDeleteValueA(hKey, "ClashMetaX");
    }

    RegCloseKey(hKey);
    return true;
}

bool IsAutoStartEnabled() {
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return false;
    }

    char buffer[MAX_PATH];
    DWORD size = sizeof(buffer);
    DWORD type = REG_SZ;
    bool result = (RegQueryValueExA(hKey, "ClashMetaX", NULL, &type, (LPBYTE)buffer, &size) == ERROR_SUCCESS);

    RegCloseKey(hKey);
    return result;
}

void LoadConfig() {
    ProxyManager::GetProxyConfig(g_proxyServer, g_proxyPort, g_proxyBypass);

    if (g_proxyServer.empty()) {
        g_proxyServer = "127.0.0.1";
        g_proxyPort = 7890;
        g_proxyBypass = "localhost;127.*;<local>";
        ProxyManager::SetProxyConfig(g_proxyServer, g_proxyPort, g_proxyBypass);
    }
}

void ToggleProxy() {
    bool currentState = ProxyManager::IsProxyEnabled();
    bool newState = !currentState;

    if (ProxyManager::SetProxyEnabled(newState)) {
        if (g_trayIcon) {
            g_trayIcon->Update(newState);
        }
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        g_trayIcon = new TrayIcon(hwnd);
        if (!g_trayIcon->Add()) {
            MessageBoxA(hwnd, "无法创建托盘图标", "错误", MB_ICONERROR);
            PostQuitMessage(1);
        }

        UpdateTrayState();

        g_mihomoManager = new MihomoManager();
        if (!g_mihomoManager->Initialize()) {
            MessageBoxA(hwnd, "内核初始化失败：请检查网络后重试。", "ClashMetaX", MB_OK | MB_ICONWARNING);
        } else if (!g_mihomoManager->Start()) {
            MessageBoxA(hwnd, "Mihomo 启动失败", "ClashMetaX", MB_OK | MB_ICONWARNING);
        }
        UpdateTrayState();
        break;

    case WM_TRAY_ICON:
        if (g_trayIcon) {
            g_trayIcon->HandleMessage(message, wParam, lParam);
        }
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDM_TOGGLE_PROXY:
            ToggleProxy();
            break;

        case IDM_TOGGLE_TUN: {
            if (!g_mihomoManager) {
                MessageBoxA(hwnd, "Mihomo 管理器未初始化", "错误", MB_OK | MB_ICONERROR);
                break;
            }

            bool enableTun = !g_mihomoManager->IsTunEnabled();
            if (g_mihomoManager->SetTunEnabled(enableTun)) {
                UpdateTrayState();
                MessageBoxA(hwnd,
                    enableTun ? "TUN Mode 已开启，Mihomo 已重启" : "TUN Mode 已关闭，Mihomo 已重启",
                    "成功", MB_OK | MB_ICONINFORMATION);
            } else {
                MessageBoxA(hwnd,
                    enableTun ? "TUN Mode 开启失败" : "TUN Mode 关闭失败",
                    "错误", MB_OK | MB_ICONERROR);
            }
            break;
        }

        case IDM_SETTINGS: {
            if (SettingsDialog::Show(hwnd, g_proxyServer, g_proxyPort, g_proxyBypass)) {
                ProxyManager::SetProxyConfig(g_proxyServer, g_proxyPort, g_proxyBypass);
                MessageBoxA(hwnd, "配置已保存！", "成功", MB_OK | MB_ICONINFORMATION);
            }
            break;
        }

        case IDM_AUTOSTART: {
            bool currentState = IsAutoStartEnabled();
            bool newState = !currentState;
            if (SetAutoStart(newState)) {
                char currentExe[MAX_PATH];
                GetModuleFileNameA(NULL, currentExe, MAX_PATH);
                if (newState) {
                    char message[512];
                    sprintf_s(message, sizeof(message),
                        "开机自启动已启用\n\n启动路径: %s\n\n注意: 更新版本后会自动指向新版本",
                        currentExe);
                    MessageBoxA(hwnd, message, "ClashMetaX", MB_OK | MB_ICONINFORMATION);
                } else {
                    MessageBoxA(hwnd, "开机自启动已禁用", "ClashMetaX", MB_OK | MB_ICONINFORMATION);
                }
            } else {
                MessageBoxA(hwnd, "设置开机自启动失败", "错误", MB_OK | MB_ICONERROR);
            }
            break;
        }

        case IDM_OPEN_WEBUI: {
            if (!g_httpServer) {
                g_httpServer = new HTTPServer();
                const int WEBUI_PORT = 12345;

                if (g_httpServer->Start(WEBUI_PORT)) {
                    char url[64];
                    sprintf(url, "http://localhost:%d", WEBUI_PORT);
                    ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOW);
                } else {
                    MessageBoxA(hwnd,
                        "端口12345冲突，启动失败！\n\n"
                        "请检查端口占用或关闭占用该端口的程序。",
                        "WebUI 启动失败", MB_OK | MB_ICONERROR);
                    delete g_httpServer;
                    g_httpServer = nullptr;
                }
            } else {
                ShellExecuteA(NULL, "open", "http://localhost:12345", NULL, NULL, SW_SHOW);
            }
            break;
        }

        case IDM_RESTART_MIHOMO: {
            if (g_mihomoManager) {
                if (g_mihomoManager->Restart()) {
                    MessageBoxA(hwnd, "Mihomo 已重启", "成功", MB_OK | MB_ICONINFORMATION);
                } else {
                    MessageBoxA(hwnd, "Mihomo 重启失败", "错误", MB_OK | MB_ICONERROR);
                }
            }
            break;
        }

        case IDM_OPEN_CONFIG_DIR: {
            if (g_mihomoManager) {
                std::string configPath = g_mihomoManager->GetConfigPath();
                if (!configPath.empty()) {
                    ShellExecuteA(NULL, "open", "explorer.exe", ("/select," + configPath).c_str(), NULL, SW_SHOW);
                } else {
                    MessageBoxA(hwnd, "无法获取配置文件路径", "错误", MB_OK | MB_ICONERROR);
                }
            }
            break;
        }

        case IDM_OPEN_KERNEL_DIR: {
            if (g_mihomoManager) {
                std::string kernelDir = g_mihomoManager->GetKernelDirectory();
                ShellExecuteA(NULL, "open", "explorer.exe", kernelDir.c_str(), NULL, SW_SHOW);
            }
            break;
        }

        case IDM_DOWNLOAD_LATEST_KERNEL: {
            if (!g_mihomoManager) break;
            std::string error;
            if (g_mihomoManager->DownloadLatestKernel(&error)) {
                g_mihomoManager->Restart();
                MessageBoxA(hwnd, "最新稳定版内核下载并启用成功。", "ClashMetaX", MB_OK | MB_ICONINFORMATION);
            } else {
                MessageBoxA(hwnd, error.c_str(), "下载失败", MB_OK | MB_ICONERROR);
            }
            break;
        }

        case IDM_EXIT:
            DestroyWindow(hwnd);
            break;

        default:
            if (LOWORD(wParam) >= IDM_KERNEL_BASE && LOWORD(wParam) <= IDM_KERNEL_MAX && g_mihomoManager) {
                std::vector<KernelMetadata> kernels = g_mihomoManager->GetInstalledKernels();
                UINT index = LOWORD(wParam) - IDM_KERNEL_BASE;
                if (index < kernels.size()) {
                    std::string error;
                    if (g_mihomoManager->SwitchKernel(kernels[index].id, &error)) {
                        MessageBoxA(hwnd, "内核切换成功。", "ClashMetaX", MB_OK | MB_ICONINFORMATION);
                    } else {
                        MessageBoxA(hwnd, error.c_str(), "内核切换失败", MB_OK | MB_ICONERROR);
                    }
                }
            }
            break;
        }
        break;

    case WM_DESTROY:
        if (g_mihomoManager) {
            g_mihomoManager->Stop();
            delete g_mihomoManager;
            g_mihomoManager = nullptr;
        }
        if (g_trayIcon) {
            delete g_trayIcon;
            g_trayIcon = nullptr;
        }
        if (g_httpServer) {
            delete g_httpServer;
            g_httpServer = nullptr;
        }
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProcA(hwnd, message, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int) {
    if (!IsRunningAsAdmin()) {
        if (RelaunchAsAdmin()) {
            return 0;
        }

        MessageBoxA(NULL,
            "ClashMetaX 需要管理员权限启动，以支持 TUN Mode 和相关网络配置。",
            "需要管理员权限", MB_OK | MB_ICONWARNING);
        return 1;
    }

    const char* mutexName = "Global\\ClashMetaX_SingleInstance_Mutex";
    HANDLE hMutex = CreateMutexA(NULL, TRUE, mutexName);

    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        MessageBoxA(NULL, "ClashMetaX 已经在运行中", "提示", MB_OK | MB_ICONINFORMATION);
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }

    LoadConfig();

    WNDCLASSEXA wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszClassName = "ClashMetaXClass";

    if (!RegisterClassExA(&wc)) {
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            MessageBoxA(NULL, "窗口类注册失败", "错误", MB_ICONERROR);
            if (hMutex) CloseHandle(hMutex);
            return 1;
        }
    }

    HWND hwnd = CreateWindowExA(
        0,
        "ClashMetaXClass",
        "ClashMetaX",
        0,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd) {
        MessageBoxA(NULL, "窗口创建失败", "错误", MB_ICONERROR);
        if (hMutex) CloseHandle(hMutex);
        return 1;
    }

    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    if (hMutex) CloseHandle(hMutex);
    return (int)msg.wParam;
}
