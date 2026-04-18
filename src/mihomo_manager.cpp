#include "mihomo_manager.h"

#include <shlobj.h>
#include <urlmon.h>
#include <io.h>
#include <process.h>
#include <wincrypt.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdio>
#include <ctime>

#pragma comment(lib, "urlmon.lib")

namespace {
bool FileExists(const std::string& path) {
    return _access(path.c_str(), 0) == 0;
}

bool EnsureDirectoryExists(const std::string& path) {
    if (path.empty()) {
        return false;
    }
    DWORD attributes = GetFileAttributesA(path.c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY)) {
        return true;
    }

    size_t pos = path.find_last_of("\\/");
    if (pos != std::string::npos) {
        std::string parent = path.substr(0, pos);
        if (!parent.empty() && !EnsureDirectoryExists(parent)) {
            return false;
        }
    }

    return CreateDirectoryA(path.c_str(), NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
}

std::string EscapeJson(const std::string& value) {
    std::string out;
    for (size_t i = 0; i < value.size(); ++i) {
        const char c = value[i];
        if (c == '\\' || c == '"') {
            out.push_back('\\');
            out.push_back(c);
        } else if (c == '\n') {
            out += "\\n";
        } else {
            out.push_back(c);
        }
    }
    return out;
}

std::string UnescapeJson(const std::string& value) {
    std::string out;
    bool escaped = false;
    for (size_t i = 0; i < value.size(); ++i) {
        char c = value[i];
        if (escaped) {
            if (c == 'n') {
                out.push_back('\n');
            } else {
                out.push_back(c);
            }
            escaped = false;
        } else if (c == '\\') {
            escaped = true;
        } else {
            out.push_back(c);
        }
    }
    return out;
}

std::string JsonValue(const std::string& blob, const std::string& key) {
    std::string marker = "\"" + key + "\"";
    size_t k = blob.find(marker);
    if (k == std::string::npos) {
        return "";
    }
    size_t colon = blob.find(':', k + marker.size());
    size_t quote1 = blob.find('"', colon + 1);
    size_t quote2 = blob.find('"', quote1 + 1);
    if (colon == std::string::npos || quote1 == std::string::npos || quote2 == std::string::npos) {
        return "";
    }
    return UnescapeJson(blob.substr(quote1 + 1, quote2 - quote1 - 1));
}

std::string MakeKernelId(const std::string& version, const std::string& arch) {
    return version + "-" + arch;
}

std::string NormalizePathForJson(const std::string& path) {
    std::string normalized = path;
    for (size_t i = 0; i < normalized.size(); ++i) {
        if (normalized[i] == '\\') {
            normalized[i] = '/';
        }
    }
    return normalized;
}

std::string QuoteForPowershell(const std::string& path) {
    std::string escaped;
    for (size_t i = 0; i < path.size(); ++i) {
        if (path[i] == '\'') {
            escaped += "''";
        } else {
            escaped.push_back(path[i]);
        }
    }
    return "'" + escaped + "'";
}

bool CopyFileAtomic(const std::string& from, const std::string& to) {
    std::string tmp = to + ".tmp";
    if (!CopyFileA(from.c_str(), tmp.c_str(), FALSE)) {
        return false;
    }
    if (!MoveFileExA(tmp.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        DeleteFileA(tmp.c_str());
        return false;
    }
    return true;
}

std::string ComputeSha256(const std::string& filePath) {
    HANDLE hFile = CreateFileA(filePath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return "";
    }

    HCRYPTPROV provider = 0;
    HCRYPTHASH hash = 0;
    std::string result;
    if (CryptAcquireContextA(&provider, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT) &&
        CryptCreateHash(provider, CALG_SHA_256, 0, 0, &hash)) {
        BYTE buffer[8192];
        DWORD read = 0;
        bool ok = true;
        while (ReadFile(hFile, buffer, sizeof(buffer), &read, NULL) && read > 0) {
            if (!CryptHashData(hash, buffer, read, 0)) {
                ok = false;
                break;
            }
        }
        if (ok) {
            BYTE digest[32];
            DWORD digestSize = sizeof(digest);
            if (CryptGetHashParam(hash, HP_HASHVAL, digest, &digestSize, 0)) {
                static const char* kHex = "0123456789abcdef";
                result.reserve(digestSize * 2);
                for (DWORD i = 0; i < digestSize; ++i) {
                    result.push_back(kHex[(digest[i] >> 4) & 0x0F]);
                    result.push_back(kHex[digest[i] & 0x0F]);
                }
            }
        }
    }

    if (hash) CryptDestroyHash(hash);
    if (provider) CryptReleaseContext(provider, 0);
    CloseHandle(hFile);
    return result;
}
}

KernelRegistry::KernelRegistry(const std::string& appDir)
    : m_appDir(appDir)
    , m_kernelsDir(appDir + "\\kernels")
    , m_stateDir(appDir + "\\state")
    , m_statePath(appDir + "\\state\\kernel-state.json") {}

bool KernelRegistry::Initialize() {
    if (!EnsureDirectoryExists(m_kernelsDir) || !EnsureDirectoryExists(m_stateDir)) {
        return false;
    }

    ScanKernelDirectories();
    if (!LoadState()) {
        return SaveState();
    }
    return true;
}

std::vector<KernelMetadata> KernelRegistry::GetInstalledKernels() const { return m_kernels; }

KernelMetadata KernelRegistry::GetSelectedKernel() const {
    for (size_t i = 0; i < m_kernels.size(); ++i) {
        if (m_kernels[i].id == m_selectedKernelId) {
            return m_kernels[i];
        }
    }
    return m_kernels.empty() ? KernelMetadata() : m_kernels[0];
}

std::string KernelRegistry::GetSelectedKernelId() const { return m_selectedKernelId; }
bool KernelRegistry::HasKernels() const { return !m_kernels.empty(); }

bool KernelRegistry::AddOrUpdateKernel(const KernelMetadata& kernel) {
    if (!FileExists(kernel.path)) {
        return false;
    }
    for (size_t i = 0; i < m_kernels.size(); ++i) {
        if (m_kernels[i].id == kernel.id) {
            m_kernels[i] = kernel;
            return SaveState();
        }
    }
    m_kernels.push_back(kernel);
    if (m_selectedKernelId.empty()) {
        m_selectedKernelId = kernel.id;
    }
    return SaveState();
}

bool KernelRegistry::SelectKernel(const std::string& kernelId) {
    for (size_t i = 0; i < m_kernels.size(); ++i) {
        if (m_kernels[i].id == kernelId && FileExists(m_kernels[i].path)) {
            m_selectedKernelId = kernelId;
            return SaveState();
        }
    }
    return false;
}

bool KernelRegistry::LoadState() {
    std::ifstream in(m_statePath.c_str(), std::ios::in | std::ios::binary);
    if (!in.is_open()) {
        return false;
    }
    std::stringstream ss;
    ss << in.rdbuf();
    const std::string content = ss.str();
    m_selectedKernelId = JsonValue(content, "selected_kernel_id");
    if (m_selectedKernelId.empty() && !m_kernels.empty()) {
        m_selectedKernelId = m_kernels[0].id;
    }
    return true;
}

bool KernelRegistry::SaveState() const {
    if (!EnsureDirectoryExists(m_stateDir)) {
        return false;
    }

    std::ofstream out(m_statePath.c_str(), std::ios::out | std::ios::trunc | std::ios::binary);
    if (!out.is_open()) {
        return false;
    }

    KernelMetadata selected;
    for (size_t i = 0; i < m_kernels.size(); ++i) {
        if (m_kernels[i].id == m_selectedKernelId) {
            selected = m_kernels[i];
            break;
        }
    }

    out << "{\n";
    out << "  \"selected_kernel_id\": \"" << EscapeJson(m_selectedKernelId) << "\",\n";
    out << "  \"selected_version\": \"" << EscapeJson(selected.version) << "\",\n";
    out << "  \"installed_kernels\": [\n";
    for (size_t i = 0; i < m_kernels.size(); ++i) {
        const KernelMetadata& k = m_kernels[i];
        out << "    {\n";
        out << "      \"id\": \"" << EscapeJson(k.id) << "\",\n";
        out << "      \"version\": \"" << EscapeJson(k.version) << "\",\n";
        out << "      \"source\": \"" << EscapeJson(k.source) << "\",\n";
        out << "      \"path\": \"" << EscapeJson(NormalizePathForJson(k.path)) << "\",\n";
        out << "      \"installed_at\": \"" << EscapeJson(k.installedAt) << "\",\n";
        out << "      \"arch\": \"" << EscapeJson(k.arch) << "\",\n";
        out << "      \"asset_name\": \"" << EscapeJson(k.assetName) << "\",\n";
        out << "      \"sha256\": \"" << EscapeJson(k.sha256) << "\"\n";
        out << "    }" << (i + 1 < m_kernels.size() ? "," : "") << "\n";
    }
    out << "  ]\n";
    out << "}\n";
    return true;
}

void KernelRegistry::ScanKernelDirectories() {
    m_kernels.clear();
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA((m_kernelsDir + "\\*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) {
        return;
    }

    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) continue;
        std::string kernelDir = m_kernelsDir + "\\" + fd.cFileName;
        std::string exePath = kernelDir + "\\mihomo.exe";
        if (!FileExists(exePath)) continue;

        KernelMetadata km;
        km.id = fd.cFileName;
        km.version = fd.cFileName;
        km.source = "downloaded";
        km.path = exePath;
        km.installedAt = "";
        km.arch = "windows-amd64";
        km.assetName = "";
        km.sha256 = ComputeSha256(exePath);
        m_kernels.push_back(km);
    } while (FindNextFileA(h, &fd));

