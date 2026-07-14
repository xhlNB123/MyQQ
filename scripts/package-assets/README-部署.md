# MyQQ Windows x64 发布包

本压缩包是可直接运行的 Release 版本。接收方**不需要 Visual Studio、MFC、MinGW 或额外运行库**。

## 单机测试

1. 完整解压压缩包，不要直接在压缩软件预览窗口里运行。
2. 双击 `server\Start-Server.cmd`，保持服务端窗口开启。
3. 双击 `client\MyQQClient.exe`；服务器 IP 填 `127.0.0.1`，端口填 `6000`。
4. 可双开客户端，注册两个账号测试好友和聊天。

## 局域网测试

1. 服务端电脑运行 `server\Start-Server.cmd`，防火墙询问时允许“专用网络”。
2. 在服务端窗口查看局域网 IPv4 地址（通常为 `192.168.x.x`）。
3. 其他电脑运行 `client\MyQQClient.exe`，填服务端局域网 IP 和端口 `6000`。

## 文件位置

- 服务端数据库：`server\data\myqq.db`（首次启动自动创建）
- 客户端聊天记录：`client\logs\`
- GUI 客户端：`client\MyQQClient.exe`

不要把 `MyQQConsoleClient.exe` 当作图形客户端；发布包不会包含该开发测试程序。

若 Windows SmartScreen 提示“未知发布者”，这是因为学生项目没有商业代码签名。请先核对压缩包旁的 SHA-256 文件，再选择继续运行。
