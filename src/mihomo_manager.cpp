#include "mihomo_manager.h"
#include "mihomo_resource.h"
#include <stdio.h>
#include <stdlib.h>
#include <io.h>
#include <process.h>
#include <wincrypt.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <tlhelp32.h>

#pragma comment(lib, "shell32.lib")

MihomoManager::MihomoManager()
    : m_monitorThread(NULL)
    , m_initialized(false)
    , m_shouldMonitor(false)
    , m_manifestLoaded(false)
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

namespace {
std::string TrimLeft(const std::string& value) {
    size_t start = value.find_first_not_of(" \t");
    return (start == std::string::npos) ? "" : value.substr(start);
}

std::string Trim(const std::string& value) {
    size_t start = value.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    size_t end = value.find_last_not_of(" \t\r\n");
    return value.substr(start, end - start + 1);
}

std::string ToLowerAscii(const std::string& value) {
    std::string lowered = value;
    for (size_t i = 0; i < lowered.size(); ++i) {
        if (lowered[i] >= 'A' && lowered[i] <= 'Z') {
            lowered[i] = static_cast<char>(lowered[i] - 'A' + 'a');
        }
    }
    return lowered;
}

size_t IndentWidth(const std::string& value) {
    size_t indent = 0;
    while (indent < value.size() && (value[indent] == ' ' || value[indent] == '\t')) {
        indent++;
    }
    return indent;
}

bool LoadResourceData(int resourceId, const char* resourceType, const void** data, DWORD* size) {
    if (data == NULL || size == NULL) {
        return false;
    }

    HMODULE hModule = GetModuleHandle(NULL);
    HRSRC hRes = FindResourceA(hModule, MAKEINTRESOURCEA(resourceId), resourceType);
    if (hRes == NULL) {
        return false;
    }

    HGLOBAL hLoaded = LoadResource(hModule, hRes);
    if (hLoaded == NULL) {
        return false;
    }

    void* pData = LockResource(hLoaded);
    DWORD resourceSize = SizeofResource(hModule, hRes);
    if (pData == NULL || resourceSize == 0) {
        return false;
    }

    *data = pData;
    *size = resourceSize;
    return true;
}

std::string BytesToHex(const BYTE* data, DWORD size) {
    static const char kHexChars[] = "0123456789abcdef";
    std::string hex;
    hex.resize(size * 2);

    for (DWORD i = 0; i < size; ++i) {
        hex[i * 2] = kHexChars[(data[i] >> 4) & 0x0F];
        hex[i * 2 + 1] = kHexChars[data[i] & 0x0F];
    }

    return hex;
}

std::string FinalizeHash(HCRYPTHASH hash) {
    BYTE digest[32];
    DWORD digestSize = sizeof(digest);
    if (!CryptGetHashParam(hash, HP_HASHVAL, digest, &digestSize, 0)) {
        return "";
    }

    return BytesToHex(digest, digestSize);
}

std::string CreateSha256HashFromFile(const std::string& path) {
    HANDLE hFile = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return "";
    }

    HCRYPTPROV provider = 0;
    HCRYPTHASH hash = 0;
    std::string digest;

    if (!CryptAcquireContextA(&provider, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) {
        CloseHandle(hFile);
        return "";
    }

    if (CryptCreateHash(provider, CALG_SHA_256, 0, 0, &hash)) {
        BYTE buffer[8192];
        DWORD bytesRead = 0;
        BOOL readOk = FALSE;
        do {
            readOk = ReadFile(hFile, buffer, sizeof(buffer), &bytesRead, NULL);
            if (!readOk) {
                break;
            }
            if (bytesRead > 0 && !CryptHashData(hash, buffer, bytesRead, 0)) {
                readOk = FALSE;
                break;
            }
        } while (bytesRead > 0);

        if (readOk) {
            digest = FinalizeHash(hash);
        }
    }

    if (hash != 0) {
        CryptDestroyHash(hash);
    }
    if (provider != 0) {
        CryptReleaseContext(provider, 0);
    }

    CloseHandle(hFile);
    return digest;
}

bool GetFileSizeForPath(const std::string& path, unsigned long long* size) {
    if (size == NULL) {
        return false;
    }

    WIN32_FILE_ATTRIBUTE_DATA attrs;
    if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &attrs)) {
        return false;
    }

    ULARGE_INTEGER value;
    value.LowPart = attrs.nFileSizeLow;
    value.HighPart = attrs.nFileSizeHigh;
    *size = value.QuadPart;
    return true;
}

