# SysProxyBar

一个轻量级的 Windows 系统代理控制工具，集成 Mihomo 代理核心和 WebUI 管理界面。

## 功能特性

### 核心功能
- ✅ 快速开启/关闭系统代理
- ✅ 图形化配置绕过列表
- ✅ 开机自启动支持
- ✅ 内置 Mihomo 代理核心
- ✅ 内置 Zashboard 面板

## 界面预览

### 托盘菜单
```
切换代理
────────
设置...
开机自启动
────────
打开面板
重启 Mihomo
打开配置文件
────────
退出
```

## 系统要求

- Windows 7 或更高版本
- Python 3（用于构建时生成资源）
- TDM-GCC 10.3.0（用于编译）
- CMake 3.15 或更高版本

## 快速开始

### 编译项目

**1. 安装编译环境**
```powershell
# 使用 Scoop 安装 TDM-GCC
scoop install tdm-gcc

# 安装 Pixi（包含 Python）
scoop install pixi
```

**2. 下载 Mihomo 核心**
```bash
python tools/mihomo_embed.py
```
这会下载 mihomo.exe v1.19.21 到 `tools/mihomo/` 目录。

**3. 一键构建**
```bash
build.bat
```

**输出**：`release\v2.3\SysProxyBar.exe` (约 45MB)

### 运行程序

双击 `SysProxyBar.exe`，程序会：
1. 自动释放 mihomo.exe 到 `%APPDATA%\SysProxyBar\`
2. 启动 Mihomo 代理核心
3. 在任务栏显示托盘图标

## 项目结构

```
sys-proxy-bar/
├── src/
│   ├── main.cpp              # 主程序入口
│   ├── proxy_manager.*       # 代理管理器（注册表操作）
│   ├── tray_icon.*           # 托盘图标管理
│   ├── settings_dialog.*     # 设置对话框
│   ├── http_server.*         # HTTP 服务器（WebUI）
│   ├── mihomo_manager.*      # Mihomo 进程管理
│   ├── resource.h            # 资源定义
│   ├── resource.rc           # 资源文件（对话框、图标）
│   ├── webui.rc              # WebUI 资源（自动生成）
│   └── mihomo.rc             # Mihomo 资源
├── tools/
│   ├── mihomo_embed.py       # Mihomo 下载脚本
│   └── mihomo/
│       ├── mihomo.exe        # Mihomo 可执行文件
│       ├── config.yaml       # 默认配置文件
│       └── mihomo.manifest   # 构建期生成的校验清单
├── dist/                     # WebUI 静态资源（Zashboard）
│   ├── index.html
│   └── assets/
├── build.bat                 # 一键构建脚本
├── generate_resources.py     # 资源文件生成脚本
├── CMakeLists.txt            # CMake 配置
└── README.md
```

## 技术实现

- **语言**: C++17
- **API**: WinAPI + Winsock2 + Shell32
- **构建工具**: CMake + TDM-GCC 10.3.0
- **字符编码**: GBK（中文支持）
- **资源管理**: Windows 资源文件（.rc）
- **进程管理**: CreateProcess + 监控线程

## 常见问题

**Q: 中文显示乱码？**
A: 在 CMakeLists.txt 中配置 `-fexec-charset=GBK`

**Q: 程序无法启动？**
A: 确保 Windows 7 或更高版本，并且没有被安全软件拦截

**Q: 代理不生效？**
A: 检查 Mihomo 是否正常运行，配置是否正确

**Q: 如何更新 Mihomo 版本？**
A: 更新 `tools/mihomo/` 目录下的文件，重新编译运行即可

**Q: 能否修改端口？**
A: 可以编辑 `%APPDATA%\SysProxyBar\config.yaml` 文件

**Q: 可以同时运行多个实例吗？**
A: 不可以，程序有单实例限制

**Q: 开机自启动指向旧版本？**
A: 重新启用开机自启动会自动更新到当前版本路径

## 构建选项

**版本化构建**：
```bash
build.bat
# 输出到 release/v{VERSION}/SysProxyBar.exe
```

**编译选项**：
- 静态链接所有库
- 字符编码：GBK
- 嵌入所有资源（WebUI + Mihomo）
- 自定义托盘图标

## 自定义托盘图标

1. 准备两个 `.ico` 格式图标文件：
   - `proxy-on.ico`（代理开启，建议绿色/蓝色）
   - `proxy-off.ico`（代理关闭，建议灰色/红色）
2. 放到 `src/res/icons/` 目录
3. 重新编译

图标规格建议：
- 尺寸：16x16 或 32x32 像素
- 格式：.ico（可包含多个尺寸）
- 支持透明背景

## 致谢

- [Mihomo](https://github.com/MetaCubeX/mihomo) - 核心代理引擎
- [Zashboard](https://github.com/Zephyruso/zashboard) - WebUI 管理界面
