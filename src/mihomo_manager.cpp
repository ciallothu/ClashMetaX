#include "mihomo_manager.h"
#include "mihomo_resource.h"
#include <stdio.h>
#include <io.h>
#include <process.h>
#include <tlhelp32.h>

#pragma comment(lib, "shell32.lib")

MihomoManager::MihomoManager()
    : m_monitorThread(NULL)
    , m_initialized(false)
    , m_shouldMonitor(false)
{
    ZeroMemory(&m_processInfo, sizeof(m_processInfo));

    // Get AppData path
    char appdataPath[MAX_PATH];
    if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appdataPath) == S_OK) {
        m_workDir = std::string(appdataPath) + "\\SysProxyBar";
        m_exePath = m_workDir + "\\mihomo.exe";
        m_configPath = m_workDir + "\\config.yaml";
    }
}

MihomoManager::~MihomoManager() {
    Stop();
}

bool MihomoManager::FileExists(const std::string& path) const {
    return (_access(path.c_str(), 0) == 0);
}

bool MihomoManager::StopManagedMihomoProcesses() {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        printf("WARNING: Failed to enumerate processes (error: %lu)\n", GetLastError());
        return false;
    }

    bool stoppedAny = false;
    int widePathLength = MultiByteToWideChar(CP_ACP, 0, m_exePath.c_str(), -1, NULL, 0);
    std::wstring managedExePath;
    if (widePathLength > 0) {
        managedExePath.resize(widePathLength);
        MultiByteToWideChar(CP_ACP, 0, m_exePath.c_str(), -1, &managedExePath[0], widePathLength);
        if (!managedExePath.empty() && managedExePath.back() == L'\0') {
            managedExePath.pop_back();
        }
    }

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(pe);

    if (Process32First(snapshot, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, L"mihomo.exe") != 0) {
                continue;
            }

            HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE | SYNCHRONIZE, FALSE, pe.th32ProcessID);
            if (process == NULL) {
                continue;
            }

            wchar_t processPath[MAX_PATH];
            DWORD processPathSize = MAX_PATH;
            bool shouldStop = false;
            if (!managedExePath.empty() && QueryFullProcessImageNameW(process, 0, processPath, &processPathSize)) {
                shouldStop = (_wcsicmp(processPath, managedExePath.c_str()) == 0);
            }

            if (shouldStop) {
                printf("Stopping stale mihomo process (PID: %lu)\n", pe.th32ProcessID);
                if (TerminateProcess(process, 0)) {
                    WaitForSingleObject(process, 5000);
                    stoppedAny = true;
                } else {
                    printf("WARNING: Failed to terminate stale mihomo process (error: %lu)\n", GetLastError());
                }
            }

            CloseHandle(process);
        } while (Process32Next(snapshot, &pe));
    }

    CloseHandle(snapshot);
    return stoppedAny;
}

bool MihomoManager::EnsureDirectoryExists(const std::string& path) {
    if (path.empty()) {
        return false;
    }

    // Check if directory already exists
    DWORD attrib = GetFileAttributesA(path.c_str());
    if (attrib != INVALID_FILE_ATTRIBUTES &&
        (attrib & FILE_ATTRIBUTE_DIRECTORY)) {
        return true;
    }

    // Create directory (recursive)
    if (CreateDirectoryA(path.c_str(), NULL) ||
        GetLastError() == ERROR_ALREADY_EXISTS) {
        return true;
    }

    return false;
}

