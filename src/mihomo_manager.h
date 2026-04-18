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
#include <vector>

struct KernelMetadata {
    std::string id;
    std::string version;
    std::string source;
    std::string path;
    std::string installedAt;
    std::string arch;
    std::string assetName;
    std::string sha256;
};

class KernelRegistry {
public:
    explicit KernelRegistry(const std::string& appDir);

    bool Initialize();
    std::vector<KernelMetadata> GetInstalledKernels() const;
    KernelMetadata GetSelectedKernel() const;
    std::string GetSelectedKernelId() const;
    bool HasKernels() const;
    bool AddOrUpdateKernel(const KernelMetadata& kernel);
    bool SelectKernel(const std::string& kernelId);

    const std::string& GetKernelsDir() const { return m_kernelsDir; }
    const std::string& GetStateFilePath() const { return m_statePath; }

private:
    bool LoadState();
    bool SaveState() const;
    void ScanKernelDirectories();

private:
    std::string m_appDir;
    std::string m_kernelsDir;
    std::string m_stateDir;
    std::string m_statePath;
    std::string m_selectedKernelId;
    std::vector<KernelMetadata> m_kernels;
};

class KernelDownloader {
public:
    KernelDownloader();
    bool DownloadLatestStable(const std::string& kernelsDir, KernelMetadata* installedKernel, std::string* error);

private:
    bool QueryLatestStableRelease(std::string* tag, std::string* assetName, std::string* assetUrl, std::string* error);
    bool DownloadFile(const std::string& url, const std::string& outputPath, std::string* error);
    bool ExtractZip(const std::string& zipPath, const std::string& destination, std::string* error);
    std::string NowIso8601Utc() const;
};

class KernelProcessManager {
public:
    KernelProcessManager();
    ~KernelProcessManager();

    bool Start(const std::string& kernelExePath, const std::string& workDir, const std::string& configPath);
    void Stop();
    bool Restart(const std::string& kernelExePath, const std::string& workDir, const std::string& configPath);
    bool IsRunning() const;

private:
    static unsigned __stdcall MonitorThread(void* param);
    void StartMonitor();
    void StopMonitor();

private:
    PROCESS_INFORMATION m_processInfo;
    HANDLE m_monitorThread;
    bool m_shouldMonitor;
    std::string m_lastKernelExe;
    std::string m_lastWorkDir;
    std::string m_lastConfig;
};

class MihomoManager {
public:
    MihomoManager();
    ~MihomoManager();

    bool Initialize();
    bool Start();
    void Stop();
    bool Restart();
    bool IsRunning() const;

    bool IsTunEnabled() const;
    bool SetTunEnabled(bool enable);

    std::string GetWorkingDirectory() const { return m_workDir; }
    std::string GetConfigPath() const { return m_configPath; }
    std::string GetKernelDirectory() const { return m_registry.GetKernelsDir(); }

    std::vector<KernelMetadata> GetInstalledKernels() const;
    KernelMetadata GetCurrentKernel() const;
    bool SwitchKernel(const std::string& kernelId, std::string* error);
    bool DownloadLatestKernel(std::string* error);

private:
    bool EnsureDirectoryExists(const std::string& path);
    bool EnsureDefaultConfig();
    bool FileExists(const std::string& path) const;
    bool UpdateTunConfig(bool enable);

private:
    std::string m_workDir;
    std::string m_configPath;
    KernelRegistry m_registry;
    KernelDownloader m_downloader;
    KernelProcessManager m_processManager;
    bool m_initialized;
};
