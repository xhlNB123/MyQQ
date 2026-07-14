[CmdletBinding()]
param(
    [string]$Version = '1.0.0'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$outRoot = Join-Path $repoRoot 'out'
$distRoot = Join-Path $repoRoot 'dist'
$serverBuild = Join-Path $outRoot 'build-server-msvc-x64'
$clientOutput = Join-Path $outRoot 'build-client-msvc-x64\Release\MyQQClient.exe'
$packageName = "MyQQ-$Version-windows-x64"
$stageRoot = Join-Path $distRoot $packageName
$zipPath = Join-Path $distRoot "$packageName.zip"
$hashPath = "$zipPath.sha256"

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) { throw "未找到 vswhere.exe，请安装 Visual Studio 2022。" }
$vsRoot = (& $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath).Trim()
if (-not $vsRoot) { throw "未找到 Visual Studio C++ x64 工具。请安装‘使用 C++ 的桌面开发’。" }

$cmake = Join-Path $vsRoot 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$msbuild = Join-Path $vsRoot 'MSBuild\Current\Bin\MSBuild.exe'
$vcvars = Join-Path $vsRoot 'VC\Auxiliary\Build\vcvars64.bat'
foreach ($tool in @($cmake, $msbuild, $vcvars)) {
    if (-not (Test-Path $tool)) { throw "缺少构建工具：$tool" }
}
$mfcHeader = Get-ChildItem (Join-Path $vsRoot 'VC\Tools\MSVC\*\atlmfc\include\afxwin.h') -ErrorAction SilentlyContinue | Select-Object -First 1
$mfcLibrary = Get-ChildItem (Join-Path $vsRoot 'VC\Tools\MSVC\*\atlmfc\lib\x64\nafxcw.lib') -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $mfcHeader -or -not $mfcLibrary) {
    throw "缺少 x64 静态 MFC。请在 VS Installer 中安装‘适用于最新 v143 生成工具的 C++ MFC’。"
}

Write-Host '[1/6] 清理脚本专属输出目录...'
Remove-Item $serverBuild -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item $stageRoot -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item $zipPath, $hashPath -Force -ErrorAction SilentlyContinue
New-Item $distRoot -ItemType Directory -Force | Out-Null

Write-Host '[2/6] 构建 MSVC x64 Release 服务端...'
& $cmake -S $repoRoot -B $serverBuild -G 'Visual Studio 17 2022' -A x64
if ($LASTEXITCODE -ne 0) { throw 'CMake 配置服务端失败。' }
& $cmake --build $serverBuild --config Release --target MyQQServer --parallel
if ($LASTEXITCODE -ne 0) { throw '服务端 Release 构建失败。' }
$serverExe = Join-Path $serverBuild 'Release\MyQQServer.exe'
if (-not (Test-Path $serverExe)) { throw "找不到服务端产物：$serverExe" }

Write-Host '[3/6] 构建静态 MFC x64 Release 客户端...'
$solution = Join-Path $repoRoot 'src\client\MyQQClient\MyQQClient.sln'
$clientObj = Join-Path $outRoot 'build-client-msvc-x64\obj\'
& $msbuild $solution /m /t:Rebuild /p:Configuration=Release /p:Platform=x64 `
    "/p:MyQQClientOutDir=$([IO.Path]::GetDirectoryName($clientOutput))\" `
    "/p:MyQQClientIntDir=$clientObj" /v:minimal
if ($LASTEXITCODE -ne 0) { throw 'MFC 客户端 Release 构建失败。' }
if (-not (Test-Path $clientOutput)) { throw "找不到客户端产物：$clientOutput" }

function Get-PeInfo([string]$Path) {
    $stream = [IO.File]::OpenRead($Path)
    try {
        $reader = [IO.BinaryReader]::new($stream)
        $stream.Position = 0x3c
        $peOffset = $reader.ReadInt32()
        $stream.Position = $peOffset
        if ($reader.ReadUInt32() -ne 0x00004550) { throw "$Path 不是有效 PE 文件。" }
        $machine = $reader.ReadUInt16()
        $sections = $reader.ReadUInt16()
        $stream.Position += 12
        $optionalSize = $reader.ReadUInt16()
        $stream.Position += 2
        $optionalStart = $stream.Position
        $magic = $reader.ReadUInt16()
        $stream.Position = $optionalStart + 68
        $subsystem = $reader.ReadUInt16()
        return [pscustomobject]@{ Machine=$machine; Subsystem=$subsystem; Sections=$sections; Magic=$magic }
    } finally { $stream.Dispose() }
}