    FindClose(h);
}

KernelDownloader::KernelDownloader() {}

bool KernelDownloader::DownloadFile(const std::string& url, const std::string& outputPath, std::string* error) {
    HRESULT hr = URLDownloadToFileA(NULL, url.c_str(), outputPath.c_str(), 0, NULL);
    if (FAILED(hr)) {
        if (error) *error = "下载失败，请检查网络或 GitHub 访问。";
        return false;
    }
    return true;
}

bool KernelDownloader::ExtractZip(const std::string& zipPath, const std::string& destination, std::string* error) {
    if (!EnsureDirectoryExists(destination)) {
        if (error) *error = "创建解压目录失败。";
        return false;
    }
    std::string command = "powershell -NoProfile -ExecutionPolicy Bypass -Command \"Expand-Archive -Path " +
        QuoteForPowershell(zipPath) + " -DestinationPath " + QuoteForPowershell(destination) + " -Force\"";
    int rc = system(command.c_str());
    if (rc != 0) {
        if (error) *error = "解压内核压缩包失败。";
        return false;
    }
    return true;
}

bool KernelDownloader::QueryLatestStableRelease(std::string* tag, std::string* assetName, std::string* assetUrl, std::string* error) {
    const std::string tempFile = "mihomo_release_latest.json";
    if (!DownloadFile("https://api.github.com/repos/MetaCubeX/mihomo/releases/latest", tempFile, error)) {
        return false;
    }

    std::ifstream in(tempFile.c_str(), std::ios::in | std::ios::binary);
    if (!in.is_open()) {
        if (error) *error = "读取版本信息失败。";
        DeleteFileA(tempFile.c_str());
        return false;
    }
    std::stringstream ss;
    ss << in.rdbuf();
    const std::string body = ss.str();
    DeleteFileA(tempFile.c_str());

    *tag = JsonValue(body, "tag_name");
    if (tag->empty()) {
        if (error) *error = "解析最新版本号失败。";
        return false;
    }

    size_t pos = 0;
    const std::string nameMarker = "\"name\"";
    const std::string urlMarker = "\"browser_download_url\"";
    while ((pos = body.find(nameMarker, pos)) != std::string::npos) {
        size_t nq1 = body.find('"', body.find(':', pos) + 1);
        size_t nq2 = body.find('"', nq1 + 1);
        std::string name = body.substr(nq1 + 1, nq2 - nq1 - 1);
        if (name.find("windows") != std::string::npos && name.find("amd64") != std::string::npos && name.find(".zip") != std::string::npos) {
            size_t upos = body.find(urlMarker, nq2);
            size_t uq1 = body.find('"', body.find(':', upos) + 1);
            size_t uq2 = body.find('"', uq1 + 1);
            *assetName = name;
            *assetUrl = body.substr(uq1 + 1, uq2 - uq1 - 1);
            return true;
        }
        pos = nq2;
    }

    if (error) *error = "未找到适配 Windows amd64 的内核资产。";
    return false;
}

