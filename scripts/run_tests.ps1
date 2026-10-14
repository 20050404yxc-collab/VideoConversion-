param(
    [string]$QtRoot = 'D:\Qt\5.12.12\mingw73_64',
    [string]$MingwRoot = 'D:\Qt\Tools\mingw730_64',
    [switch]$SkipBuild,
    [switch]$SkipFixtures
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$env:PATH = "$QtRoot\bin;$MingwRoot\bin;$env:PATH"
$env:VIDEOCONVERSION_TOOLS = Join-Path $projectRoot 'tools\bin'
$env:VIDEOCONVERSION_FIXTURES = Join-Path $projectRoot 'tests\fixtures'
$env:VIDEOCONVERSION_SCREENSHOTS = Join-Path $projectRoot 'docs\screenshots'
if (!$SkipFixtures) { & "$PSScriptRoot\prepare_fixtures.ps1" }
if (!$SkipBuild) { & "$PSScriptRoot\build_release.ps1" -QtRoot $QtRoot -MingwRoot $MingwRoot -Tests }
foreach ($target in @(@{ Directory = 'tests'; Executable = 'conversion_tests' }, @{ Directory = 'ui-tests'; Executable = 'ui_tests' })) {
    Push-Location -LiteralPath (Join-Path $projectRoot "build\$($target.Directory)")
    try {
        $arguments = @('-o','test-results.txt,txt','-o','test-results.xml,xunitxml')
        & ".\release\$($target.Executable).exe" @arguments
        if ($LASTEXITCODE -ne 0) {
            Get-Content -LiteralPath test-results.txt
            throw "Tests failed: $($target.Executable)"
        }
        Get-Content -LiteralPath test-results.txt -Tail 3
    } finally { Pop-Location }
}
