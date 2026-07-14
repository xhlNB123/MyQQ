@echo off
chcp 65001 >nul
cd /d "%~dp0"
echo [MyQQ] 正在启动局域网聊天服务端...
echo [MyQQ] 请保持本窗口开启；客户端连接本机时使用 127.0.0.1:6000。
echo.
MyQQServer.exe
set "EXIT_CODE=%ERRORLEVEL%"
echo.
echo [MyQQ] 服务端已退出，退出码：%EXIT_CODE%
pause
exit /b %EXIT_CODE%
