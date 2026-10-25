param(
    [ValidateSet("dev", "release", "sanitize", "dev-vs")]
    [string]$Preset = "dev"
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot

Push-Location $repoRoot
try {
    cmake --preset $Preset
    cmake --build --preset $Preset
    ctest --preset $Preset --output-on-failure
}
finally {
    Pop-Location
}
