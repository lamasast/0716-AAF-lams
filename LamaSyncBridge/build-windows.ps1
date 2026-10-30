[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',
    [switch]$Clean
)

$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot

function Resolve-CMake {
    $command = Get-Command cmake -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }

    $programFilesX86 = [Environment]::GetFolderPath('ProgramFilesX86')
    $vswhere = Join-Path $programFilesX86 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswhere) {
        $instances = & $vswhere -latest -products * -format json | ConvertFrom-Json
        foreach ($instance in $instances) {
            $candidate = Join-Path $instance.installationPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
            if (Test-Path -LiteralPath $candidate) { return $candidate }
        }
    }
    throw 'CMake was not found. Install Visual Studio Build Tools with Desktop development with C++ and CMake.'
}

$cmake = Resolve-CMake
$ctest = Join-Path (Split-Path -Parent $cmake) 'ctest.exe'
$buildDirectory = Join-Path $PSScriptRoot 'build-win'

if ($Clean -and (Test-Path -LiteralPath $buildDirectory)) {
    $resolvedBuild = (Resolve-Path -LiteralPath $buildDirectory).Path
    $resolvedRoot = (Resolve-Path -LiteralPath $PSScriptRoot).Path
    if (-not $resolvedBuild.StartsWith($resolvedRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to remove a build directory outside the project: $resolvedBuild"
    }
    Remove-Item -LiteralPath $resolvedBuild -Recurse -Force
}

& $cmake -S . -B $buildDirectory -A x64 -DLAMASYNC_BUILD_AUDIO=ON -DBUILD_TESTING=ON
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed' }

& $cmake --build $buildDirectory --config $Configuration --target LamaSyncAudIO LamaSyncProtocolTest LamaSyncAudioRenderTest
if ($LASTEXITCODE -ne 0) { throw 'Windows build failed' }

& $ctest --test-dir $buildDirectory -C $Configuration --output-on-failure
if ($LASTEXITCODE -ne 0) { throw 'CTest failed' }

$binaryDirectory = Join-Path $buildDirectory "bin\$Configuration"
$application = Join-Path $binaryDirectory 'LamaSyncAudIO.exe'
$wave = Join-Path $buildDirectory 'audio-test\lamasync-220hz-48k.wav'
if (-not (Test-Path -LiteralPath $application)) { throw "Expected executable missing: $application" }
if (-not (Test-Path -LiteralPath $wave)) { throw "Expected audio test output missing: $wave" }

Write-Host "Build and tests passed."
Write-Host "Application: $application"
Write-Host "Audio test: $wave"
