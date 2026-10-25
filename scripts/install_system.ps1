param(
    [Parameter(Mandatory = $true)]
    [string] $Src
)

$ErrorActionPreference = "Stop"
$log = Join-Path $env:TEMP "abyss_install.log"
$root = "C:\Program Files\Common Files\VST3"
$dst = Join-Path $root "Abyss.vst3"

New-Item -ItemType Directory -Force -Path $root | Out-Null
if (Test-Path -LiteralPath $dst) {
    Remove-Item -LiteralPath $dst -Recurse -Force
}

& robocopy $Src $dst /MIR /NJH /NJS /NP /LOG:$log
$code = $LASTEXITCODE
if ($code -ge 8) {
    exit $code
}
exit 0
