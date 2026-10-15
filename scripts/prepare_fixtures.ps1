param([string]$ToolDirectory = '')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
if (!$ToolDirectory) { $ToolDirectory = Join-Path $projectRoot 'tools\bin' }
$ffmpeg = Join-Path $ToolDirectory 'ffmpeg.exe'
$fixtures = Join-Path $projectRoot 'tests\fixtures'
New-Item -ItemType Directory -Force -Path $fixtures | Out-Null
function Invoke-Fixture([string[]]$Arguments) {
    & $ffmpeg -hide_banner -loglevel error -nostdin -y @Arguments
    if ($LASTEXITCODE -ne 0) { throw 'Fixture generation failed.' }
}
Invoke-Fixture @('-f','lavfi','-i','testsrc2=size=320x180:rate=24','-f','lavfi','-i','sine=frequency=440:sample_rate=48000','-t','1.2','-c:v','libx264','-preset','ultrafast','-pix_fmt','yuv420p','-c:a','aac',(Join-Path $fixtures '中文 sample.mp4'))
Invoke-Fixture @('-f','lavfi','-i','testsrc2=size=1080x1920:rate=5','-t','0.8','-c:v','libx264','-preset','ultrafast','-pix_fmt','yuv420p',(Join-Path $fixtures 'portrait.mp4'))
Invoke-Fixture @('-f','lavfi','-i','testsrc=size=321x181:rate=5','-t','0.8','-c:v','ffv1',(Join-Path $fixtures 'odd.mkv'))
Invoke-Fixture @('-f','lavfi','-i','testsrc2=size=1920x1080:rate=30','-t','12','-c:v','libx264','-preset','ultrafast','-pix_fmt','yuv420p',(Join-Path $fixtures 'long.mp4'))
Invoke-Fixture @('-i',(Join-Path $fixtures '中文 sample.mp4'),'-c','copy','-metadata:s:v:0','rotate=90',(Join-Path $fixtures 'rotated.mp4'))
Invoke-Fixture @('-f','lavfi','-i','testsrc2=size=320x180:rate=5','-t','0.8','-vf','setsar=2','-c:v','libx264','-preset','ultrafast',(Join-Path $fixtures 'anamorphic.mp4'))
Invoke-Fixture @('-f','lavfi','-i','testsrc2=size=320x180:rate=5','-t','0.8','-c:v','libx264','-preset','ultrafast','-x264-params','colorprim=bt2020:transfer=smpte2084:colormatrix=bt2020nc',(Join-Path $fixtures 'hdr.mp4'))
'not a video' | Set-Content -LiteralPath (Join-Path $fixtures 'damaged.mp4') -Encoding UTF8
# 长 GOP 配合颜色和频率分段，验证非关键帧截取的位置与对应音频内容。
Invoke-Fixture @('-f','lavfi','-i','color=red:size=160x90:rate=24:duration=2','-f','lavfi','-i','color=green:size=160x90:rate=24:duration=2','-f','lavfi','-i','color=blue:size=160x90:rate=24:duration=2','-f','lavfi','-i','aevalsrc=sin(2*PI*if(lt(t\,2)\,440\,if(lt(t\,4)\,880\,1320))*t):s=48000:d=6','-filter_complex','[0:v][1:v][2:v]concat=n=3:v=1:a=0[v]','-map','[v]','-map','3:a','-c:v','libx264','-g','144','-keyint_min','144','-sc_threshold','0','-pix_fmt','yuv420p','-c:a','aac',(Join-Path $fixtures 'marked.mp4'))
Invoke-Fixture @('-i',(Join-Path $fixtures 'marked.mp4'),'-c','copy','-output_ts_offset','7','-f','mpegts',(Join-Path $fixtures 'marked-offset.ts'))
# Qt DirectShow 在当前机器可播放的基线，不假定 H.264 MP4 可预览。
Invoke-Fixture @('-i',(Join-Path $fixtures 'marked.mp4'),'-c:v','mjpeg','-c:a','pcm_s16le',(Join-Path $fixtures 'preview.avi'))
Invoke-Fixture @('-i',(Join-Path $fixtures 'preview.avi'),'-vf','transpose=1','-an','-c:v','mjpeg',(Join-Path $fixtures 'preview-portrait.avi'))
Write-Output "Fixtures: $fixtures"