bool MihomoManager::ExtractResource(int resourceId, const char* resourceType,
                                   const std::string& outputPath) {
    // Get module handle
    HMODULE hModule = GetModuleHandle(NULL);

    // Find resource
    HRSRC hRes = FindResourceA(hModule, MAKEINTRESOURCEA(resourceId), resourceType);
    if (hRes == NULL) {
        printf("ERROR: FindResource failed for ID %d, type %s\n", resourceId, resourceType);
        return false;
    }

    // Load resource
    HGLOBAL hLoaded = LoadResource(hModule, hRes);
    if (hLoaded == NULL) {
        printf("ERROR: LoadResource failed\n");
        return false;
    }

    // Lock resource
    void* pData = LockResource(hLoaded);
    if (pData == NULL) {
        printf("ERROR: LockResource failed\n");
        return false;
    }

    // Get resource size
    DWORD size = SizeofResource(hModule, hRes);
    if (size == 0) {
        printf("ERROR: SizeofResource returned 0\n");
        return false;
    }

    // Create file
    HANDLE hFile = CreateFileA(outputPath.c_str(), GENERIC_WRITE, 0, NULL,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        printf("ERROR: CreateFile failed for %s (error: %lu)\n",
               outputPath.c_str(), GetLastError());
        return false;
    }

    // Write resource data to file
    DWORD written;
    BOOL result = WriteFile(hFile, pData, size, &written, NULL);
    CloseHandle(hFile);

    if (!result || written != size) {
        printf("ERROR: WriteFile failed (written: %lu, expected: %lu)\n",
               written, size);
        return false;
    }

    printf("Extracted: %s (%lu bytes)\n", outputPath.c_str(), size);
    return true;
}

std::string MihomoManager::GetEmbeddedMihomoVersion() {
    HMODULE hModule = GetModuleHandle(NULL);
    HRSRC hRes = FindResourceA(hModule, MAKEINTRESOURCEA(IDR_MIHOMO_VERSION), "MIHOMO");
    if (hRes == NULL) {
        return "";
    }

    HGLOBAL hLoaded = LoadResource(hModule, hRes);
    if (hLoaded == NULL) {
        return "";
    }

    void* pData = LockResource(hLoaded);
    DWORD size = SizeofResource(hModule, hRes);

    if (pData == NULL || size == 0) {
        return "";
    }

    // Convert to string (trim whitespace/newlines)
    std::string version((const char*)pData, size);
    size_t end = version.find_last_not_of(" \t\r\n");
    if (end != std::string::npos) {
        version = version.substr(0, end + 1);
    }

    return version;
}

std::string MihomoManager::GetInstalledMihomoVersion() {
    std::string versionFile = m_workDir + "\\.mihomo_version";

    HANDLE hFile = CreateFileA(versionFile.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return "";
    }

    char buffer[32];
    DWORD bytesRead;
    BOOL result = ReadFile(hFile, buffer, sizeof(buffer) - 1, &bytesRead, NULL);
    CloseHandle(hFile);

    if (!result || bytesRead == 0) {
        return "";
    }

    buffer[bytesRead] = '\0';
    return std::string(buffer);
}

bool MihomoManager::IsMihomoUpdateNeeded() {
    // If mihomo.exe doesn't exist, need to extract
    if (!FileExists(m_exePath)) {
        return true;
    }

    // Get embedded version
    std::string embeddedVersion = GetEmbeddedMihomoVersion();
    if (embeddedVersion.empty()) {
        printf("Warning: Failed to read embedded mihomo version\n");
        return false;
    }

    // Get installed version
    std::string installedVersion = GetInstalledMihomoVersion();
    if (installedVersion.empty()) {
        printf("Mihomo not installed yet\n");
        return true;
    }

    // Compare versions
    if (embeddedVersion == installedVersion) {
        printf("Mihomo up to date: %s\n", embeddedVersion.c_str());
        return false;
    }

    printf("Mihomo update available: %s -> %s\n", installedVersion.c_str(), embeddedVersion.c_str());
    return true;
}

bool MihomoManager::ExtractMihomoExe() {
    // Check if update is needed
    if (!IsMihomoUpdateNeeded()) {
        return true;
    }

    // A stale mihomo.exe from an older build can keep the target file locked and
    // block replacement during initialization.
    StopManagedMihomoProcesses();

    printf("Extracting mihomo.exe...\n");
    if (!ExtractResource(IDR_MIHOMO_EXE, "MIHOMO", m_exePath)) {
        return false;
    }

    // Write version file
    std::string embeddedVersion = GetEmbeddedMihomoVersion();
    if (!embeddedVersion.empty()) {
        std::string versionFile = m_workDir + "\\.mihomo_version";
        HANDLE hFile = CreateFileA(versionFile.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hFile != INVALID_HANDLE_VALUE) {
            DWORD written;
            WriteFile(hFile, embeddedVersion.c_str(), embeddedVersion.length(), &written, NULL);
            CloseHandle(hFile);
            printf("Version saved: %s\n", embeddedVersion.c_str());
        }
    }

    return true;
}

