# ClashMetaForWindows (ClashMetaX)

ClashMetaX 是基于 SysProxyBar fork 改造的轻量级 Win32 托盘代理工具：保留原有轻量形态，同时把 Mihomo 从“硬编码内核”升级为“可下载、可选择、可切换”的本地内核管理机制。

## 本次阶段目标（已落地）

- 应用运行时标识统一为 **ClashMetaX**。
- Mihomo 运行路径改为由本地 `kernel-state.json` + `kernels/` 目录决定。
- 首次启动若无内核：自动下载 **Mihomo 最新稳定版**（Windows amd64 zip）并安装。
- 支持已安装内核切换，支持手动“下载最新稳定版内核”。
- 新增 CI 与 Pre-Release 自动发布工作流。
- 应用/托盘图标替换为开源 Shield 风格图标（enabled/disabled）。

## 内核目录与状态文件

应用工作目录：

```text
%APPDATA%\ClashMetaX\
```

核心目录结构：

```text
%APPDATA%\ClashMetaX\
├─ config.yaml
├─ kernels\
│  └─ <kernel-id>\
│     └─ mihomo.exe
└─ state\
   └─ kernel-state.json
```

`kernel-state.json` 记录：

- `selected_kernel_id`
- `selected_version`
- `installed_kernels`（含 source/path/installed_at/arch/asset_name/sha256）

## 首次启动行为

1. 启动时扫描 `%APPDATA%\ClashMetaX\kernels\`。
2. 若无已安装内核，则自动访问 Mihomo Releases 最新稳定版并下载 Windows 资产。
3. 解压安装后写入 `kernel-state.json` 并设为当前内核。
4. 若下载失败，弹窗提示并允许稍后重试（不会强制崩溃退出）。

## 托盘菜单（内核）

在原托盘菜单基础上新增“内核”子菜单：

- 当前内核：`<version>`
- 已安装内核列表（可直接切换）
- 下载最新稳定版内核
- 打开内核目录

## 构建

```bat
build.bat
```

输出（示例）：

```text
release\v3.2.0\ClashMetaX.exe
```

## CI / Pre-Release

### CI (`.github/workflows/ci.yml`)

触发：

- pull_request
- push 到 main

内容：

- Windows runner 构建
- 上传 `ClashMetaX.exe` artifact

### Pre-Release (`.github/workflows/prerelease.yml`)

触发：

- `ci` workflow 成功后 (`workflow_run`)
- 且分支为 `main`

内容：

- 重新构建发布产物
- 自动创建/更新 Pre-Release
- 名称格式：`ClashMetaX Pre-Release <yyyyMMdd> <shortsha>`
- 上传 `ClashMetaX.exe` 与 `ClashMetaX.zip`

## 致谢

- [Mihomo](https://github.com/MetaCubeX/mihomo)
- [Zashboard](https://github.com/Zephyruso/zashboard)
