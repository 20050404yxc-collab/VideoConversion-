param([string]$RuntimeRoot = '')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (!$RuntimeRoot) { $RuntimeRoot = Join-Path $projectRoot 'tools\vlc' }
$RuntimeRoot = [System.IO.Path]::GetFullPath($RuntimeRoot)
New-Item -ItemType Directory -Force -Path $RuntimeRoot | Out-Null
$version = '3.0.24'
$archiveName = "vlc-$version-win64.zip"
$archivePath = Join-Path $projectRoot "tools\bin\$archiveName"
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $archivePath) | Out-Null
$archiveSha256 = 'fcf30850371ad10c9373cc4f0f4501e7dee49e3e9ae9f20c72fb2661a1ca6323'
$archiveUrl = "https://download.videolan.org/pub/videolan/vlc/$version/win64/$archiveName"
$sourceName = "vlc-$version.tar.xz"
$sourcePath = Join-Path $RuntimeRoot $sourceName
$sourceSha256 = 'e7cab503d1d7d5849b89d2cf0e1ee60d0ef6d012407791b644b9cfc0cc225fdf'
$sourceUrl = "https://download.videolan.org/pub/videolan/vlc/$version/$sourceName"
foreach ($download in @(@{ Path = $archivePath; Url = $archiveUrl; Hash = $archiveSha256 }, @{ Path = $sourcePath; Url = $sourceUrl; Hash = $sourceSha256 })) {
    # 完整且校验一致的已有归档可复用，部分文件由 curl 断点续传。
    if (!(Test-Path -LiteralPath $download.Path) -or (Get-FileHash -LiteralPath $download.Path -Algorithm SHA256).Hash -ne $download.Hash) {
        & curl.exe -L -C - --fail --retry 3 --output $download.Path $download.Url
        if ($LASTEXITCODE -ne 0) { throw "Download failed: $($download.Url)" }
    }
    if ((Get-FileHash -LiteralPath $download.Path -Algorithm SHA256).Hash -ne $download.Hash) { throw "Checksum mismatch: $($download.Path). Replace the invalid archive and retry." }
}
$runtimePath = Join-Path $RuntimeRoot "vlc-$version"
# 不覆盖现有运行时，避免悄悄混用不同版本；现有目录同样生成逐文件清单。
if (!(Test-Path -LiteralPath $runtimePath)) { Expand-Archive -LiteralPath $archivePath -DestinationPath $RuntimeRoot }
& tar.exe -xf $sourcePath -C $RuntimeRoot "vlc-$version/COPYING.LIB"
if ($LASTEXITCODE -ne 0) { throw 'Cannot extract the LGPL license.' }
$files = @()
foreach ($folder in @('plugins', 'hrtfs', 'lua')) {
    $files += Get-ChildItem -LiteralPath (Join-Path $runtimePath $folder) -File -Recurse
}
$files += Get-ChildItem -LiteralPath $runtimePath -Filter '*.dll' -File
foreach ($fileName in @('COPYING.txt', 'COPYING.LIB', 'README.txt')) { $files += Get-Item -LiteralPath (Join-Path $runtimePath $fileName) }
# 已有运行目录也逐项比对官方归档，不能为改过或额外加入的插件生成可信清单。
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = [System.IO.Compression.ZipFile]::OpenRead($archivePath)
try {
    # 官方运行时中所有将被部署的组件必须存在，不能只为残缺目录中剩余的文件盖章。
    foreach ($entry in $archive.Entries) {
        $relative = $entry.FullName.Substring("vlc-$version/".Length)
        if ($relative -match '^(plugins|hrtfs|lua)/.+[^/]$' -or $relative -match '^[^/]+\.dll$') {
            if (!(Test-Path -LiteralPath (Join-Path $runtimePath $relative) -PathType Leaf)) { throw "Incomplete VLC runtime: $relative" }
        }
    }
    foreach ($file in $files | Where-Object { $_.Name -ne 'COPYING.LIB' }) {
        $relative = $file.FullName.Substring($runtimePath.Length + 1).Replace('\', '/')
        $entry = $archive.GetEntry("vlc-$version/$relative")
        if (!$entry) { throw "VLC file is absent from verified archive: $relative" }
        $stream = $entry.Open()
        $hasher = [System.Security.Cryptography.SHA256]::Create()
        try { $expectedHash = ([BitConverter]::ToString($hasher.ComputeHash($stream))).Replace('-', '') }
        finally { $stream.Dispose(); $hasher.Dispose() }
        if ((Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash -ne $expectedHash) { throw "VLC file differs from verified archive: $relative" }
    }
} finally { $archive.Dispose() }
$inventory = @($files | ForEach-Object {
    [PSCustomObject]@{ path = $_.FullName.Substring($runtimePath.Length + 1).Replace('\', '/'); sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash }
})
[PSCustomObject]@{
    version = $version; architecture = 'win64'; archiveUrl = $archiveUrl; archiveSha256 = $archiveSha256
    sourceUrl = $sourceUrl; sourceSha256 = $sourceSha256; files = $inventory
} | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $runtimePath 'runtime-provenance.json') -Encoding UTF8
Write-Output "VLC runtime: $runtimePath"