bool MihomoManager::ExtractDefaultConfig() {
    // Check if already exists (don't overwrite user config)
    if (FileExists(m_configPath)) {
        printf("Config already exists: %s\n", m_configPath.c_str());
        return true;
    }

    printf("Extracting config.yaml...\n");
    return ExtractResource(IDR_MIHOMO_CONFIG, "MIHOMO", m_configPath);
}

bool MihomoManager::Initialize() {
    if (m_initialized) {
        return true;
    }

    printf("Initializing MihomoManager...\n");

    // Ensure working directory exists
    if (!EnsureDirectoryExists(m_workDir)) {
        printf("ERROR: Failed to create directory: %s\n", m_workDir.c_str());
        return false;
    }

    // Extract mihomo.exe
    if (!ExtractMihomoExe()) {
        printf("ERROR: Failed to extract mihomo.exe\n");
        return false;
    }

    // Extract config.yaml
    if (!ExtractDefaultConfig()) {
        printf("ERROR: Failed to extract config.yaml\n");
        return false;
    }

    m_initialized = true;
    printf("MihomoManager initialized successfully\n");
    printf("  Work dir: %s\n", m_workDir.c_str());
    printf("  Executable: %s\n", m_exePath.c_str());
    printf("  Config: %s\n", m_configPath.c_str());

    return true;
}

bool MihomoManager::Start() {
    if (IsRunning()) {
        printf("Mihomo is already running\n");
        return true;
    }

    if (!m_initialized && !Initialize()) {
        printf("ERROR: Not initialized\n");
        return false;
    }

    printf("Starting mihomo...\n");

    // Prepare startup info
    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;  // Hide window

    ZeroMemory(&m_processInfo, sizeof(m_processInfo));

    // Build command line: "mihomo.exe" -d "." -f "config.yaml"
    // Use quotes to handle paths with spaces
    std::string cmdLine = "\"" + m_exePath + "\" -d \"" +
                         m_workDir + "\" -f \"" + m_configPath + "\"";

    printf("Command line: %s\n", cmdLine.c_str());

    // Create process
    BOOL result = CreateProcessA(
        NULL,                          // Application name
        const_cast<char*>(cmdLine.c_str()), // Command line
        NULL,                          // Process security attributes
        NULL,                          // Thread security attributes
        FALSE,                         // Do not inherit handles
        CREATE_NO_WINDOW,              // Creation flags
        NULL,                          // Environment
        m_workDir.c_str(),             // Working directory
        &si,                           // Startup info
        &m_processInfo                 // Process information
    );

    if (!result) {
        DWORD error = GetLastError();
        printf("ERROR: CreateProcess failed (error: %lu)\n", error);
        return false;
    }

    printf("Mihomo started successfully (PID: %lu)\n",
           m_processInfo.dwProcessId);

    // Start monitor thread only if not already monitoring
    if (m_monitorThread == NULL) {
        StartMonitor();
    }

    return true;
}

void MihomoManager::Stop() {
    printf("Stopping mihomo...\n");

    // Stop monitor thread first to prevent restart
    StopMonitor();

    // Terminate process if still running
    if (m_processInfo.hProcess != NULL) {
        // Check if process is still running before terminating
        DWORD exitCode;
        if (GetExitCodeProcess(m_processInfo.hProcess, &exitCode) && exitCode == STILL_ACTIVE) {
            printf("Terminating mihomo process (PID: %lu)...\n", m_processInfo.dwProcessId);
            if (!TerminateProcess(m_processInfo.hProcess, 0)) {
                printf("WARNING: TerminateProcess failed (error: %lu)\n", GetLastError());
            } else {
                // Wait for process to terminate (max 2 seconds)
                WaitForSingleObject(m_processInfo.hProcess, 2000);
            }
        }
        CloseHandle(m_processInfo.hProcess);
        m_processInfo.hProcess = NULL;
    }

    if (m_processInfo.hThread != NULL) {
        CloseHandle(m_processInfo.hThread);
        m_processInfo.hThread = NULL;
    }

    printf("Mihomo stopped\n");
}

