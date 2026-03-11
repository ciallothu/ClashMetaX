#include "proxy_manager.h"
#include "tray_icon.h"
#include "settings_dialog.h"
#include "http_server.h"
#include "mihomo_manager.h"
#include <string>

// 全局变量
TrayIcon* g_trayIcon = nullptr;
HTTPServer* g_httpServer = nullptr;
MihomoManager* g_mihomoManager = nullptr;
std::string g_proxyServer = "127.0.0.1";
int g_proxyPort = 7890;
std::string g_proxyBypass = "localhost;127.*;<local>";

// 设置开机自启动
bool SetAutoStart(bool enable) {
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_WRITE, &hKey) != ERROR_SUCCESS) {
        return false;
    }

    if (enable) {
        // 获取当前正在运行的 exe 路径（而不是固定路径）
        char exePath[MAX_PATH];
        GetModuleFileNameA(NULL, exePath, MAX_PATH);

        // 每次都写入当前路径，这样版本更新后会自动指向新版本
        if (RegSetValueExA(hKey, "SysProxyBar", 0, REG_SZ, (const BYTE*)exePath, strlen(exePath) + 1) != ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return false;
        }
    } else {
        RegDeleteValueA(hKey, "SysProxyBar");
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
    bool result = (RegQueryValueExA(hKey, "SysProxyBar", NULL, &type, (LPBYTE)buffer, &size) == ERROR_SUCCESS);

    RegCloseKey(hKey);
    return result;
}

// 加载配置
void LoadConfig() {
    ProxyManager::GetProxyConfig(g_proxyServer, g_proxyPort, g_proxyBypass);

    // 如果没有配置，使用默认值
    if (g_proxyServer.empty()) {
        g_proxyServer = "127.0.0.1";
        g_proxyPort = 7890;
        g_proxyBypass = "localhost;127.*;<local>";
        ProxyManager::SetProxyConfig(g_proxyServer, g_proxyPort, g_proxyBypass);
    }
}

// 切换代理状态
void ToggleProxy() {
    bool currentState = ProxyManager::IsProxyEnabled();
    bool newState = !currentState;

    if (ProxyManager::SetProxyEnabled(newState)) {
        if (g_trayIcon) {
            g_trayIcon->Update(newState);
        }
    }
}

// 窗口过程
LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_CREATE:
        g_trayIcon = new TrayIcon(hwnd);
        if (!g_trayIcon->Add()) {
            MessageBoxA(hwnd, "无法创建托盘图标", "错误", MB_ICONERROR);
            PostQuitMessage(1);
        }

        // 初始化图标状态
        g_trayIcon->Update(ProxyManager::IsProxyEnabled());

        // 初始化并启动 mihomo
        g_mihomoManager = new MihomoManager();
        if (!g_mihomoManager->Initialize()) {
            MessageBoxA(hwnd, "Mihomo 初始化失败", "警告", MB_OK | MB_ICONWARNING);
        } else if (!g_mihomoManager->Start()) {
            MessageBoxA(hwnd, "Mihomo 启动失败", "警告", MB_OK | MB_ICONWARNING);
        }
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

        case IDM_SETTINGS: {
            if (SettingsDialog::Show(hwnd, g_proxyServer, g_proxyPort, g_proxyBypass)) {
                // 用户点击了OK，保存配置
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
                    MessageBoxA(hwnd, message, "系统代理", MB_OK | MB_ICONINFORMATION);
                } else {
                    MessageBoxA(hwnd, "开机自启动已禁用", "系统代理", MB_OK | MB_ICONINFORMATION);
                }
            } else {
                MessageBoxA(hwnd, "设置开机自启动失败", "错误", MB_OK | MB_ICONERROR);
            }
            break;
        }

        case IDM_OPEN_WEBUI: {
            // 启动 HTTP 服务器（如果还没启动）
            if (!g_httpServer) {
                g_httpServer = new HTTPServer();

                // 固定使用端口 12345
                const int WEBUI_PORT = 12345;

                if (g_httpServer->Start(WEBUI_PORT)) {
                    // 服务器启动成功，在浏览器中打开
                    char url[64];
                    sprintf(url, "http://localhost:%d", WEBUI_PORT);
                    ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOW);
                } else {
                    // 端口冲突，启动失败
                    MessageBoxA(hwnd,
                        "端口12345冲突，启动失败！\n\n"
                        "可能原因：\n"
                        "1. 端口 12345 被其他程序占用\n"
                        "2. 防火墙阻止了网络访问\n\n"
                        "请检查端口占用或关闭占用该端口的程序。",
                        "WebUI 启动失败", MB_OK | MB_ICONERROR);
                    delete g_httpServer;
                    g_httpServer = nullptr;
                }
            } else {
                // 服务器已在运行，直接打开浏览器
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
                    // Select the config file in explorer
                    ShellExecuteA(NULL, "open", "explorer.exe", ("/select," + configPath).c_str(), NULL, SW_SHOW);
                } else {
                    MessageBoxA(hwnd, "无法获取配置文件路径", "错误", MB_OK | MB_ICONERROR);
                }
            }
            break;
        }

        case IDM_EXIT:
            // Destroy window first (triggers WM_DESTROY for cleanup)
            DestroyWindow(hwnd);
            break;
        }
        break;

    case WM_DESTROY:
        // 停止 mihomo
        if (g_mihomoManager) {
            g_mihomoManager->Stop();
            delete g_mihomoManager;
            g_mihomoManager = nullptr;
        }
        // 清理托盘图标
        if (g_trayIcon) {
            delete g_trayIcon;
            g_trayIcon = nullptr;
        }
        // 清理 HTTP 服务器
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

// 主函数
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // 单实例检查
    const char* mutexName = "Global\\SysProxyBar_SingleInstance_Mutex";
    HANDLE hMutex = CreateMutexA(NULL, TRUE, mutexName);

    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        // 已经有一个实例在运行
        MessageBoxA(NULL, "SysProxyBar 已经在运行中", "提示", MB_OK | MB_ICONINFORMATION);
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }

    // 加载配置
    LoadConfig();

    // 注册窗口类
    WNDCLASSEXA wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXA);
    wc.style = 0;
    wc.lpfnWndProc = WindowProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = hInstance;
    wc.hIcon = NULL;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = "SysProxyBarClass";
    wc.hIconSm = NULL;

    if (!RegisterClassExA(&wc)) {
        // 如果类已注册，可能是上次崩溃残留
        if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            MessageBoxA(NULL, "窗口类注册失败", "错误", MB_ICONERROR);
            if (hMutex) CloseHandle(hMutex);
            return 1;
        }
    }

    // 创建隐藏窗口
    HWND hwnd = CreateWindowExA(
        0,
        "SysProxyBarClass",
        "系统代理控制",
        0,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        NULL, NULL, hInstance, NULL
    );

    if (!hwnd) {
        MessageBoxA(NULL, "窗口创建失败", "错误", MB_ICONERROR);
        if (hMutex) CloseHandle(hMutex);
        return 1;
    }

    // 消息循环
    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    // 清理互斥体
    if (hMutex) CloseHandle(hMutex);

    return (int)msg.wParam;
}
