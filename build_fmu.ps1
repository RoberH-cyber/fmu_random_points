param(
    [string]$FmiHeadersPath = "C:\Users\RHU101\OneDrive - WinGD\work\ACMSModel\FMI-Standard-2.0.5\headers",
    [string]$OutDir = "build",
    [string]$ModelIdentifier = "RandomPoints500",
    [string]$FmuOutputPath = "C:\Users\RHU101\OneDrive - WinGD\work\ACMSModel\RandomPoints500.fmu",
    [switch]$UseMingw,
    [string]$MingwGccPath = "C:\mingw81\bin\gcc.exe"
)

# 关键：任何错误都立即停止，避免生成半成品
$ErrorActionPreference = "Stop"

# 关键：定位源码与模型描述文件
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$SourcesDir = Join-Path $Root "sources"
$SourceFile = Join-Path $SourcesDir "fmu_random_points.c"
$ModelDescription = Join-Path $Root "modelDescription.xml"

# 关键：输入文件必须存在
if (-not (Test-Path $SourceFile)) {
    throw "Source file not found: $SourceFile"
}
if (-not (Test-Path $ModelDescription)) {
    throw "modelDescription.xml not found: $ModelDescription"
}

# 关键：FMI 头文件路径必须有效
if ([string]::IsNullOrWhiteSpace($FmiHeadersPath)) {
    throw "FmiHeadersPath is required. Example: -FmiHeadersPath C:\\path\\to\\FMI\\headers"
}

# 关键：至少需要 fmi2Functions.h
$HeaderFile = Join-Path $FmiHeadersPath "fmi2Functions.h"
if (-not (Test-Path $HeaderFile)) {
    throw "fmi2Functions.h not found in: $FmiHeadersPath"
}

$BuildDir = Join-Path $Root $OutDir
$BinDir = Join-Path $BuildDir "binaries\win64"
$StageDir = Join-Path $BuildDir "fmu_stage"
# 关键：DLL 输出路径
$DllPath = Join-Path $BinDir ("$ModelIdentifier.dll")
# 关键：FMU 最终输出路径
$FmuPath = if ([string]::IsNullOrWhiteSpace($FmuOutputPath)) {
    Join-Path $BuildDir ("$ModelIdentifier.fmu")
} else {
    $resolvedDir = Split-Path -Parent $FmuOutputPath
    if (-not [string]::IsNullOrWhiteSpace($resolvedDir)) {
        New-Item -ItemType Directory -Force -Path $resolvedDir | Out-Null
    }
    $FmuOutputPath
}

# 关键：创建输出目录
New-Item -ItemType Directory -Force -Path $BinDir | Out-Null

# 关键：编译 C 源码为 DLL（MinGW 或 MSVC 二选一）
if ($UseMingw) {
    if (Test-Path $MingwGccPath) {
        $gccExe = $MingwGccPath
    } else {
        $gcc = Get-Command gcc -ErrorAction SilentlyContinue
        if (-not $gcc) {
            throw "gcc not found. Install MinGW-w64 or set -MingwGccPath to your gcc.exe."
        }
        $gccExe = $gcc.Source
    }
    & $gccExe -shared -o $DllPath $SourceFile -I $FmiHeadersPath
} else {
    $cl = Get-Command cl -ErrorAction SilentlyContinue
    if (-not $cl) {
        throw "cl.exe not found in PATH. Run from a Visual Studio Developer PowerShell, or use -UseMingw."
    }
    Push-Location $BuildDir
    & $cl.Source /nologo /LD /I $FmiHeadersPath $SourceFile /link /OUT:$DllPath
    Pop-Location
}

# 关键：确保 DLL 已生成
if (-not (Test-Path $DllPath)) {
    throw "DLL not produced: $DllPath"
}

# 关键：准备 FMU 打包目录
if (Test-Path $StageDir) {
    Remove-Item $StageDir -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $StageDir | Out-Null

# 关键：拷贝 FMU 所需文件
Copy-Item $ModelDescription (Join-Path $StageDir "modelDescription.xml")
New-Item -ItemType Directory -Force -Path (Join-Path $StageDir "sources") | Out-Null
Copy-Item $SourceFile (Join-Path $StageDir "sources\fmu_random_points.c")
New-Item -ItemType Directory -Force -Path (Join-Path $StageDir "binaries\win64") | Out-Null
Copy-Item $DllPath (Join-Path $StageDir "binaries\win64\$ModelIdentifier.dll")

# 关键：清理旧 FMU
if (Test-Path $FmuPath) {
    Remove-Item $FmuPath -Force
}

# 关键：打包为 FMU
Compress-Archive -Path (Join-Path $StageDir "*") -DestinationPath $FmuPath

# 关键：输出结果
Write-Host "FMU created: $FmuPath"