$serverPe = Get-PeInfo $serverExe
$clientPe = Get-PeInfo $clientOutput
if ($serverPe.Machine -ne 0x8664 -or $serverPe.Subsystem -ne 3) { throw '服务端不是 x64 Console PE。' }
if ($clientPe.Machine -ne 0x8664 -or $clientPe.Subsystem -ne 2) { throw '客户端不是 x64 Windows GUI PE，可能选错 exe。' }

Write-Host '[4/6] 检查动态依赖...'
$vcToolsVersion = (Get-Content (Join-Path $vsRoot 'VC\Auxiliary\Build\Microsoft.VCToolsVersion.default.txt') -Raw).Trim()
$dumpbin = Join-Path $vsRoot "VC\Tools\MSVC\$vcToolsVersion\bin\Hostx64\x64\dumpbin.exe"
if (-not (Test-Path $dumpbin)) { throw "找不到 dumpbin.exe：$dumpbin" }
$forbidden = '(?i)(mfc\d+u?d?\.dll|msvcp\d+(?:_\d+)?d?\.dll|vcruntime\d+(?:_\d+)?d?\.dll|ucrtbased\.dll|api-ms-win-crt-|libgcc|libstdc\+\+|libwinpthread)'
foreach ($exe in @($serverExe, $clientOutput)) {
    $deps = (& $dumpbin /dependents $exe 2>&1 | Out-String)
    if ($deps -match $forbidden) { throw "发布文件仍依赖外部/调试运行库：$exe`n$($Matches[0])" }
}

Write-Host '[5/6] 组装发布目录...'
$serverStage = Join-Path $stageRoot 'server'
$clientStage = Join-Path $stageRoot 'client'
$docsStage = Join-Path $stageRoot 'docs'
New-Item (Join-Path $serverStage 'database'), $clientStage, $docsStage -ItemType Directory -Force | Out-Null
Copy-Item $serverExe (Join-Path $serverStage 'MyQQServer.exe')
Copy-Item $clientOutput (Join-Path $clientStage 'MyQQClient.exe')
Copy-Item (Join-Path $repoRoot 'database\schema_sqlite.sql') (Join-Path $serverStage 'database\schema_sqlite.sql')
Copy-Item (Join-Path $PSScriptRoot 'package-assets\Start-Server.cmd') (Join-Path $serverStage 'Start-Server.cmd')
Copy-Item (Join-Path $PSScriptRoot 'package-assets\README-部署.md') (Join-Path $stageRoot 'README-部署.md')
Copy-Item (Join-Path $repoRoot 'docs\客户端使用说明书.md') $docsStage
Copy-Item (Join-Path $repoRoot 'docs\服务端说明与测试指南.md') $docsStage

$forbiddenFiles = Get-ChildItem $stageRoot -Recurse -File | Where-Object {
    $_.Extension -match '^\.(pdb|obj|o|lib|exp|db)$' -or
    $_.FullName -match '(?i)[\\/](Debug|logs)[\\/]' -or
    $_.Name -eq 'MyQQConsoleClient.exe'
}
if ($forbiddenFiles) { throw "发布目录包含禁止文件：$($forbiddenFiles.FullName -join ', ')" }

Write-Host '[6/6] 压缩并生成 SHA-256...'
Compress-Archive -Path $stageRoot -DestinationPath $zipPath -CompressionLevel Optimal
$hash = (Get-FileHash $zipPath -Algorithm SHA256).Hash.ToLowerInvariant()
"$hash  $([IO.Path]::GetFileName($zipPath))" | Set-Content $hashPath -Encoding ascii

Write-Host "发布完成：$zipPath"
Write-Host "SHA-256：$hash"
