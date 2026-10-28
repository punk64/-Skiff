# 轻舟 Skiff - 鸿蒙手机端 SSH 工具

[![Version](https://img.shields.io/badge/version-0.1.x%20dev-blue.svg)](CHANGELOG.md)
[![HarmonyOS Compatibility](https://img.shields.io/badge/HarmonyOS-6.1%20(API%2024)-blue?logo=OpenHarmony)](https://www.openharmony.cn/)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)

**独立社区分支** · 基于 [CrossShellNext](https://gitcode.com/OpenHarmonyPCDeveloper/CrossShellNext) 分叉，专注**手机形态**的 SSH 连接工具。

## 📖 这是什么

轻舟（Skiff）是一种轻快的小船——我们希望它也是：装在口袋里的远程运维工具，打开即连、随手转发。

上游 CrossShellNext 是一款面向 OpenHarmony 桌面形态的 SSH 客户端。本分支将其改造为**纯手机端**应用：导航、终端、文件与隧道全部按手机竖屏重新设计；桌面式页面原样保留在代码里、不再作为启动入口，需要时可切回对照。

## ✨ 特性

### 已具备（界面与交互）

- **主机列表即首页**：启动落到列表、默认不自动连接；标题行右侧齿轮进应用设置、加号新建主机；卡片显示状态灯、名称与 CPU / RAM / 带宽占用（未连接时灰显 `--`），连接 / 断开按钮直接切换
- **卡片三入口**：每张卡片上「终端 / 隧道 / 文件」三个快捷按钮，点击即连接该主机并直达对应界面；未连接时按钮锁定置灰
- **一屏三页互切**：进入会话后顶部段控在终端 / 隧道 / 文件间一键切换，会话栏常驻主机名与断开，无需退回列表
- **左滑操作**：卡片向右收拢，右侧露出「设置」与「删除」两个独立小方块按钮，删除逻辑只在此处触发（主机设置页不再重复提供）
- **主机管理**：搜索过滤、密码 / 私钥认证切换、多会话并存
- **端口转发**：本地（L）/ 远程（R）/ 动态（D）三种类型独立表单，实时生成等效 `ssh -L/-R/-D` 命令行供核对；L 型转发可一键跳转**内置浏览页**访问目标服务
- **浏览页**：占满会话区的 ArkWeb，顶栏图标化工具栏（关闭 / 后退 / 前进 / 刷新），连接失败给出原因与重试
- **终端**：xterm.js 渲染，内置 Cascadia Mono 等宽字体，字号可调，底部修饰键条
- **文件（SFTP 界面）**：目录浏览、面包屑、多选、文件夹递归打包、传输队列（进度 / 暂停 / 继续）
- **其他**：密钥生成（RSA / ECDSA / Ed25519）、会话日志、明暗双主题、**中英双语界面**（默认中文，设置内即时切换、重启后保持）

### 当前边界（内核接入中）

> 本分支正处于「界面先行」阶段，以下能力**尚未接入真实内核**，目前为演示数据：

- 点击主机不会真正建立 SSH 连接
- 隧道开关只改界面状态，不发起真实端口转发
- 终端是本地命令模拟，文件页读取示例目录
- 数据不持久化：主机、隧道等业务数据重启后回到示例数据（仅主题与界面语言会保存）
- 仅在 **x86_64 模拟器**验证；真机（arm64）需补编静态库并切双架构打包
- 应用未签名，只能安装到调试设备

内核接入按 P0~P6 路线推进（见下文路线图），每一步的验收标准在开发方案中明确。

## 🚀 快速开始

### 环境要求

- DevEco Studio（SDK 对齐 API 24 / HarmonyOS 6.1）
- HarmonyOS 模拟器或真机（x86_64 模拟器开箱即用，arm64 真机需自行补编静态库）

### 构建

```bash
git clone <本仓库地址>
cd Skiff
hvigorw assembleHap --mode module -p product=default -p buildMode=debug --no-daemon
```

产物：`entry/build/default/outputs/default/entry-default-unsigned.hap`

```bash
hdc install entry/build/default/outputs/default/entry-default-unsigned.hap
```

### 原生依赖

C++ 内核基于 **libssh2 1.11.1 + OpenSSL 1.1.1w** 静态链接。仓库自带 `entry/libs/x86_64/` 静态库（自编，见上游 arm64 版），构建脚本按 `OHOS_ARCH` 自动选择库路径；交叉编译方法与踩坑记录见 `文档指南/`。

## 🗺️ 路线图

| 阶段 | 内容 | 状态 |
|---|---|---|
| P0 | 建连 + 真实终端（SshService 串行门 → 真实状态回显） | 待开始 |
| P1 | 隧道真实转发 | 待开始 |
| P2 | SFTP 真实传输 | 待开始 |
| P3 | 持久化 / 密钥 | 待开始 |
| P4 | 日志 / 重连 | 待开始 |
| P5 | 真机适配（arm64 双架构 + 签名） | 待开始 |

## 🤝 参与

欢迎 Issue 与 PR。提交前请注意：

- 遵循现有代码风格（ArkTS 零注释约定、C++17）
- 界面改动请在 x86_64 模拟器上验证并附录屏或截图
- 涉及转发 / 协议行为时，先读 `文档指南/方案/移动端开发总方案.md` 中的内核硬约束（单连接锁、无主机密钥校验、SFTP 会话级取消等）

## 📜 许可与致谢

本项目基于 [MIT License](LICENSE) 开源。

- 上游：[CrossShellNext](https://gitcode.com/OpenHarmonyPCDeveloper/CrossShellNext) —— SSH / Telnet / SFTP 协议内核与桌面端实现
- 终端字体：[Cascadia Mono](https://github.com/microsoft/cascadia-code)（MIT）
- 终端渲染：[xterm.js](https://xtermjs.org/)（MIT）

---

**轻舟 Skiff** —— 口袋里的远程运维，轻舟已备，只待扬帆。
