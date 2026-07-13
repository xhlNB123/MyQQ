# MyQQ —— 简易 QQ 聊天工具

武汉大学《程序设计实践》阶段项目。C/S 架构，C++/MFC 实现，**局域网（LAN）内通信**。需求与工作流见 [项目开发计划.md](项目开发计划.md)，通信协议见 [docs/02_设计文档/通信协议设计.md](docs/02_设计文档/通信协议设计.md)。

当前进度：服务端已接入 **SQLite** 数据库，注册 / 登录 / 好友管理 / 消息持久化 / **在线实时转发** 全部跑通并通过端到端联调。

## 三分钟快速自测（单机双开）

在一台机器上就能验证全部核心功能，无需第二台电脑：

1. **启动服务端**：编译后运行 `build\MyQQServer.exe`，看到 `MyQQ 服务端启动，端口 6000` 即成功。
2. **开第一个客户端**（用户 A）：运行 `MyQQClient.exe` → 填服务器 IP `127.0.0.1`、端口 `6000` → 点「注册」建账号 `alice`（记住 UserId，如 10001）→ 用 alice 登录。
3. **开第二个客户端**（用户 B）：再运行一个 `MyQQClient.exe` → 同样连 `127.0.0.1:6000` → 注册 `bob`（记住 UserId，如 10002）→ 用 bob 登录。
4. **加好友**：在 alice 窗口的查找框输入 `bob` → 点「查找/添加」→ 确认添加 → 双方点「刷新」应能在好友列表看到对方。
5. **实时聊天**：alice 选中 bob 打开聊天窗口发消息 → bob 侧应**立即弹出/显示**该消息 → bob 回复，alice 侧同样实时收到。
6. **验证持久化与日志**：关闭重开客户端，历史消息仍在数据库；程序目录下 `logs\` 会生成聊天记录 `.txt` 文件。
7. **版本查询**：在登录窗或聊天窗**右键** → 「查询软件版本」应弹出 `MyQQ v1.0.0`。

> 详细测试步骤、双机联调、常见问题见 [docs/服务端说明与测试指南.md](docs/服务端说明与测试指南.md) 与 [docs/客户端使用说明书.md](docs/客户端使用说明书.md)。

## 工程结构
```
myQQ/
├─ README.md                 本文件
├─ 项目开发计划.md            需求总结 + 工程工作流
├─ CMakeLists.txt            构建 common/服务端/控制台客户端
├─ .gitignore
│
├─ database/                 数据库脚本
│   ├─ schema.sql            SQL Server 版建表（课程要求参考）
│   ├─ schema_sqlite.sql     SQLite 版建表（服务端实际使用）
│   └─ seed.sql              字典表初始化数据
│
├─ third_party/sqlite/       SQLite amalgamation（sqlite3.c/.h）
│
├─ src/
│   ├─ common/               客户端与服务端共用
│   │   ├─ Protocol.h        通信协议：命令字、端口、版本号
│   │   ├─ Message.h/.cpp    消息打包/解包
│   │   ├─ Socket.h/.cpp     Winsock TCP 封装 + 本机 IP 查询
│   │   └─ ChatLogger.h/.cpp 聊天记录写文件
│   │
│   ├─ server/
│   │   ├─ ServerMain.cpp    控制台服务端（协议分发 + 业务逻辑）
│   │   ├─ Database.h/.cpp   SQLite 数据访问层
│   │   └─ SessionManager.h/.cpp  在线用户会话管理（消息转发）
│   │
│   └─ client/
│       ├─ README.md         MFC 客户端类划分说明
│       └─ ConsoleClient.cpp 控制台测试客户端（含接收线程）
│
├─ resources/                头像等资源
│   └─ avatars/
│
├─ docs/                     阶段文档（对应瀑布模型各阶段交付物）
│   ├─ 01_需求分析/
│   ├─ 02_设计文档/
│   ├─ 03_测试报告/
│   ├─ 04_设计报告/
│   └─ 流程图/
│
└─ build/                    构建输出（git 忽略）
```

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