std::string KernelDownloader::NowIso8601Utc() const {
    time_t now = time(NULL);
    struct tm t;
#if defined(_WIN32)
    gmtime_s(&t, &now);
#else
    gmtime_r(&now, &t);
#endif
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &t);
    return buf;
}

bool KernelDownloader::DownloadLatestStable(const std::string& kernelsDir, KernelMetadata* installedKernel, std::string* error) {
    std::string tag, assetName, assetUrl;
    if (!QueryLatestStableRelease(&tag, &assetName, &assetUrl, error)) {
        return false;
    }

    const std::string kernelId = MakeKernelId(tag, "windows-amd64");
    const std::string installDir = kernelsDir + "\\" + kernelId;
    const std::string downloadZip = kernelsDir + "\\" + kernelId + ".zip";
    const std::string tempExtractDir = kernelsDir + "\\" + kernelId + "-extract";

    if (!DownloadFile(assetUrl, downloadZip, error)) {
        return false;
    }
    if (!ExtractZip(downloadZip, tempExtractDir, error)) {
        DeleteFileA(downloadZip.c_str());
        return false;
    }

    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA((tempExtractDir + "\\*.exe").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) {
        if (error) *error = "解压后未找到 mihomo 可执行文件。";
        DeleteFileA(downloadZip.c_str());
        return false;
    }

    std::string sourceExe = tempExtractDir + "\\" + fd.cFileName;
    FindClose(h);
    EnsureDirectoryExists(installDir);
    std::string targetExe = installDir + "\\mihomo.exe";
    if (!CopyFileAtomic(sourceExe, targetExe)) {
        if (error) *error = "安装内核失败，无法写入 mihomo.exe。";
        return false;
    }
    DeleteFileA(downloadZip.c_str());

    if (installedKernel) {
        installedKernel->id = kernelId;
        installedKernel->version = tag;
        installedKernel->source = "downloaded";
        installedKernel->path = targetExe;
        installedKernel->installedAt = NowIso8601Utc();
        installedKernel->arch = "windows-amd64";
        installedKernel->assetName = assetName;
        installedKernel->sha256 = ComputeSha256(targetExe);
    }
    return true;
}