bool MihomoManager::Restart() {
    printf("Restarting mihomo...\n");
    Stop();
    Sleep(500);  // Wait a bit for cleanup
    return Start();
}

bool MihomoManager::IsRunning() const {
    if (m_processInfo.hProcess == NULL) {
        return false;
    }

    DWORD exitCode;
    if (GetExitCodeProcess(m_processInfo.hProcess, &exitCode)) {
        return (exitCode == STILL_ACTIVE);
    }

    return false;
}

void MihomoManager::StartMonitor() {
    if (m_monitorThread != NULL) {
        return;  // Already monitoring
    }

    m_shouldMonitor = true;

    // Create monitor thread
    m_monitorThread = (HANDLE)_beginthreadex(
        NULL, 0,
        MonitorThread,
        this,
        0,
        NULL
    );

    if (m_monitorThread == NULL) {
        printf("WARNING: Failed to create monitor thread\n");
    }
}

void MihomoManager::StopMonitor() {
    if (m_monitorThread == NULL) {
        return;
    }

    printf("Stopping monitor thread...\n");
    m_shouldMonitor = false;

    // Wait for thread to exit (max 5 seconds)
    WaitForSingleObject(m_monitorThread, 5000);
    CloseHandle(m_monitorThread);
    m_monitorThread = NULL;
}

unsigned __stdcall MihomoManager::MonitorThread(void* param) {
    MihomoManager* manager = static_cast<MihomoManager*>(param);

    printf("Monitor thread started\n");

    while (manager->m_shouldMonitor) {
        // Wait for process exit, timeout 1 second
        DWORD waitResult = WaitForSingleObject(
            manager->m_processInfo.hProcess, 1000);

        if (waitResult == WAIT_OBJECT_0) {
            // Process has exited
            DWORD exitCode;
            GetExitCodeProcess(manager->m_processInfo.hProcess, &exitCode);

            printf("Mihomo process exited (code: %lu)\n", exitCode);

            if (exitCode != 0 && manager->m_shouldMonitor) {
                // Abnormal exit, restart after delay
                printf("Abnormal exit, restarting in 2 seconds...\n");
                Sleep(2000);

                if (manager->m_shouldMonitor) {
                    // Close old handles
                    CloseHandle(manager->m_processInfo.hProcess);
                    CloseHandle(manager->m_processInfo.hThread);
                    ZeroMemory(&manager->m_processInfo, sizeof(manager->m_processInfo));

                    // Restart process without creating new monitor thread
                    STARTUPINFOA si;
                    ZeroMemory(&si, sizeof(si));
                    si.cb = sizeof(si);
                    si.dwFlags = STARTF_USESHOWWINDOW;
                    si.wShowWindow = SW_HIDE;

                    std::string cmdLine = "\"" + manager->m_exePath + "\" -d \"" +
                                         manager->m_workDir + "\" -f \"" + manager->m_configPath + "\"";

                    BOOL result = CreateProcessA(
                        NULL, const_cast<char*>(cmdLine.c_str()),
                        NULL, NULL, FALSE, CREATE_NO_WINDOW,
                        NULL, manager->m_workDir.c_str(), &si, &manager->m_processInfo);

                    if (result) {
                        printf("Mihomo restarted successfully (PID: %lu)\n",
                               manager->m_processInfo.dwProcessId);
                    } else {
                        printf("Failed to restart mihomo (error: %lu)\n", GetLastError());
                        break;
                    }
                }
            } else {
                // Normal exit or monitor stopped
                printf("Normal exit or monitor stopped, exiting monitor thread\n");
                break;
            }
        }
    }

    printf("Monitor thread exiting\n");
    return 0;
}
