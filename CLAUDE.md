## 项目功能

一个Windows平台的轻量级系统代理控制工具，集成WebUI管理界面和Mihomo代理核心。

## 功能需求

### 核心功能
1. ✅ 能够开启/关闭系统代理
2. ✅ 能够配置系统代理的绕过规则
3. ✅ 能够设置系统代理的监听端口
4. ✅ 开启/关闭系统代理，分别对应不同的任务栏图标颜色，以起到提示作用
5. ✅ 支持开机自启动
6. ✅ 集成 Mihomo 代理核心（v1.19.21）
7. ✅ 自动管理 Mihomo 进程生命周期

### WebUI 功能
8. ✅ 单击任务栏图标自动打开 WebUI（zashboard）
9. ✅ 内置 HTTP 服务器提供静态资源
10. ✅ 所有 WebUI 资源嵌入 exe，真正的单文件应用

## 架构需求

1. ✅ 尽可能的轻量化，使用C++17 + WinAPI
2. ✅ 单文件分发，无需外部依赖
3. ✅ 内置 Mihomo 核心，运行时自动释放

## 实现状态

- ✅ **核心功能**: 全部实现
- ✅ **图形界面**: 设置对话框完整实现
- ✅ **WebUI 集成**: HTTP 服务器 + 嵌入式静态资源
- ✅ **Mihomo 集成**: 完整的进程管理和资源释放
- ✅ **编译环境**: TDM-GCC 10.3.0
- ✅ **文件大小**: 约 46MB（包含 Mihomo + WebUI 静态资源）
- ✅ **编码支持**: GBK编码（中文完美显示）
- ✅ **版本化构建**: 支持版本号目录输出

## 技术栈

- **语言**: C++17
- **API**: WinAPI + Winsock2 + Shell32
- **构建工具**: CMake + TDM-GCC 10.3.0
- **资源管理**: Windows资源文件（.rc）
- **资源类型**: 自定义 "WEBUI" 和 "MIHOMO" 类型
- **资源生成**: Python 3 自动化脚本
- **代理核心**: Mihomo v1.19.21 (Windows amd64-v3)

## 项目结构

```
sys-proxy-bar/
├── src/
│   ├── main.cpp              # 主程序入口
│   ├── proxy_manager.cpp/h   # 系统代理管理
│   ├── tray_icon.cpp/h       # 托盘图标管理
│   ├── settings_dialog.cpp/h # 设置对话框
│   ├── http_server.cpp/h     # HTTP服务器
│   ├── mihomo_manager.cpp/h  # Mihomo进程管理
│   ├── resource.rc           # UI资源文件
│   ├── webui.rc              # WebUI资源（自动生成）
│   ├── webui_resource.h      # WebUI资源映射表（自动生成）
│   ├── mihomo.rc             # Mihomo资源定义
│   └── mihomo_resource.h     # Mihomo资源ID
├── tools/
│   ├── mihomo_embed.py       # Mihomo下载脚本
│   └── mihomo/
│       ├── mihomo.exe        # Mihomo可执行文件
│       └── config.yaml       # 默认配置文件
├── dist/                     # WebUI静态资源（zashboard）
│   ├── index.html
│   └── assets/               # 310个静态文件
├── build.bat                 # Windows构建脚本
├── build_versioned.bat       # 版本化构建脚本
├── generate_resources.py     # 资源文件生成脚本
├── CMakeLists.txt            # CMake配置
└── CLAUDE.md                 # 项目文档
```

## 交互说明

### 任务栏图标操作

- **单击左键**: 打开 WebUI（`http://localhost:12345`）
- **双击左键**: 切换代理状态（开启/关闭）
- **右键点击**: 显示上下文菜单
  - 切换代理
  - 设置...
  - 开机自启动
  - 打开面板
  - 重启 Mihomo
  - 退出

### 任务栏图标状态

- **蓝色图标（IDI_INFORMATION）**: 代理已开启
- **红色图标（IDI_ERROR）**: 代理已关闭

### Mihomo 管理

