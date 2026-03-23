param(
    [string]$InstallDir = ""
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($InstallDir)) {
    if ($env:LUNARA_HOME) {
        $InstallDir = $env:LUNARA_HOME
    } else {
        $InstallDir = Join-Path $env:LOCALAPPDATA "Programs\Lunara"
    }
}

$installDir = [System.IO.Path]::GetFullPath($InstallDir)
$binDir = Join-Path $installDir "bin"
$startMenuDir = Join-Path $env:APPDATA "Microsoft\Windows\Start Menu\Programs\Lunara"
$manifestPath = Join-Path $installDir "install_manifest.json"
$manifest = $null

if (Test-Path $manifestPath) {
    try {
        $manifest = Get-Content $manifestPath -Raw | ConvertFrom-Json
    } catch {
        $manifest = $null
    }
}

$currentPath = [Environment]::GetEnvironmentVariable("Path", "User")
if ($currentPath) {
    $entries = @($currentPath -split ';' | Where-Object { $_ -and $_.Trim() -ne "" -and $_ -ne $binDir })
    [Environment]::SetEnvironmentVariable("Path", ($entries -join ';'), "User")
}

[Environment]::SetEnvironmentVariable("LUNARA_HOME", $null, "User")

if (Test-Path $startMenuDir) {
    Remove-Item -Path $startMenuDir -Recurse -Force
}

if (Test-Path $installDir) {
    Remove-Item -Path $installDir -Recurse -Force
}

if ($manifest -and $manifest.version) {
    Write-Host ("Lunara " + $manifest.version + " removed from " + $installDir)
} else {
    Write-Host "Lunara removed from $installDir"
}

