param(
    [string]$InstallDir = ""
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($InstallDir)) {
    if ($env:PYLUA_HOME) {
        $InstallDir = $env:PYLUA_HOME
    } else {
        $InstallDir = Join-Path $env:LOCALAPPDATA "Programs\PyLua"
    }
}

$installDir = [System.IO.Path]::GetFullPath($InstallDir)
$binDir = Join-Path $installDir "bin"
$startMenuDir = Join-Path $env:APPDATA "Microsoft\Windows\Start Menu\Programs\PyLua"
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

[Environment]::SetEnvironmentVariable("PYLUA_HOME", $null, "User")

if (Test-Path $startMenuDir) {
    Remove-Item -Path $startMenuDir -Recurse -Force
}

if (Test-Path $installDir) {
    Remove-Item -Path $installDir -Recurse -Force
}

if ($manifest -and $manifest.version) {
    Write-Host ("PyLua " + $manifest.version + " removed from " + $installDir)
} else {
    Write-Host "PyLua removed from $installDir"
}