bool ParseUnsignedLongLong(const std::string& value, unsigned long long* result) {
    if (result == NULL || value.empty()) {
        return false;
    }

    char* end = NULL;
    unsigned long long parsed = _strtoui64(value.c_str(), &end, 10);
    if (end == value.c_str() || *end != '\0') {
        return false;
    }

    *result = parsed;
    return true;
}
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
    const void* pData = NULL;
    DWORD size = 0;
    if (!LoadResourceData(resourceId, resourceType, &pData, &size)) {
        printf("ERROR: FindResource failed for ID %d, type %s\n", resourceId, resourceType);
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

bool MihomoManager::LoadEmbeddedMihomoManifest() {
    if (m_manifestLoaded) {
        return !m_manifest.sha256.empty() && m_manifest.size > 0;
    }

    const void* pData = NULL;
    DWORD size = 0;
    if (!LoadResourceData(IDR_MIHOMO_MANIFEST, "MIHOMO", &pData, &size)) {
        return false;
    }

    MihomoManifest manifest;
    std::istringstream stream(std::string((const char*)pData, size));
    std::string line;
    while (std::getline(stream, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }

        size_t sep = line.find('=');
        if (sep == std::string::npos) {
            continue;
        }

        std::string key = Trim(line.substr(0, sep));
        std::string value = Trim(line.substr(sep + 1));
        if (key == "version") {
            manifest.version = value;
        } else if (key == "size") {
            if (!ParseUnsignedLongLong(value, &manifest.size)) {
                return false;
            }
        } else if (key == "sha256") {
            manifest.sha256 = ToLowerAscii(value);
        }
    }

    m_manifestLoaded = true;
    m_manifest = manifest;
    return !m_manifest.sha256.empty() && m_manifest.size > 0;
}

std::string MihomoManager::GetEmbeddedMihomoVersion() {
    if (!LoadEmbeddedMihomoManifest()) {
        return "";
    }

    return m_manifest.version;
}

std::string MihomoManager::GetEmbeddedMihomoHash() {
    if (!LoadEmbeddedMihomoManifest()) {
        return "";
    }

    return m_manifest.sha256;
}

unsigned long long MihomoManager::GetEmbeddedMihomoSize() {
    if (!LoadEmbeddedMihomoManifest()) {
        return 0;
    }

    return m_manifest.size;
}

std::string MihomoManager::GetInstalledMihomoHash() {
    return ToLowerAscii(CreateSha256HashFromFile(m_exePath));
}

bool MihomoManager::GetInstalledMihomoSize(unsigned long long* size) {
    return GetFileSizeForPath(m_exePath, size);
}

bool MihomoManager::IsTunEnabled() const {
    std::ifstream configFile(m_configPath.c_str(), std::ios::in);
    if (!configFile.is_open()) {
        return false;
    }

    std::string line;
    bool inTunSection = false;
    size_t tunIndent = 0;

    while (std::getline(configFile, line)) {
        std::string trimmed = TrimLeft(line);
        size_t indent = IndentWidth(line);

        if (!inTunSection) {
            if (trimmed == "tun:") {
                inTunSection = true;
                tunIndent = indent;
            }
            continue;
        }

        if (!trimmed.empty() && trimmed[0] != '#' && indent <= tunIndent) {
            break;
        }

        if (indent > tunIndent && trimmed.rfind("enable:", 0) == 0) {
            std::string value = Trim(trimmed.substr(7));
            return value == "true";
        }
    }

    return false;
}

bool MihomoManager::UpdateTunConfig(bool enable) {
    std::ifstream configFile(m_configPath.c_str(), std::ios::in);
    if (!configFile.is_open()) {
        printf("ERROR: Failed to open config file: %s\n", m_configPath.c_str());
        return false;
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(configFile, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(line);
    }
    configFile.close();

    bool inTunSection = false;
    bool updated = false;
    size_t tunIndent = 0;

    for (size_t i = 0; i < lines.size(); ++i) {
        const std::string& currentLine = lines[i];
        std::string trimmed = TrimLeft(currentLine);
        size_t indent = IndentWidth(currentLine);

        if (!inTunSection) {
            if (trimmed == "tun:") {
                inTunSection = true;
                tunIndent = indent;
            }
            continue;
        }

        if (!trimmed.empty() && trimmed[0] != '#' && indent <= tunIndent) {
            break;
        }

        if (indent > tunIndent && trimmed.rfind("enable:", 0) == 0) {
            lines[i] = std::string(indent, ' ') + "enable: " + (enable ? "true" : "false");
            updated = true;
            break;
        }
    }

    if (!updated) {
        if (!lines.empty() && !lines.back().empty()) {
            lines.push_back("");
        }
        lines.push_back("tun:");
        lines.push_back(std::string(2, ' ') + "enable: " + (enable ? "true" : "false"));
        lines.push_back("  stack: mixed");
        lines.push_back("  dns-hijack:");
        lines.push_back("    - 'any:53'");
        lines.push_back("    - 'tcp://any:53'");
        lines.push_back("  auto-route: true");
        lines.push_back("  auto-detect-interface: true");
        lines.push_back("  strict-route: true");
    }

    std::ofstream outputFile(m_configPath.c_str(), std::ios::out | std::ios::trunc);
    if (!outputFile.is_open()) {
        printf("ERROR: Failed to write config file: %s\n", m_configPath.c_str());
        return false;
    }

    for (size_t i = 0; i < lines.size(); ++i) {
        outputFile << lines[i] << "\n";
    }

    return outputFile.good();
}

bool MihomoManager::SetTunEnabled(bool enable) {
    if (!FileExists(m_configPath)) {
        if (!ExtractDefaultConfig()) {
            return false;
        }
    }

    bool current = IsTunEnabled();
    if (current == enable) {
        return true;
    }

    if (!UpdateTunConfig(enable)) {
        return false;
    }

    printf("TUN mode %s\n", enable ? "enabled" : "disabled");

    if (IsRunning()) {
        return Restart();
    }

    return Start();
}

bool MihomoManager::IsMihomoUpdateNeeded() {
    // If mihomo.exe doesn't exist, need to extract
    if (!FileExists(m_exePath)) {
        return true;
    }

    unsigned long long embeddedSize = GetEmbeddedMihomoSize();
    std::string embeddedHash = GetEmbeddedMihomoHash();
    if (embeddedSize == 0 || embeddedHash.empty()) {
        printf("Warning: Failed to read embedded mihomo manifest\n");
        return false;
    }

    std::string embeddedVersion = GetEmbeddedMihomoVersion();

    unsigned long long installedSize = 0;
    if (!GetInstalledMihomoSize(&installedSize)) {
        printf("Installed mihomo.exe is unreadable, refresh required\n");
        return true;
    }

    if (embeddedSize != installedSize) {
        if (!embeddedVersion.empty()) {
            printf("Mihomo size changed, updating to embedded version %s\n", embeddedVersion.c_str());
        } else {
            printf("Mihomo size differs from embedded manifest, update needed\n");
        }
        return true;
    }

    std::string installedHash = GetInstalledMihomoHash();
    if (installedHash.empty()) {
        printf("Installed mihomo.exe hash check failed, refresh required\n");
        return true;
    }

    if (embeddedHash == installedHash) {
        if (!embeddedVersion.empty()) {
            printf("Mihomo up to date: %s\n", embeddedVersion.c_str());
        } else {
            printf("Mihomo binary matches embedded resource\n");
        }
        return false;
    }

    if (!embeddedVersion.empty()) {
        printf("Mihomo hash changed, updating to embedded version %s\n", embeddedVersion.c_str());
    } else {
        printf("Mihomo hash differs from embedded manifest, update needed\n");
    }
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
    std::string tempPath = m_exePath + ".new";
    if (!ExtractResource(IDR_MIHOMO_EXE, "MIHOMO", tempPath)) {
        return false;
    }

    unsigned long long embeddedSize = GetEmbeddedMihomoSize();
    std::string embeddedHash = GetEmbeddedMihomoHash();
    unsigned long long extractedSize = 0;
    std::string extractedHash = CreateSha256HashFromFile(tempPath);
    if (embeddedSize == 0 || embeddedHash.empty() ||
        !GetFileSizeForPath(tempPath, &extractedSize) || extractedSize != embeddedSize ||
        extractedHash.empty() || ToLowerAscii(extractedHash) != embeddedHash) {
        printf("ERROR: Extracted mihomo.exe hash verification failed\n");
        DeleteFileA(tempPath.c_str());
        return false;
    }

    if (!MoveFileExA(tempPath.c_str(), m_exePath.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        printf("ERROR: Failed to replace mihomo.exe (error: %lu)\n", GetLastError());
        DeleteFileA(tempPath.c_str());
        return false;
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
