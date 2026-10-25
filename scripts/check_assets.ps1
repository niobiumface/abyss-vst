# Checks Abyss asset files, dimensions and strip frame counts.
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$Repo = Split-Path -Parent $PSScriptRoot
$fail = 0

function Check-Image {
    param([string]$Rel, [int]$W, [int]$H)
    $path = Join-Path $Repo $Rel
    if (-not (Test-Path -LiteralPath $path)) {
        Write-Output ("MISSING  {0}" -f $Rel)
        $script:fail++
        return
    }
    $img = [System.Drawing.Image]::FromFile($path)
    $ok = ($img.Width -eq $W -and $img.Height -eq $H)
    $mark = $(if ($ok) { "OK" } else { "SIZE" })
    Write-Output ("{0,-6} {1,-32} {2}x{3}  expected {4}x{5}" -f $mark, $Rel, $img.Width, $img.Height, $W, $H)
    if (-not $ok) { $script:fail++ }
    $img.Dispose()
}

Write-Output "=== anim (11 levels x 3 frames, 112x74) ==="
foreach ($level in 0,10,20,30,40,50,60,70,80,90,100) {
    $tag = "{0:D3}" -f $level
    foreach ($f in 0,1,2) {
        Check-Image "anim\depth_${tag}_f$f.png" 112 74
    }
}

Write-Output "=== ui ==="
Check-Image "ui\ui_frame.png"            240 186
Check-Image "ui\knob_depth_strip.png"     24 1536
Check-Image "ui\knob_pressure_strip.png"  24 1536
Check-Image "ui\knob_small_strip.png"     12 384
Check-Image "ui\bypass_off.png"           20 16
Check-Image "ui\bypass_on.png"            20 16
Check-Image "ui\screen_off.png"           112 74

Write-Output "=== frames ==="
Write-Output "knob_depth_strip    64 frames of 24x24"
Write-Output "knob_pressure_strip 64 frames of 24x24"
Write-Output "knob_small_strip    32 frames of 12x12"

$palette = Join-Path $Repo "palette.json"
if (Test-Path -LiteralPath $palette) { Write-Output "OK     palette.json" }
else { Write-Output "MISSING  palette.json"; $fail++ }

if ($fail -gt 0) {
    Write-Output "FAILED $fail check(s)"
    exit 1
}
Write-Output "ALL ASSETS OK"
exit 0
