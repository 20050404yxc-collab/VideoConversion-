param(
    [string]$QtRoot = 'D:\Qt\5.12.12\mingw73_64',
    [string]$MingwRoot = 'D:\Qt\Tools\mingw730_64',
    [string]$ToolDirectory = '',
    [string]$VlcDirectory = '',
    [string]$PackageName = 'VideoConversion-1.0.0-win64-revision4-timeline'
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (!$ToolDirectory) { $ToolDirectory = Join-Path $projectRoot 'tools\bin' }
if (!$VlcDirectory) { $VlcDirectory = Join-Path $projectRoot 'tools\vlc\vlc-3.0.24' }
# 打包前必须准备完整的官方 VLC 3 运行时和许可材料，不能依赖系统安装。
foreach ($vlcFile in @('libvlc.dll', 'libvlccore.dll', 'COPYING.txt', 'COPYING.LIB', 'README.txt', 'runtime-provenance.json')) {
    if (!(Test-Path -LiteralPath (Join-Path $VlcDirectory $vlcFile) -PathType Leaf)) { throw "Missing VLC dependency: $vlcFile" }
}
foreach ($vlcFolder in @('plugins', 'hrtfs', 'lua')) {
    if (!(Test-Path -LiteralPath (Join-Path $VlcDirectory $vlcFolder) -PathType Container)) { throw "Missing VLC directory: $vlcFolder" }
}
$vlcProvenance = Get-Content -LiteralPath (Join-Path $VlcDirectory 'runtime-provenance.json') -Raw | ConvertFrom-Json
if ($vlcProvenance.version -ne '3.0.24' -or $vlcProvenance.archiveSha256 -ne 'fcf30850371ad10c9373cc4f0f4501e7dee49e3e9ae9f20c72fb2661a1ca6323') { throw 'Unverified VLC runtime provenance.' }
foreach ($entry in $vlcProvenance.files) {
    $runtimeFile = Join-Path $VlcDirectory $entry.path
    if (!(Test-Path -LiteralPath $runtimeFile -PathType Leaf) -or (Get-FileHash -LiteralPath $runtimeFile -Algorithm SHA256).Hash -ne $entry.sha256) { throw "VLC runtime changed or incomplete: $($entry.path)" }
}
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
# VLC 插件整目录复制并保留数据目录，避免人工挑插件遗漏音频或解码依赖。
Get-ChildItem -LiteralPath $VlcDirectory -Filter '*.dll' -File | Where-Object { $_.Name -ne 'axvlc.dll' } | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination $packagePath
}
foreach ($vlcFolder in @('plugins', 'hrtfs', 'lua')) {
    Copy-Item -LiteralPath (Join-Path $VlcDirectory $vlcFolder) -Destination (Join-Path $packagePath $vlcFolder) -Recurse
}
foreach ($vlcLicense in @('COPYING.txt', 'COPYING.LIB', 'README.txt', 'runtime-provenance.json')) {
    Copy-Item -LiteralPath (Join-Path $VlcDirectory $vlcLicense) -Destination (Join-Path "$packagePath\licenses" "VLC-$vlcLicense")
}
@'
Playback: libVLC 3.0.24 win64 embedded in Qt 5.12 Widgets through its C ABI and native HWND.
Runtime: libvlc.dll, libvlccore.dll, full plugins, hrtfs and lua directories beside the executable.
FFmpeg conversion support is independent of VLC playback support.
No system codec installation or Qt Multimedia/DirectShow playback dependency.
Human audible sound/synchronization and an independent clean target still require acceptance.
'@ | Set-Content -LiteralPath "$packagePath\playback-backend.txt" -Encoding UTF8
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
# 隔离开发路径的部署证据与普通构建测试分开保留。
$deploymentEvidence = Join-Path $projectRoot 'build\revision4-deployment-check'
if (Test-Path -LiteralPath "$deploymentEvidence\range-results.xml") {
    foreach ($evidenceFile in @('test-results.txt', 'test-results.xml', 'range-results.txt', 'range-results.xml', 'user-mp4.txt', 'user-mp4.xml', 'deployment.log')) {
        Copy-Item -LiteralPath "$deploymentEvidence\$evidenceFile" -Destination "$packagePath\test-evidence\portable-$evidenceFile"
    }
}
Copy-Item -LiteralPath (Join-Path $projectRoot 'tools\README.md') -Destination "$packagePath\tools"
# 第四版诊断/实际 MP4 证据独立命名，避免与历史 DirectShow 证据混淆。
foreach ($diagnosticEvidence in @{
    'revision4-native.txt' = 'build\ui-tests\revision4-results.txt'
    'revision4-native.xml' = 'build\ui-tests\revision4-results.xml'
    'revision4-user-mp4.txt' = 'build\ui-tests\revision4-user-mp4.txt'
    'revision4-user-mp4.xml' = 'build\ui-tests\revision4-user-mp4.xml'
}.GetEnumerator()) {
    $diagnosticSource = Join-Path $projectRoot $diagnosticEvidence.Value
    if (Test-Path -LiteralPath $diagnosticSource) {
        Copy-Item -LiteralPath $diagnosticSource -Destination (Join-Path "$packagePath\test-evidence" $diagnosticEvidence.Key)
    }
}
# 进度交互修复及本次隔离部署结果独立保留，不把旧包结果当作当前包验证。
foreach ($evidenceRoot in @('build\ui-tests', 'build\timeline-deployment-check')) {
    $evidencePath = Join-Path $projectRoot $evidenceRoot
    if (Test-Path -LiteralPath $evidencePath) {
        Get-ChildItem -LiteralPath $evidencePath -File | Where-Object {
            $_.Name -like 'timeline-*.txt' -or $_.Name -like 'timeline-*.xml'
        } | ForEach-Object {
            $prefix = if ($evidenceRoot -like '*deployment*') { 'portable-' } else { 'native-' }
            Copy-Item -LiteralPath $_.FullName -Destination (Join-Path "$packagePath\test-evidence" ($prefix + $_.Name))
        }
    }
}
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
    ("libVLC " + $vlcProvenance.version + ' win64; archive SHA256 ' + $vlcProvenance.archiveSha256)
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
