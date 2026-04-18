#pragma once

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <string>

class ProxyManager {
public:
    static bool IsProxyEnabled();
    static bool SetProxyEnabled(bool enabled);
    static bool SetProxyConfig(const std::string& server, int port, const std::string& bypass);
    static bool GetProxyConfig(std::string& server, int& port, std::string& bypass);
    static bool NotifySystemChange();

private:
    static const std::string GetProxyRegKey();
};
