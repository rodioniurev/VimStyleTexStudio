# SPDX-License-Identifier: GPL-3.0-only
param(
    [string]$Sdk = 'C:/msys64/ucrt64',
    [string]$Source = (Join-Path $PSScriptRoot '../..'),
    [string]$Build = '',
    [int]$Jobs = 4
)
$ErrorActionPreference = 'Stop'
$Source = (Resolve-Path -LiteralPath $Source).Path
$Sdk = (Resolve-Path -LiteralPath $Sdk).Path.Replace('\', '/')
if (!$Build) { $Build = Join-Path $Source 'build-windows' }
$env:PATH = "$Sdk/bin;$env:PATH"
$env:PKG_CONFIG_PATH = "$Sdk/lib/pkgconfig;$Sdk/share/pkgconfig"
$env:PKG_CONFIG_LIBDIR = $env:PKG_CONFIG_PATH
$cmakePath = Join-Path $Sdk 'bin/cmake.exe'
if (!(Test-Path -LiteralPath $cmakePath)) { $cmakePath = (Get-Command cmake.exe -ErrorAction Stop).Source }
$ninjaPath = Join-Path $Sdk 'bin/ninja.exe'
if (!(Test-Path -LiteralPath $ninjaPath)) { $ninjaPath = (Get-Command ninja.exe -ErrorAction Stop).Source }
& $cmakePath -S $Source -B $Build -G Ninja `
    -DCMAKE_BUILD_TYPE=Release -DQT_VERSION_MAJOR=6 `
    "-DCMAKE_PREFIX_PATH=$Sdk" "-DCMAKE_MAKE_PROGRAM=$ninjaPath" `
    "-DCMAKE_C_COMPILER=$Sdk/bin/gcc.exe" "-DCMAKE_CXX_COMPILER=$Sdk/bin/g++.exe" `
    "-DCMAKE_RC_COMPILER=$Sdk/bin/windres.exe" "-DPKG_CONFIG_EXECUTABLE=$Sdk/bin/pkgconf.exe" `
    -DCMAKE_DISABLE_FIND_PACKAGE_GitInfo=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON `
    -DTEXSTUDIO_BUILD_ADWAITA=OFF -DTEXSTUDIO_ENABLE_CPP23_CRASH_HANDLER=OFF `
    -DTEXSTUDIO_ENABLE_TESTS=OFF
if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed' }
& $cmakePath --build $Build -- -d keeprsp "-j$Jobs"
if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
Write-Output (Join-Path $Build 'texstudio.exe')
