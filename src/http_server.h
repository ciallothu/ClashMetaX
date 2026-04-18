#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <winsock2.h>
#include <windows.h>
#include <string>
#include <map>

class HTTPServer {
public:
    HTTPServer();
    ~HTTPServer();

    bool Start(int port = 9090);
    void Stop();
    bool IsRunning() const { return m_running; }

private:
    SOCKET m_listenSocket;
    int m_port;
    bool m_running;
    HANDLE m_thread;

    static DWORD WINAPI ServerThread(LPVOID lpParam);
    void Run();
    void HandleClient(SOCKET clientSocket);
    std::string GetResourceData(const std::string& path);
    std::string GetMimeType(const std::string& path);
    std::string URLDecode(const std::string& url);
};