KernelProcessManager::KernelProcessManager() : m_monitorThread(NULL), m_shouldMonitor(false) {
    ZeroMemory(&m_processInfo, sizeof(m_processInfo));
}

KernelProcessManager::~KernelProcessManager() {
    Stop();
}

bool KernelProcessManager::Start(const std::string& kernelExePath, const std::string& workDir, const std::string& configPath) {
    if (IsRunning()) {
        return true;
    }

    if (!FileExists(kernelExePath) || !FileExists(configPath)) {
        return false;
    }

    std::string command = "\"" + kernelExePath + "\" -d \".\" -f \"" + configPath + "\"";
    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);

    ZeroMemory(&m_processInfo, sizeof(m_processInfo));

    BOOL ok = CreateProcessA(NULL, &command[0], NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, workDir.c_str(), &si, &m_processInfo);
    if (!ok) {
        return false;
    }

    m_lastKernelExe = kernelExePath;
    m_lastWorkDir = workDir;
    m_lastConfig = configPath;
    StartMonitor();
    return true;
}

void KernelProcessManager::Stop() {
    StopMonitor();
    if (m_processInfo.hProcess) {
        TerminateProcess(m_processInfo.hProcess, 0);
        WaitForSingleObject(m_processInfo.hProcess, 3000);
        CloseHandle(m_processInfo.hProcess);
        m_processInfo.hProcess = NULL;
    }
    if (m_processInfo.hThread) {
        CloseHandle(m_processInfo.hThread);
        m_processInfo.hThread = NULL;
    }
}

