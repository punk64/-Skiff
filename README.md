# 轻舟 Skiff - 鸿蒙手机端 SSH 工具

[![Version](https://img.shields.io/badge/version-0.2.1-blue.svg)](CHANGELOG.md)
[![HarmonyOS Compatibility](https://img.shields.io/badge/HarmonyOS-6.1%20(API%2024)-blue?logo=OpenHarmony)](https://www.openharmony.cn/)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)

**独立社区分支** · 基于 [CrossShellNext](https://gitcode.com/OpenHarmonyPCDeveloper/CrossShellNext) 分叉，专注**手机形态**的 SSH 连接工具。

## 📖 这是什么

轻舟（Skiff）是一种轻快的小船——我们希望它也是：装在口袋里的远程运维工具，打开即连、随手转发。

上游 CrossShellNext 是一款面向 OpenHarmony 桌面形态的 SSH 客户端。本分支将其改造为**纯手机端**应用：导航、终端、文件与隧道全部按手机竖屏重新设计；桌面式页面原样保留在代码里、不再作为启动入口，需要时可切回对照。

建连、终端、转发、传输全部走真实内核，无演示数据、无界面占位。

## ✨ 特性

### 连接与认证

- **主机列表即首页**：启动落到列表、默认不自动连接；标题行右侧齿轮进应用设置、加号新建主机；卡片显示状态灯、名称与实时 CPU / RAM / 网络占用（每 5 秒采集，未连接时灰显 `--`）
- **卡片三入口**：每张卡片上「终端 / 隧道 / 文件」三个快捷按钮，点击即连接该主机并直达对应界面；未连接时按钮锁定置灰
- **认证方式**：密码（掩码输入）与私钥（RSA 2048 / ECDSA P-256 / Ed25519，可在应用内生成并选用）
- **主机指纹校验**：首次连接记录服务器指纹（TOFU），之后每次连接在认证前比对，指纹变化直接拒绝并在提示中给出实际指纹；修改地址或端口时自动清空旧指纹
- **多会话并存**：不同主机可同时保持连接，互不干扰
- **连接可选代理**：支持 SOCKS5 与 HTTP 代理，连接前可测试连通性

### 终端

- xterm.js 渲染真实 PTY 输出，内置 Cascadia Mono 等宽字体，字号 11~18 可调
- **六行扩展键盘**：字母、数字符号、控制键、方向与编辑键分组，`Shift` / `Caps` 独立生效，`Ctrl` / `Alt` 组合键直通，`Del`、`⌫` 分别发送对应控制序列
- 一屏三页互切：进入会话后顶部段控在终端 / 隧道 / 文件间一键切换，会话栏常驻主机名与断开
- 输出按帧合并渲染（16ms / 32KB 阈值），长时间刷屏不掉帧

### 端口转发

- **本地（L）/ 远程（R）/ 动态（D）** 三种类型独立表单，实时生成等效 `ssh -L/-R/-D` 命令行供核对
- 开关真实启停转发：失败回滚为关闭并说明原因，主机未连接时置灰
- **每 15 秒与内核对账**：转发会话已失效时开关自动回落，不再「假开启」
- L 型转发可一键跳转**内置浏览页**，打开前先探测本机端口，未就绪时提示而不是白屏

### 浏览页

- 占满会话区的 ArkWeb，顶栏图标化工具栏（关闭 / 后退 / 前进 / 刷新）
- 加载失败给出原因与重试入口，加载超时会显诊断浮层而非一片黑

### 文件（SFTP）

- 目录浏览、面包屑、多选、新建目录、重命名、删除（文件夹递归）
- 上传与下载带传输队列（进度 / 暂停 / 继续 / 取消），失败时透传内核原始错误说明

### 日志与诊断

- **会话日志**：按会话独立落盘，可查看、导出、清除，自动保留最近 19 个文件
- **应用日志**：连接、转发、异常与崩溃信息按天写入沙箱，与会话日志分栏查看；预览显示最新记录，超上限时标注仅显示末尾
- **全局错误观测**：未捕获异常与崩溃原因落盘，便于事后定位

### 外观与语言

- 明暗双主题，切换即时生效（不重建页面）
- 中英双语界面，默认中文，设置内即时切换、重启后保持
- 设置项持久化：主机、隧道、主题、语言、字号、连接行为重启后全部保留

## ⚠️ 当前边界

- **未签名**：构建产物均为未签名包，只能安装到调试设备；正式分发需自行签名
- **认证方式有限**：仅支持密码与公钥，不支持 keyboard-interactive，需要两步验证（2FA）的服务器无法登录
- **后台常驻未充分验证**：「后台保持隧道」选项已实现，真机长时间后台存活仍需实测
- **「保活」开关暂不生效**：关掉后内核仍按默认 60 秒发探测包（`0` 被当成「用默认值」），开关的实际作用是切换到应用层心跳兜底；要真正禁用需改内核参数语义
- **转发类型验证不均**：本地（L）转发与内置浏览页已联机验证；远程（R）与动态（D）转发仅完成实现，未单独联机验证
- 桌面形态页面保留在代码中但不在导航路径上，未随手机端改造同步维护

## 🚀 快速开始

### 环境要求

- DevEco Studio（SDK 对齐 API 24 / HarmonyOS 6.1），`DEVECO_HOME` 指向安装目录
- HarmonyOS 模拟器（x86_64）或真机（arm64-v8a）
- 构建缓存请指向非系统盘（`HVIGOR_USER_HOME`），避免占满 C 盘

### 构建与出包

推荐用 `release.py`，它一次完成版本推进、构建、摘除未配置的签名项、归集产物与 SHA256：

```bash
git clone <本仓库地址>
cd Skiff
```

```bash
python release.py --debug          # debug 构建（同样推进版本号并归集产物）
python release.py                  # release 包（arm64-v8a）
python release.py --abi both       # 双架构
python release.py --no-bump        # 不推进版本号，其余流程照旧
python release.py --dry-run        # 只打印将要执行的步骤
```

Windows 下可直接双击 `release.bat`。

产物：

- 出包产物归集到仓库外的 `产出/`（含 `.sha256`），构建记录写入项目文档 `构建记录.md`
- 中间产物在 `entry/build/default/outputs/default/entry-default-unsigned.hap`

安装到设备：

```bash
hdc install entry/build/default/outputs/default/entry-default-unsigned.hap
```

也可以直接用 DevEco Studio 打开工程构建，等价于
`hvigorw assembleHap --mode module -p product=default -p buildMode=debug --no-daemon`（`hvigorw` 来自 DevEco Studio 工具链，不在仓库内）。

### 原生依赖

C++ 内核基于 **libssh2 1.11.1 + OpenSSL 1.1.1w** 静态链接，仓库自带 `entry/libs/arm64-v8a/` 与 `entry/libs/x86_64/` 两套静态库，构建脚本按 `OHOS_ARCH` 自动选择。交叉编译方法与踩坑记录见项目文档 `文档指南/`（该项目文档目录不随仓库分发）。

## 🏗️ 架构

```
ArkTS 界面层   pages/mobile/*  主机列表 / 终端 / 隧道 / 文件 / 浏览 / 设置
      ↓         common/  SshService · KeyStore · Store · AppLog · PortProbe
NAPI 桥接层    entry/src/main/cpp/napi*.cpp
      ↓
C++ 内核层     libssh2（会话 / 通道 / SFTP） + 转发线程 + keepalive 线程
```

主机与设置统一走 preferences 单文件持久化（`skiff_store`），不依赖 `PersistentStorage`。

## 🗺️ 里程碑

| 阶段 | 内容 | 状态 |
|---|---|---|
| P0 | 真实建连 + 真实终端（会话串行门 → 状态回显） | 完成 |
| P1 | 隧道 L / R / D 真实转发 + 内置浏览页 | 完成 |
| P2 | SFTP 真实传输（含队列与中断） | 完成 |
| P3 | 持久化 + 密钥库 + 主机指纹 | 完成 |
| P4 | 会话日志 + 应用日志 + 断线自动重连 | 完成 |
| P5 | 双架构（arm64-v8a + x86_64）打包 | 完成 |
| P6 | 状态真实性收口：全局心跳、隧道对账、回前台探活、假功能清理 | 完成 |

每一轮改动的细节、根因与验证记录见项目文档 `文档指南/开发记录.md`（不随仓库分发）。

## 🤝 参与

欢迎 Issue 与 PR。提交前请注意：

- 遵循现有代码风格（ArkTS 零注释约定、C++17）
- 界面改动请在模拟器或真机上实际操作验证，不以截图代替
- 涉及转发 / 协议行为时，先读 `文档指南/方案/移动端开发总方案.md` 中的内核硬约束（建连与断开互斥、会话可并存、SFTP 取消是会话级、exec 通道须持锁等）

## 📜 许可与致谢

本项目基于 [MIT License](LICENSE) 开源。

- 上游：[CrossShellNext](https://gitcode.com/OpenHarmonyPCDeveloper/CrossShellNext) —— SSH / Telnet / SFTP 协议内核与桌面端实现
- 终端字体：[Cascadia Mono](https://github.com/microsoft/cascadia-code)（MIT）
- 终端渲染：[xterm.js](https://xtermjs.org/)（MIT）

---

**轻舟 Skiff** —— 口袋里的远程运维，轻舟已备，只待扬帆。
