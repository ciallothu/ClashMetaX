#pragma once

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#include <windows.h>
#include <string>
#include <shlobj.h>

/**
 * MihomoManager - Manages mihomo.exe process lifecycle
 *
 * This class handles:
 * - Extracting mihomo.exe and config.yaml from embedded resources
 * - Starting and stopping the mihomo process
 * - Monitoring process health and auto-restart on crash
 * - Managing process lifecycle tied to application
 */
class MihomoManager {
public:
    MihomoManager();
    ~MihomoManager();

    // Initialize: Extract files and prepare for launch
    bool Initialize();

    // Start mihomo process
    bool Start();

    // Stop mihomo process
    void Stop();

    // Restart mihomo process
    bool Restart();

    // Check if process is running
    bool IsRunning() const;

    // TUN mode helpers
    bool IsTunEnabled() const;
    bool SetTunEnabled(bool enable);

    // Get paths
    std::string GetWorkingDirectory() const { return m_workDir; }
    std::string GetExecutablePath() const { return m_exePath; }
    std::string GetConfigPath() const { return m_configPath; }

private:
    struct MihomoManifest {
        std::string version;
        unsigned long long size;
        std::string sha256;

        MihomoManifest() : size(0) {}
    };

    // Extract mihomo.exe from resources
    bool ExtractMihomoExe();

    // Check if mihomo update is needed
    bool IsMihomoUpdateNeeded();

    // Load embedded manifest metadata
    bool LoadEmbeddedMihomoManifest();

    // Get current mihomo version from manifest
    std::string GetEmbeddedMihomoVersion();

    // Get embedded mihomo.exe SHA-256 from manifest
    std::string GetEmbeddedMihomoHash();

    // Get embedded mihomo.exe size from manifest
    unsigned long long GetEmbeddedMihomoSize();

    // Get installed mihomo.exe SHA-256
    std::string GetInstalledMihomoHash();

    // Get installed mihomo.exe size
    bool GetInstalledMihomoSize(unsigned long long* size);

    // Extract default config.yaml from resources
    bool ExtractDefaultConfig();

    // Ensure target directory exists
    bool EnsureDirectoryExists(const std::string& path);

    // Extract resource to file
    bool ExtractResource(int resourceId, const char* resourceType,
                        const std::string& outputPath);

    // Monitoring thread function
    static unsigned __stdcall MonitorThread(void* param);

    // Start monitoring thread
    void StartMonitor();

    // Stop monitoring thread
    void StopMonitor();

    // Check if file exists
    bool FileExists(const std::string& path) const;

    // Stop any stale mihomo.exe process using the managed work directory
    bool StopManagedMihomoProcesses();

    // Update tun.enable in the runtime config file
    bool UpdateTunConfig(bool enable);

private:
    PROCESS_INFORMATION m_processInfo;
    HANDLE m_monitorThread;
    bool m_initialized;
    bool m_shouldMonitor;
    std::string m_workDir;
    std::string m_exePath;
    std::string m_configPath;
    bool m_manifestLoaded;
    MihomoManifest m_manifest;
};
