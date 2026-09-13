# Build the userspace test client for Android arm64 with the NDK.
#   pwsh -NoProfile -ExecutionPolicy Bypass -File .\kernel\build-client.ps1
param(
    [string]$Ndk = "D:\Software\android-ndk-r26d",
    [string]$Api = "34"
)
$ErrorActionPreference = "Stop"
$bin = Join-Path $Ndk "toolchains\llvm\prebuilt\windows-x86_64\bin"
if (!(Test-Path $bin)) { throw "NDK bin not found: $bin" }

$src = Join-Path $PSScriptRoot "xth_client.c"
$out = Join-Path $PSScriptRoot "xth_client"

$clang = Join-Path $bin "aarch64-linux-android$Api-clang.cmd"
if (!(Test-Path $clang)) { throw "NDK clang wrapper not found: $clang" }
& $clang -O2 -static -o $out $src
if ($LASTEXITCODE -ne 0) { throw "clang failed ($LASTEXITCODE)" }

$llvmReadelf = Join-Path $bin "llvm-readelf.exe"
& $llvmReadelf -h $out | Select-String "Class:|Type:|Machine:"
Write-Host "built: $out"
