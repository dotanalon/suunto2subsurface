#Requires -Version 5.1
<#
.SYNOPSIS
    Builds suunto2subsurface on Windows with MSVC and Qt6.

.DESCRIPTION
    Mirrors vendor/subsurface/packaging/windows-msvc/build.ps1's approach for
    building libdivecomputer and subsurface_corelib, but targets
    DownloaderExecutable mode (corelib + subsurface_commands, no
    desktop-widgets/mobile-widgets/googlemaps/qlitehtml -- see
    vendor/README.md for why "commands" specifically is needed), then
    configures+builds this repo's own CMakeLists.txt against that.

.PARAMETER VcpkgRoot
    Path to the vcpkg installation. Defaults to $env:VCPKG_ROOT or C:\vcpkg.

.PARAMETER Qt6Dir
    Path to the Qt6 MSVC kit (e.g. installed via jurplel/install-qt-action).
    Defaults to $env:QT_ROOT_DIR (what jurplel/install-qt-action@v4 actually
    sets), falling back to $env:Qt6_DIR for other Qt-provisioning setups.

.PARAMETER BuildType
    Build type: Debug or Release. Defaults to Release.

.PARAMETER SkipCorelibBuild
    Skip building libdivecomputer and subsurface_corelib/subsurface_commands
    and reuse whatever is already at vendor\install-root and
    vendor\subsurface\build-downloader (e.g. restored from a CI cache keyed
    on the vendor/subsurface submodule commit -- that output only changes
    when the submodule is bumped or a patches/*.patch file changes, so
    ordinary commits to this repo's own code can safely reuse it instead of
    rebuilding both from scratch every run).
#>

param(
    [string]$VcpkgRoot = "",
    [string]$Qt6Dir = "",
    [string]$BuildType = "Release",
    [int]$Jobs = 0,
    [switch]$SkipCorelibBuild
)

$ErrorActionPreference = "Stop"

function Write-Step($Message) {
    Write-Host ""
    Write-Host "=== $Message ===" -ForegroundColor Cyan
}

$RepoRoot = (Get-Item "$PSScriptRoot\..\..").FullName
$SubsurfaceSrc = Join-Path $RepoRoot "vendor\subsurface"
$SubsurfaceBuild = Join-Path $SubsurfaceSrc "build-downloader"
$InstallRoot = Join-Path $RepoRoot "vendor\install-root"

if (-not $VcpkgRoot) {
    # Deliberately not trusting $env:VCPKG_ROOT here: ilammy/msvc-dev-cmd
    # (run before this script, to put cl.exe/link.exe on PATH) imports
    # vcvarsall's full environment, which overwrites VCPKG_ROOT with Visual
    # Studio's own *bundled* vcpkg -- not the standalone C:\vcpkg the
    # workflow actually installed our packages into. Confirmed by a real CI
    # failure ("...VC\vcpkg\installed\x64-windows\lib\libxml2.lib' ...
    # missing"). vendor/subsurface's own windows-msvc-qt6.yml sidesteps this
    # the same way, hardcoding the path instead of reading the env var here.
    $VcpkgRoot = "C:\vcpkg"
}
if (-not $Qt6Dir) {
    $Qt6Dir = if ($env:QT_ROOT_DIR) { $env:QT_ROOT_DIR } else { $env:Qt6_DIR }
}
if (-not $Qt6Dir -or -not (Test-Path $Qt6Dir)) {
    Write-Error "Qt6 not found -- set -Qt6Dir, or `$env:QT_ROOT_DIR/`$env:Qt6_DIR (jurplel/install-qt-action@v4 sets QT_ROOT_DIR)"
    exit 1
}
if ($Jobs -le 0) {
    $Jobs = if ($env:NUMBER_OF_PROCESSORS) { $env:NUMBER_OF_PROCESSORS } else { 4 }
}
$VcpkgInstalled = Join-Path $VcpkgRoot "installed\x64-windows"

if ($SkipCorelibBuild) {
    Write-Step "Skipping libdivecomputer + subsurface_corelib/subsurface_commands build (reusing cached output)"
} else {

# ---------------------------------------------------------------
# libdivecomputer (MSVC vcxproj build, same as vendor/subsurface's own
# packaging/windows-msvc/build.ps1)
# ---------------------------------------------------------------

Write-Step "Building libdivecomputer"

$LibdcDir = Join-Path $SubsurfaceSrc "libdivecomputer"
$LibdcProj = Join-Path $LibdcDir "contrib\msvc\libdivecomputer.vcxproj"
if (-not (Test-Path $LibdcProj)) {
    Write-Error "libdivecomputer MSVC project not found at $LibdcProj"
    exit 1
}

# version.h.in's placeholders are @DC_VERSION@/@DC_VERSION_MAJOR@/
# @DC_VERSION_MINOR@/@DC_VERSION_MICRO@ (not a single "@VERSION@" -- an
# earlier version of this script replaced that non-existent placeholder,
# a silent no-op that left the real ones untouched and broke the compile
# with "unknown character '0x40'"/"DC_VERSION_MAJOR: undeclared identifier"
# once the build got far enough to hit it). Parse them from configure.ac,
# same as vendor/subsurface's own windows-msvc-qt6.yml does.
$ConfigureAc = Get-Content (Join-Path $LibdcDir "configure.ac") -Raw
function Get-DcVersionPart($Name) {
    if ($ConfigureAc -match "m4_define\(\[dc_version_$Name\],\[(\d+)\]\)") {
        return $Matches[1]
    }
    return "0"
}
$VerMajor = Get-DcVersionPart "major"
$VerMinor = Get-DcVersionPart "minor"
$VerMicro = Get-DcVersionPart "micro"
$DcVersion = "$VerMajor.$VerMinor.$VerMicro"

$VersionHIn = Join-Path $LibdcDir "include\libdivecomputer\version.h.in"
$VersionHOut = Join-Path $LibdcDir "include\libdivecomputer\version.h"
$VersionH = (Get-Content $VersionHIn -Raw) `
    -replace '@DC_VERSION@', $DcVersion `
    -replace '@DC_VERSION_MAJOR@', $VerMajor `
    -replace '@DC_VERSION_MINOR@', $VerMinor `
    -replace '@DC_VERSION_MICRO@', $VerMicro
Set-Content -Path $VersionHOut -Value $VersionH

# src/version.c #includes "revision.h" (not generated by the .in template
# above); vendor/subsurface's own windows-msvc-qt6.yml generates it the same
# way -- confirmed by a real CI failure ("Cannot open include file:
# 'revision.h'").
Push-Location $LibdcDir
$GitRevision = git rev-parse --verify HEAD
Pop-Location
"#define DC_VERSION_REVISION `"$GitRevision`"" | Set-Content (Join-Path $LibdcDir "src\revision.h")

msbuild $LibdcProj /p:Configuration=$BuildType /p:Platform=x64 /m
if ($LASTEXITCODE -ne 0) {
    Write-Error "libdivecomputer build failed"
    exit $LASTEXITCODE
}

New-Item -ItemType Directory -Force -Path "$InstallRoot\include\libdivecomputer" | Out-Null
New-Item -ItemType Directory -Force -Path "$InstallRoot\lib" | Out-Null
New-Item -ItemType Directory -Force -Path "$InstallRoot\bin" | Out-Null
Copy-Item "$LibdcDir\include\libdivecomputer\*" -Destination "$InstallRoot\include\libdivecomputer" -Recurse -Force

# libdivecomputer.vcxproj's <ConfigurationType> is DynamicLibrary (a DLL,
# not a static lib) and its <OutDir> is
# "$(SolutionDir)$(PlatformTarget)\$(Configuration)\bin\" -- confirmed by a
# real CI failure ("Cannot find path '...\x64\Release\libdivecomputer.lib'
# because it does not exist", missing the "\bin\" component). Copy both the
# import lib (needed at link time) and the DLL itself (needed at runtime,
# since this is a dynamic library).
$LibdcOutDir = Join-Path $LibdcDir "contrib\msvc\x64\$BuildType\bin"
Copy-Item (Join-Path $LibdcOutDir "libdivecomputer.lib") -Destination "$InstallRoot\lib\" -Force
Copy-Item (Join-Path $LibdcOutDir "libdivecomputer.dll") -Destination "$InstallRoot\bin\" -Force

# ---------------------------------------------------------------
# subsurface_corelib + subsurface_commands (DownloaderExecutable mode)
# ---------------------------------------------------------------

Write-Step "Configuring subsurface_corelib + subsurface_commands"

New-Item -ItemType Directory -Force -Path $SubsurfaceBuild | Out-Null
Push-Location $SubsurfaceBuild

$PrefixPath = @($Qt6Dir, $InstallRoot, $VcpkgInstalled) -join ";"
cmake $SubsurfaceSrc -G Ninja `
    "-DCMAKE_BUILD_TYPE=$BuildType" `
    "-DCMAKE_PREFIX_PATH=$PrefixPath" `
    "-DCMAKE_TOOLCHAIN_FILE=$VcpkgRoot\scripts\buildsystems\vcpkg.cmake" `
    "-DSUBSURFACE_TARGET_EXECUTABLE=DownloaderExecutable" `
    "-DBUILD_WITH_QT6=ON" `
    "-DSUBSURFACE_VCPKG_ROOT=$VcpkgRoot" `
    "-DLIBDIVECOMPUTER_INCLUDE_DIR=$InstallRoot\include" `
    "-DLIBDIVECOMPUTER_LIBRARIES=$InstallRoot\lib\libdivecomputer.lib" `
    "-DLIBGIT2_INCLUDE_DIR=$VcpkgInstalled\include" `
    "-DLIBGIT2_LIBRARIES=$VcpkgInstalled\lib\git2.lib" `
    "-DMAKE_TESTS=OFF"
if ($LASTEXITCODE -ne 0) { Write-Error "subsurface CMake configure failed"; exit $LASTEXITCODE }

cmake --build . --config $BuildType -j $Jobs
if ($LASTEXITCODE -ne 0) { Write-Error "subsurface_corelib/commands build failed"; exit $LASTEXITCODE }
Pop-Location

}

# ---------------------------------------------------------------
# suunto2subsurface itself
# ---------------------------------------------------------------

Write-Step "Configuring suunto2subsurface"

$OurBuild = Join-Path $RepoRoot "build"
New-Item -ItemType Directory -Force -Path $OurBuild | Out-Null
Push-Location $OurBuild

$PrefixPath2 = @($Qt6Dir, $VcpkgInstalled) -join ";"
cmake $RepoRoot -G Ninja `
    "-DCMAKE_BUILD_TYPE=$BuildType" `
    "-DCMAKE_PREFIX_PATH=$PrefixPath2" `
    "-DCMAKE_TOOLCHAIN_FILE=$VcpkgRoot\scripts\buildsystems\vcpkg.cmake" `
    "-DSUBSURFACE_BUILD=$SubsurfaceBuild" `
    "-DSUBSURFACE_INSTALL_ROOT=$InstallRoot"
if ($LASTEXITCODE -ne 0) { Write-Error "suunto2subsurface CMake configure failed"; exit $LASTEXITCODE }

cmake --build . --config $BuildType -j $Jobs
if ($LASTEXITCODE -ne 0) { Write-Error "suunto2subsurface build failed"; exit $LASTEXITCODE }
Pop-Location

# suunto2subsurface.exe links libdivecomputer.dll dynamically -- it needs to
# sit next to the exe (the workflow's packaging step globs build\*.dll).
Copy-Item "$InstallRoot\bin\libdivecomputer.dll" -Destination $OurBuild -Force

Write-Step "Build complete: $OurBuild\suunto2subsurface.exe"
