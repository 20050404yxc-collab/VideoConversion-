param(
    [string]$QtRoot = 'D:\Qt\5.12.12\mingw73_64',
    [string]$MingwRoot = 'D:\Qt\Tools\mingw730_64',
    [string]$ToolDirectory = '',
    [string]$PackageName = 'VideoConversion-1.0.0-win64'
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (!$ToolDirectory) { $ToolDirectory = Join-Path $projectRoot 'tools\bin' }
if ($PackageName -notmatch '^[a-zA-Z0-9][a-zA-Z0-9._-]+$') { throw 'Invalid package name' }
$env:PATH = "$MingwRoot\bin;$QtRoot\bin;$env:PATH"
$application = Join-Path $projectRoot 'build\app\release\VideoConversion.exe'
if (!(Test-Path -LiteralPath $application)) { throw 'Build the application first.' }
foreach ($tool in @('ffmpeg.exe', 'ffprobe.exe')) {
    if (!(Test-Path -LiteralPath (Join-Path $ToolDirectory $tool))) { throw "Missing tool: $tool" }
}
$encoders = & (Join-Path $ToolDirectory 'ffmpeg.exe') -hide_banner -encoders 2>&1
if ($LASTEXITCODE -ne 0 -or ($encoders -join "`n") -notmatch '\blibx264\b' -or ($encoders -join "`n") -notmatch '\baac\b') {
    throw 'FFmpeg must provide libx264 and AAC.'
}
& (Join-Path $ToolDirectory 'ffprobe.exe') -version | Out-Null
if ($LASTEXITCODE -ne 0) { throw 'ffprobe cannot run.' }
$packagePath = Join-Path $projectRoot "dist\$PackageName"
# 已有目录拒绝复用，避免把上次部署的依赖误计入本次包。
if (Test-Path -LiteralPath $packagePath) { throw "Package already exists: $packagePath. Choose a new PackageName." }
New-Item -ItemType Directory -Path "$packagePath\tools", "$packagePath\licenses" | Out-Null
Copy-Item -LiteralPath $application -Destination $packagePath
& "$QtRoot\bin\windeployqt.exe" --release --no-translations --no-system-d3d-compiler "$packagePath\VideoConversion.exe"
if ($LASTEXITCODE -ne 0) { throw 'windeployqt failed.' }
Copy-Item -LiteralPath "$ToolDirectory\ffmpeg.exe", "$ToolDirectory\ffprobe.exe" -Destination "$packagePath\tools"
Copy-Item -LiteralPath (Join-Path $projectRoot 'README.md') -Destination $packagePath
Copy-Item -LiteralPath (Join-Path $projectRoot 'docs') -Destination "$packagePath\docs" -Recurse
Copy-Item -LiteralPath (Join-Path $projectRoot '会话总结.md') -Destination $packagePath
Copy-Item -LiteralPath (Join-Path $projectRoot 'task-specs') -Destination "$packagePath\task-specs" -Recurse
New-Item -ItemType Directory -Path "$packagePath\test-evidence" | Out-Null
foreach ($testSuite in @('tests', 'ui-tests')) {
    Copy-Item -LiteralPath "$projectRoot\build\$testSuite\test-results.txt" -Destination "$packagePath\test-evidence\$testSuite.txt"
    Copy-Item -LiteralPath "$projectRoot\build\$testSuite\test-results.xml" -Destination "$packagePath\test-evidence\$testSuite.xml"
}
Copy-Item -LiteralPath (Join-Path $projectRoot 'tools\README.md') -Destination "$packagePath\tools"
Copy-Item -LiteralPath "$ToolDirectory\LICENSE" -Destination "$packagePath\licenses\FFmpeg-GPL.txt"
Copy-Item -LiteralPath "$ToolDirectory\README.txt" -Destination "$packagePath\tools\build-provider-README.txt"
$qtLicenseRoot = Join-Path $QtRoot '..\..\Licenses'
Get-ChildItem -LiteralPath $qtLicenseRoot -File | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination "$packagePath\licenses"
}
foreach ($component in @('gcc', 'mingw-w64', 'winpthreads')) {
    $licenseDestination = Join-Path "$packagePath\licenses" "MinGW-$component"
    New-Item -ItemType Directory -Path $licenseDestination | Out-Null
    Get-ChildItem -LiteralPath "$MingwRoot\licenses\$component" -File | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination $licenseDestination
    }
}
Copy-Item -LiteralPath (Join-Path $projectRoot 'docs\依赖与许可证.md') -Destination "$packagePath\licenses"
$versionInfo = @(
    (& "$QtRoot\bin\qmake.exe" -v 2>&1 | Out-String),
    (& "$MingwRoot\bin\g++.exe" --version | Out-String),
    (& "$ToolDirectory\ffmpeg.exe" -version | Out-String),
    (& "$ToolDirectory\ffprobe.exe" -version | Out-String)
)
$versionInfo | Set-Content -LiteralPath "$packagePath\dependency-versions.txt" -Encoding UTF8
$inventory = Get-ChildItem -LiteralPath $packagePath -File -Recurse | ForEach-Object {
    [PSCustomObject]@{
        path = $_.FullName.Substring($packagePath.Length + 1).Replace('\', '/')
        bytes = $_.Length
        sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
    }
}
$inventory | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath "$packagePath\inventory.json" -Encoding UTF8
Compress-Archive -LiteralPath $packagePath -DestinationPath "$packagePath.zip" -CompressionLevel Optimal
# 源码包只选择工程和说明，明确排除大工具、构建产物与生成样例。
Add-Type -AssemblyName System.IO.Compression.FileSystem
$sourceFiles = @()
foreach ($folder in @('Application', 'Control', 'Interface', 'scripts', 'docs', 'task-specs')) {
    $sourceFiles += Get-ChildItem -LiteralPath (Join-Path $projectRoot $folder) -File -Recurse
}
$sourceFiles += Get-ChildItem -LiteralPath (Join-Path $projectRoot 'tests') -File | Where-Object { $_.Extension -in @('.cpp', '.pro') }
foreach ($rootFile in @('VideoConversion.pro', '.clang-format', '.gitignore', 'README.md', '会话总结.md', 'tools\README.md')) {
    $sourceFiles += Get-Item -LiteralPath (Join-Path $projectRoot $rootFile)
}
$sourceArchive = [System.IO.Compression.ZipFile]::Open("$packagePath-source.zip", [System.IO.Compression.ZipArchiveMode]::Create)
try {
    foreach ($file in $sourceFiles) {
        $relative = $file.FullName.Substring($projectRoot.Length + 1).Replace('\', '/')
        [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile($sourceArchive, $file.FullName, $relative) | Out-Null
    }
} finally { $sourceArchive.Dispose() }
Write-Output "Package: $packagePath.zip"
Write-Output "Source: $packagePath-source.zip"
