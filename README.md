# MyQQ —— 简易 QQ 聊天工具

武汉大学《程序设计实践》阶段项目。C/S 架构，C++/MFC 实现，**局域网（LAN）内通信**。需求与工作流见 [项目开发计划.md](项目开发计划.md)，通信协议见 [docs/02_设计文档/通信协议设计.md](docs/02_设计文档/通信协议设计.md)。

当前进度：注册 / 登录 / 好友申请 / 一对一聊天（文本+图片+文件）/ 消息历史 / 个人信息 / **群聊（建群、加群号、邀请、审批、群消息/图片/文件）** / **查看他人资料（含隐私设置）** / 切换账号 全部完成。服务端 SQLite 持久化 + 在线转发/群发，所有协议已通过控制台端到端联调；MFC 客户端各功能窗体齐备（GUI 交互需实机验证）。

## 发布给其他人

不要发送 `Debug` 客户端或早期 MinGW 服务端。运行以下脚本生成无需 Visual Studio/MFC/MinGW 的 Windows x64 Release 压缩包：

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\package-windows-x64.ps1
```

最终文件位于 `dist/MyQQ-1.0.0-windows-x64.zip`。接收方完整解压后，先运行 `server/Start-Server.cmd`，再运行 `client/MyQQClient.exe`，**不需要打开 `.sln`**。详见 [部署与发布说明](docs/部署与发布说明.md)。

## 三分钟快速自测（源码开发环境）

在一台机器上就能验证全部核心功能，无需第二台电脑：

1. **启动服务端**：在项目根目录运行 `build\MyQQServer.exe`，看到 `[server] 正在监听端口 6000` 即成功（这个窗口别关）。
2. **开第一个客户端**（用户 A）：运行 `MyQQClient.exe` → 填服务器 IP `127.0.0.1`、端口 `6000` → 点「注册」建账号 `alice`（账号是登录名，注册成功后系统再分配一个 QQ 号）→ 用 alice 登录。
3. **开第二个客户端**（用户 B）：再运行一个 `MyQQClient.exe` → 同样连 `127.0.0.1:6000` → 注册 `bob` → 用 bob 登录。
4. **加好友**：在 alice 窗口的查找框输入 `bob` → 点「查找/添加」→ 确认添加 → 双方点「刷新」应能在好友列表看到对方。
5. **实时聊天**：alice 选中 bob 打开聊天窗口发消息 → bob 侧应**立即显示**该消息 → bob 回复，alice 侧同样实时收到。
6. **个人设置**：主窗口点「个人设置」→ 修改昵称 / 性别 / 星座 / 血型 / 签名 / 头像 → 保存 → 重开确认已持久化。
7. **验证持久化与日志**：关闭重开客户端，历史消息仍在数据库；程序目录下 `logs\` 会生成聊天记录 `.txt` 文件。
8. **版本查询**：在登录窗或聊天窗**右键** → 「查询软件版本」应弹出 `MyQQ v1.0.0`。

> 详细测试步骤、双机联调、常见问题见 [docs/服务端说明与测试指南.md](docs/服务端说明与测试指南.md) 与 [docs/客户端使用说明书.md](docs/客户端使用说明书.md)。

## 工程结构
```
myQQ/
├─ README.md                 本文件
├─ 项目开发计划.md            需求总结 + 工程工作流
├─ CMakeLists.txt            构建 common/服务端/控制台客户端（MSVC 静态运行库）
├─ .gitignore
│
├─ database/                 数据库脚本
│   ├─ schema.sql            SQL Server 版建表（课程要求参考）
│   ├─ schema_sqlite.sql     SQLite 版建表（服务端实际使用，含好友申请表）
│   └─ seed.sql              字典表初始化数据
│
├─ third_party/sqlite/       SQLite amalgamation（sqlite3.c/.h）
│
├─ src/
│   ├─ common/               客户端与服务端共用
│   │   ├─ Protocol.h        通信协议：命令字、端口、版本号
│   │   ├─ Message.h/.cpp    消息打包/解包 + Base64URL 编解码
│   │   ├─ Socket.h/.cpp     Winsock TCP 封装 + 本机 IP 查询
│   │   ├─ PathUtils.h       可执行文件目录定位（数据/日志相对 exe）
│   │   └─ ChatLogger.h/.cpp 聊天记录本地备份
│   │
│   ├─ server/               控制台服务端
│   │   ├─ ServerMain.cpp    协议分发 + 业务逻辑
│   │   ├─ Database.h/.cpp   SQLite 数据访问层
│   │   └─ SessionManager.*  在线会话管理与消息转发
│   │
│   └─ client/
│       ├─ README.md         MFC 客户端说明
│       ├─ ConsoleClient.cpp 控制台测试客户端（协议联调）
│       └─ MyQQClient/       MFC GUI 工程（.sln 在此）
│           ├─ MyQQClient.sln / .vcxproj / .rc / resource.h
│           ├─ MyQQClientApp / pch
│           ├─ LoginDlg / RegisterDlg / MainDlg / ChatDlg
│           ├─ ProfileDlg（个性化设置）/ SettingsDlg（设置）
│           ├─ VerifyDlg（好友验证）/ FriendRequestsDlg（好友申请）
│           ├─ NetClient（接收线程 + 消息队列）
│           └─ AppContext（跨窗体共享连接与登录态）
│
├─ scripts/                  发布打包
│   ├─ package-windows-x64.ps1   一键生成自包含 Release zip
│   └─ package-assets/           启动脚本 + 部署说明
│
├─ docs/                     文档
│   ├─ 02_设计文档/通信协议设计.md
│   ├─ 客户端使用说明书.md
│   ├─ 服务端说明与测试指南.md
│   ├─ 部署与发布说明.md
│   └─ 课程原始材料/          课程 PPT / PDF
│
└─ resources/                预留资源目录（头像等）
```

> 说明：`build/`、`out/`、`dist/`、`.vs/`、`x64/`、`myqq.db`、`logs/` 均为构建/运行产物，已在 `.gitignore` 忽略，克隆后重新编译即可生成。

## 快速开始（服务端 + 控制台客户端）
```bash
cd build
cmake .. -G "Visual Studio 17 2022"
cmake --build . --config Debug