bool KernelProcessManager::Restart(const std::string& kernelExePath, const std::string& workDir, const std::string& configPath) {
    Stop();
    return Start(kernelExePath, workDir, configPath);
}

bool KernelProcessManager::IsRunning() const {
    if (!m_processInfo.hProcess) {
        return false;
    }
    DWORD code = 0;
    if (!GetExitCodeProcess(m_processInfo.hProcess, &code)) {
        return false;
    }
    return code == STILL_ACTIVE;
}

void KernelProcessManager::StartMonitor() {
    StopMonitor();
    m_shouldMonitor = true;
    unsigned tid = 0;
    m_monitorThread = (HANDLE)_beginthreadex(NULL, 0, MonitorThread, this, 0, &tid);
}

void KernelProcessManager::StopMonitor() {
    m_shouldMonitor = false;
    if (m_monitorThread) {
        WaitForSingleObject(m_monitorThread, 1000);
        CloseHandle(m_monitorThread);
        m_monitorThread = NULL;
    }
}

unsigned __stdcall KernelProcessManager::MonitorThread(void* param) {
    KernelProcessManager* manager = static_cast<KernelProcessManager*>(param);
    while (manager->m_shouldMonitor) {
        if (manager->m_processInfo.hProcess) {
            DWORD wait = WaitForSingleObject(manager->m_processInfo.hProcess, 1000);
            if (wait == WAIT_OBJECT_0 && manager->m_shouldMonitor) {
                manager->Start(manager->m_lastKernelExe, manager->m_lastWorkDir, manager->m_lastConfig);
            }
        } else {
            Sleep(500);
        }
    }
    return 0;
}

MihomoManager::MihomoManager()
    : m_registry("")
    , m_initialized(false) {
    char appdata[MAX_PATH] = {0};
    if (SHGetFolderPathA(NULL, CSIDL_APPDATA, NULL, 0, appdata) == S_OK) {
        m_workDir = std::string(appdata) + "\\ClashMetaX";
    }
    if (m_workDir.empty()) {
        m_workDir = ".\\ClashMetaX";
    }
    m_configPath = m_workDir + "\\config.yaml";
    m_registry = KernelRegistry(m_workDir);
}

MihomoManager::~MihomoManager() {
    Stop();
}

bool MihomoManager::FileExists(const std::string& path) const { return ::FileExists(path); }
bool MihomoManager::EnsureDirectoryExists(const std::string& path) { return ::EnsureDirectoryExists(path); }

bool MihomoManager::EnsureDefaultConfig() {
    if (FileExists(m_configPath)) {
        return true;
    }
    std::ofstream out(m_configPath.c_str(), std::ios::out | std::ios::trunc);
    if (!out.is_open()) {
        return false;
    }
    out << "mixed-port: 7890\n"
        << "allow-lan: false\n"
        << "mode: rule\n"
        << "log-level: info\n"
        << "external-controller: 127.0.0.1:9090\n"
        << "dns:\n"
        << "  enable: true\n"
        << "  nameserver:\n"
        << "    - 223.5.5.5\n"
        << "    - 119.29.29.29\n"
        << "proxies: []\n"
        << "proxy-groups:\n"
        << "  - name: PROXY\n"
        << "    type: select\n"
        << "    proxies:\n"
        << "      - DIRECT\n"
        << "rules:\n"
        << "  - MATCH,PROXY\n"
        << "tun:\n"
        << "  enable: false\n";
    return true;
}

bool MihomoManager::Initialize() {
    if (m_initialized) return true;
    if (!EnsureDirectoryExists(m_workDir)) return false;
    if (!EnsureDefaultConfig()) return false;
    if (!m_registry.Initialize()) return false;

    if (!m_registry.HasKernels()) {
        std::string error;
        if (!DownloadLatestKernel(&error)) {
            printf("ERROR: %s\n", error.c_str());
            return false;
        }
    }

    m_initialized = true;
    return true;
}