- **启动时机**: 程序启动时自动启动 Mihomo
- **停止时机**: 程序退出时自动停止 Mihomo
- **重启**: 右键菜单 → "重启 Mihomo"
- **异常监控**: Mihomo 崩溃时自动重启（2秒延迟）
- **运行位置**: `%APPDATA%\SysProxyBar\`

## 编译说明

### 环境要求

1. **编译器**: TDM-GCC 10.3.0
   ```bash
   scoop install tdm-gcc
   ```

2. **Python 3**: 用于生成资源文件和下载 Mihomo
   - 通过 pixi: `pixi run python`
   - 或系统 Python: `python`

### 构建步骤

1. **下载 Mihomo**
   ```bash
   python tools/mihomo_embed.py
   ```
   这将下载 mihomo.exe v1.19.21 到 `tools/mihomo/` 目录。

2. **运行版本化构建**
   ```bash
   build_versioned.bat
   ```

   脚本自动执行以下步骤：
   - 生成 WebUI 资源文件
   - 配置 CMake
   - 编译项目
   - 创建版本化输出目录 `dist/v{VERSION}/`
   - 复制可执行文件

3. **输出文件**
   ```
   dist/v2.2.0/SysProxyBar.exe  (约 46MB)
   ```

### 快速构建

如果不使用版本号目录，直接使用：
```bash
build.bat
```
输出：`build/bin/SysProxyBar.exe`

### 编译选项

- **静态链接**: 所有库静态链接，无运行时依赖
- **字符编码**: GBK（中文界面）
- **资源嵌入**:
  - 310个 WebUI 文件
  - mihomo.exe (35MB)
  - config.yaml (最小配置)
- **资源类型**: 自定义 "WEBUI" 和 "MIHOMO" 类型

## 分发说明

### 单文件分发

✅ **只需分发 `SysProxyBar.exe` 一个文件**

- ✅ 不需要携带 `dist/` 目录
- ✅ 不需要 Mihomo 可执行文件（已嵌入）
- ✅ 不需要任何 DLL 或配置文件
- ✅ 可复制到任何 Windows 计算机直接运行
- ✅ 首次运行自动释放 Mihomo 到 `%APPDATA%\SysProxyBar\`

### 运行要求

- Windows 7 或更高版本
- 无需安装任何运行时库
- 首次运行需要写入权限（用于释放 Mihomo）

## 技术细节

### Mihomo 进程管理

- **进程类**: `MihomoManager` (src/mihomo_manager.cpp/h)
- **释放位置**: `%APPDATA%\SysProxyBar\mihomo.exe`
- **配置文件**: `%APPDATA%\SysProxyBar\config.yaml`
- **启动参数**: `mihomo.exe -d "." -f "config.yaml"`
- **监控方式**: 独立线程使用 `WaitForSingleObject` 监控进程
- **自动重启**: 异常退出（exitCode != 0）时延迟 2 秒自动重启

### HTTP 服务器

- **端口**: 固定使用 12345
- **启动时机**: 点击托盘图标时自动启动
- **错误处理**: 端口冲突时弹窗提示
- **功能**: 提供嵌入的静态资源
- **线程**: 独立线程运行，不阻塞主线程

### 资源嵌入

**WebUI 资源**:
- **资源数量**: 310 个文件
- **资源类型**: "WEBUI"
- **资源 ID**: 1000-1309
- **路径格式**: 相对路径 `../dist/xxx`

**Mihomo 资源**:
- **可执行文件**: IDR_MIHOMO_EXE (5000)
- **配置文件**: IDR_MIHOMO_CONFIG (5001)
- **资源类型**: "MIHOMO"
- **路径格式**: 相对路径 `../tools/mihomo/xxx`

### 自动化构建

- **Mihomo 下载**: `tools/mihomo_embed.py`
  - 自动从 GitHub Releases 下载最新版本
  - 生成最小化 config.yaml
  - 支持进度显示

- **WebUI 资源**: `generate_resources.py`
  - 扫描 `dist/` 目录
  - 生成 `src/webui.rc` 和 `src/webui_resource.h`
  - 自动检测文件变化

- **版本化构建**: `build_versioned.bat`
  - 从 CMakeLists.txt 读取版本号
  - 创建 `dist/v{VERSION}/` 目录
  - 复制编译好的可执行文件

### 默认配置

Mihomo 的默认配置（config.yaml）包含：
- HTTP 代理端口: 7890
- SOCKS 代理端口: 7891
- 外部控制器: 127.0.0.1:9090
- DNS: 阿里 DNS (223.5.5.5) 和腾讯 DNS (119.29.29.29)
- 代理组: 默认 DIRECT
- 规则: MATCH → PROXY

用户可以编辑 `%APPDATA%\SysProxyBar\config.yaml` 来自定义配置。

## 已知问题

- 图标使用系统预设图标（IDI_INFORMATION/IDI_ERROR）
- Mihomo 下载需要网络连接（首次构建时）
- 首次运行需要释放文件（约 35MB），约 1-2 秒
- 端口 7890/9090 如被占用需要手动处理

## 更新日志

### v2.2.0（当前版本）
- ✅ 集成 Mihomo v1.19.21 代理核心
- ✅ 实现完整的进程生命周期管理
- ✅ 添加 Mihomo 异常监控和自动重启
- ✅ 运行时自动释放 Mihomo 到 AppData 目录
- ✅ 托盘菜单添加"重启 Mihomo"选项
- ✅ 版本化构建支持（dist/v{VERSION}/）
- ✅ 文件大小约 46MB（包含 Mihomo）

### v2.1
- ✅ 修复资源类型兼容性问题（从 RT_HTML 改为自定义 WEBUI 类型）
- ✅ 固定 WebUI 端口为 12345
- ✅ 添加端口冲突提示
- ✅ 优化构建脚本

### v2.0
- ✅ 新增单击托盘图标打开 WebUI
- ✅ 集成 HTTP 服务器
- ✅ 嵌入 310 个 WebUI 静态资源
- ✅ 实现真正的单文件分发

### v1.0
- ✅ 基础系统代理控制功能
- ✅ 设置对话框
- ✅ 开机自启动
