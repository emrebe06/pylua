$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $root
$lunara = Join-Path $root "build\Debug\lunara.exe"
$dbRoot = Join-Path $root "build\fastapi_demo"
$dbPath = Join-Path $dbRoot "app.db"
$logDir = Join-Path $root "examples\logs"

New-Item -ItemType Directory -Force -Path $dbRoot | Out-Null
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
if (Test-Path $dbPath) {
    Remove-Item -Force $dbPath
}

$stdoutLog = Join-Path $logDir "fastapi_backend.log"
$stderrLog = Join-Path $logDir "fastapi_backend.err.log"
$server = Start-Process -FilePath $lunara -ArgumentList ".\examples\fastapi_style_backend.lunara" -WorkingDirectory $root -PassThru -WindowStyle Hidden -RedirectStandardOutput $stdoutLog -RedirectStandardError $stderrLog

try {
    $ready = $false
    for ($i = 0; $i -lt 20; $i++) {
        $probe = curl.exe -s "http://127.0.0.1:8110/health"
        if ($LASTEXITCODE -eq 0 -and $probe) {
            $ready = $true
            break
        }
        Start-Sleep -Milliseconds 500
    }

    if (-not $ready) {
        throw "FastAPI-style backend did not become ready"
    }

    $homePage = [string]::Join("`n", (curl.exe -s "http://127.0.0.1:8110/"))
    if ($homePage -notmatch "Lunara API") {
        throw "Template rendering failed"
    }

    $health = [string]::Join("`n", (curl.exe -s "http://127.0.0.1:8110/health"))
    if ($health -notmatch '"framework": "fastapi-style"') {
        throw "API health route failed"
    }

    $invalid = [string]::Join("`n", (curl.exe -s -X POST -H "Content-Type: application/json" --data-binary "{}" "http://127.0.0.1:8110/notes"))
    if ($invalid -notmatch '"missing_field"') {
        throw "JSON validation middleware failed"
    }

    $notePayload = Join-Path $dbRoot "note.json"
    '{"title":"first note"}' | Set-Content -Path $notePayload -NoNewline
    $created = [string]::Join("`n", (curl.exe -s -X POST -H "Content-Type: application/json" --data-binary "@$notePayload" "http://127.0.0.1:8110/notes"))
    if ($created -notmatch '"title": "first note"') {
        throw "Note creation failed"
    }

    $notes = [string]::Join("`n", (curl.exe -s "http://127.0.0.1:8110/notes"))
    if ($notes -notmatch '"items":') {
        throw "Note listing failed"
    }

    $checkout = [string]::Join("`n", (curl.exe -s -X POST "http://127.0.0.1:8110/checkout"))
    if ($checkout -notmatch '"provider": "mock_gateway"') {
        throw "Payment contract route failed"
    }

    Write-Host "FastAPI-style backend smoke test passed"
}
finally {
    if ($null -ne $server -and -not $server.HasExited) {
        Stop-Process -Id $server.Id -Force
    }
}