bool MihomoManager::Start() {
    KernelMetadata selected = m_registry.GetSelectedKernel();
    if (selected.path.empty()) {
        return false;
    }
    return m_processManager.Start(selected.path, m_workDir, m_configPath);
}

void MihomoManager::Stop() {
    m_processManager.Stop();
}

bool MihomoManager::Restart() {
    KernelMetadata selected = m_registry.GetSelectedKernel();
    return m_processManager.Restart(selected.path, m_workDir, m_configPath);
}

bool MihomoManager::IsRunning() const {
    return m_processManager.IsRunning();
}

std::vector<KernelMetadata> MihomoManager::GetInstalledKernels() const {
    return m_registry.GetInstalledKernels();
}

KernelMetadata MihomoManager::GetCurrentKernel() const {
    return m_registry.GetSelectedKernel();
}

bool MihomoManager::DownloadLatestKernel(std::string* error) {
    KernelMetadata installed;
    if (!m_downloader.DownloadLatestStable(m_registry.GetKernelsDir(), &installed, error)) {
        return false;
    }
    if (!m_registry.AddOrUpdateKernel(installed)) {
        if (error) *error = "写入内核状态失败。";
        return false;
    }
    if (!m_registry.SelectKernel(installed.id)) {
        if (error) *error = "设置当前内核失败。";
        return false;
    }
    return true;
}

bool MihomoManager::SwitchKernel(const std::string& kernelId, std::string* error) {
    KernelMetadata oldKernel = m_registry.GetSelectedKernel();
    if (!m_registry.SelectKernel(kernelId)) {
        if (error) *error = "目标内核不存在或不可执行。";
        return false;
    }

    KernelMetadata now = m_registry.GetSelectedKernel();
    m_processManager.Stop();
    if (!m_processManager.Start(now.path, m_workDir, m_configPath)) {
        m_registry.SelectKernel(oldKernel.id);
        if (!oldKernel.path.empty()) {
            m_processManager.Start(oldKernel.path, m_workDir, m_configPath);
        }
        if (error) *error = "切换后新内核启动失败，已回滚。";
        return false;
    }
    return true;
}

bool MihomoManager::IsTunEnabled() const {
    std::ifstream file(m_configPath.c_str());
    if (!file.is_open()) return false;
    std::string line;
    bool inTun = false;
    while (std::getline(file, line)) {
        if (line.find("tun:") != std::string::npos) {
            inTun = true;
            continue;
        }
        if (inTun && line.find("enable:") != std::string::npos) {
            return line.find("true") != std::string::npos;
        }
    }
    return false;
}

bool MihomoManager::UpdateTunConfig(bool enable) {
    std::ifstream in(m_configPath.c_str());
    if (!in.is_open()) return false;
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line)) lines.push_back(line);
    in.close();

    bool foundTun = false;
    bool updated = false;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (lines[i].find("tun:") != std::string::npos) {
            foundTun = true;
            for (size_t j = i + 1; j < lines.size(); ++j) {
                if (!lines[j].empty() && lines[j][0] != ' ' && lines[j][0] != '\t') break;
                if (lines[j].find("enable:") != std::string::npos) {
                    lines[j] = "  enable: " + std::string(enable ? "true" : "false");
                    updated = true;
                    break;
                }
            }
            break;
        }
    }

    if (!foundTun) {
        lines.push_back("tun:");
        lines.push_back(std::string("  enable: ") + (enable ? "true" : "false"));
        updated = true;
    }

    if (!updated) return false;

    std::ofstream out(m_configPath.c_str(), std::ios::out | std::ios::trunc);
    if (!out.is_open()) return false;
    for (size_t i = 0; i < lines.size(); ++i) out << lines[i] << "\n";
    return true;
}

bool MihomoManager::SetTunEnabled(bool enable) {
    if (IsTunEnabled() == enable) return true;
    if (!UpdateTunConfig(enable)) return false;
    return Restart();
}
