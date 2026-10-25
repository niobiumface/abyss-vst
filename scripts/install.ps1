param(
    [string] $Repo = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
)

$ErrorActionPreference = "Stop"
$Src = Join-Path $Repo "build\Abyss_artefacts\Release\VST3\Abyss.vst3"
$BinaryName = "Abyss.vst3\Contents\x86_64-win\Abyss.vst3"

if (-not (Test-Path -LiteralPath (Join-Path $Src "Contents\x86_64-win\Abyss.vst3"))) {
    throw "Release-Bundle fehlt: $Src"
}

$User = Join-Path $env:LOCALAPPDATA "Programs\Common\VST3"
New-Item -ItemType Directory -Force -Path $User | Out-Null
$UserBundle = Join-Path $User "Abyss.vst3"
if (Test-Path -LiteralPath $UserBundle) {
    Remove-Item -LiteralPath $UserBundle -Recurse -Force
}
Copy-Item -LiteralPath $Src -Destination $User -Recurse -Force

$systemScript = Join-Path $PSScriptRoot "install_system.ps1"
$elevated = Start-Process -FilePath "powershell.exe" -Verb RunAs -Wait -PassThru -ArgumentList @(
    "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $systemScript, "-Src", $Src
)

function Show-Hash([string] $label, [string] $path) {
    if (-not (Test-Path -LiteralPath $path)) {
        Write-Host "$label fehlt: $path"
        return
    }
    $file = Get-Item -LiteralPath $path
    $hash = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash
    Write-Host "$label"
    Write-Host "  $path"
    Write-Host "  $($file.Length) bytes  $hash"
}

$srcBin = Join-Path $Src "Contents\x86_64-win\Abyss.vst3"
Show-Hash "Quelle" $srcBin
Show-Hash "Benutzer" (Join-Path $UserBundle "Contents\x86_64-win\Abyss.vst3")
Show-Hash "System" "C:\Program Files\Common Files\VST3\Abyss.vst3\Contents\x86_64-win\Abyss.vst3"

if ($null -eq $elevated -or $elevated.ExitCode -ne 0) {
    Write-Host "System-Kopie nicht bestätigt (Exit $($elevated.ExitCode))."
    exit 1
}
exit 0
