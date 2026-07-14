# MyQQ 客户端（MFC）说明

客户端为 **MFC 基于对话框的应用程序**。`MyQQClient/` 目录下已提供**可直接在 Visual Studio 打开编译的完整工程**（含 `.sln` / `.vcxproj` / `.rc` / 各对话框源码），复用 `src/common/` 的协议与网络层。

## 编译运行
1. 用 **Visual Studio 2022**（安装「使用 C++ 的桌面开发」+「适用于最新 v143 生成工具的 C++ MFC」组件）打开 `MyQQClient/MyQQClient.sln`。
2. 本机开发调试可选 `Debug|x64`；发给其他人必须选 `Release|x64`。工程已把 `../../common/` 下的 `Message.cpp / Socket.cpp / ChatLogger.cpp` 一并纳入编译。
3. 先启动服务端，再运行客户端：登录框输入服务端**局域网 IP** 和端口 `6000` → 注册 → 登录 → 查找加好友 → 双击好友打开聊天。

> **不要直接发送 Debug exe。** Debug 客户端依赖 `mfc140ud.dll`、`ucrtbased.dll` 等开发机调试 DLL，只会在装有 Visual Studio 的电脑正常运行。正式发布请从仓库根目录运行 `scripts/package-windows-x64.ps1`，使用脚本生成的 `dist/*.zip`；最终用户无需 Visual Studio，也无需打开 `.sln`。详见 `docs/部署与发布说明.md`。

## 已实现的窗体
- `CLoginDlg` 登录（含「查询本机主机名/IP」「输入服务端 IP+端口连接」「右键查版本」）
- `CRegisterDlg` 注册
- `CMainDlg` 主窗口（好友列表 CListCtrl、查找/添加、进入聊天、登出）
- `CChatDlg` 聊天窗口（非模态可多开、收发消息、聊天记录写文件 ChatLogger、右键查版本）

个人设置 / 系统消息 / 头像窗体尚未实现，可按下表约定继续扩展。

## 创建方式（如需从零重建）
Visual Studio → 新建项目 → **MFC 应用** → 应用程序类型选「基于对话框」→ 项目名 `MyQQClient`，输出到本目录。
然后把 `src/common/` 下的 `*.h/*.cpp` 加入项目（Protocol / Message / Socket / ChatLogger）。

## 对话框（窗体）与对应类

| 窗体 | 类名 | 职责（对应 PPT 需求） |
|------|------|----------------------|
| 登录界面 | `CLoginDlg` | 账号密码登录，调用 `LOGIN` |
| 注册界面 | `CRegisterDlg` | 用户注册，调用 `REGISTER` |
| 登录后主界面 | `CMainDlg` | 好友列表显示、入口导航 |
| 查找/添加好友 | `CFindFriendDlg` | `SEARCH` / `ADD_FRIEND` |
| 聊天界面 | `CChatDlg` | 收发消息 `CHAT`，写聊天记录（ChatLogger） |
| 系统消息界面 | `CSysMsgDlg` | 好友请求等系统通知 |
| 个人设置界面 | `CProfileDlg` | 个人信息显示/修改 `GET/UPDATE_PROFILE` |
| 头像列表 | `CAvatarDlg` | 选择头像 |

## 网络层
- 复用 `src/common/Socket.h` 的 `TcpSocket` 连接服务端。
- 建议单独起一个接收线程，收到消息后 `PostMessage` 回主窗口刷新 UI（避免阻塞界面）。

## 需覆盖的交互要点（PPT 客户端要求）
1. 查询按钮：`GetLocalHostInfo` 获取本机主机名与 IP 并显示。
2. 输入远程服务端 IP + 端口并连接。
3. 自定义编辑消息，点击发送。
4. 接收远端消息并显示。
5. 右键窗口菜单：查询软件版本（`myqq::kAppVersion`）。
6. 自动建立文件存储聊天记录（`ChatLogger`）。

## 建议目录（VS 项目内）
```
MyQQClient/
  MyQQClient.vcxproj
  MyQQClient.rc           # 各对话框资源
  resource.h
  Dlg/                    # 各 C*Dlg 的 .h/.cpp
  Net/                    # 客户端网络封装（基于 common/Socket）
```