# 终端 1：启动服务端（首次运行自动建库 myqq.db 并导入 schema_sqlite.sql）
Debug/MyQQServer.exe

# 终端 2：启动测试客户端并联调协议
Debug/MyQQConsoleClient.exe 127.0.0.1
# 依次输入：
#   REGISTER|alice|123456|Alice
#   LOGIN|alice|123456
```

服务端已接入 SQLite，注册/登录/加好友/查找/聊天均为真实业务逻辑并持久化；
在线用户间的消息由服务端实时转发（`CHAT_PUSH`）。协议详见
[docs/02_设计文档/通信协议设计.md](docs/02_设计文档/通信协议设计.md)。

## 局域网使用
仅需局域网互通，无需公网 IP / 端口映射：
1. 服务端机器查看内网 IP（`ipconfig`，如 `192.168.1.20`），确保防火墙放行端口 `6000`。
2. 运行 `MyQQServer.exe`。
3. 同一局域网的客户端用该 IP 连接：`MyQQConsoleClient.exe 192.168.1.20`。

## 开发路线
按 `项目开发计划.md` 的阶段推进：
1. 项目准备：建库建表、框架搭建 ✅（脚本、骨架、SQLite 已就绪）
2. 注册/登录 ✅ → 3. 好友与连接 ✅ → 4. 完整聊天 ✅（含实时转发）→ 5. 个人信息与完善（进行中）
3. MFC GUI 客户端按 `src/client/README.md` 在 VS 中落地，复用 `common/` 协议层。
