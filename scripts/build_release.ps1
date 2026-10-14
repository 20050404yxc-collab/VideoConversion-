param(
    [string]$QtRoot = 'D:\Qt\5.12.12\mingw73_64',
    [string]$MingwRoot = 'D:\Qt\Tools\mingw730_64',
    [switch]$Tests
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$env:PATH = "$MingwRoot\bin;$QtRoot\bin;$env:PATH"
$targets = @(@{ Name = 'app'; Project = 'VideoConversion.pro' })
if ($Tests) {
    $targets += @{ Name = 'tests'; Project = 'tests\tests.pro' }
    $targets += @{ Name = 'ui-tests'; Project = 'tests\ui_tests.pro' }
}
foreach ($target in $targets) {
    $buildPath = Join-Path $projectRoot "build\$($target.Name)"
    New-Item -ItemType Directory -Force -Path $buildPath | Out-Null
    Push-Location -LiteralPath $buildPath
    try {
        & "$QtRoot\bin\qmake.exe" (Join-Path $projectRoot $target.Project) -spec win32-g++ 'CONFIG+=release'
        if ($LASTEXITCODE -ne 0) { throw "qmake failed: $($target.Name)" }
        & "$MingwRoot\bin\mingw32-make.exe" -j4
        if ($LASTEXITCODE -ne 0) { throw "Build failed: $($target.Name)" }
    } finally {
        Pop-Location
    }
}
