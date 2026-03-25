$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $root
$lunara = Join-Path $root "build\Debug\lunara.exe"
$dbRoot = Join-Path $root "build\ecommerce_demo"
$dbPath = Join-Path $dbRoot "store.db"
$logDir = Join-Path $root "examples\logs"

New-Item -ItemType Directory -Force -Path $dbRoot | Out-Null
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
if (Test-Path $dbPath) {
    Remove-Item -Force $dbPath
}

$stdoutLog = Join-Path $logDir "ecommerce_backend.log"
$stderrLog = Join-Path $logDir "ecommerce_backend.err.log"
$server = Start-Process -FilePath $lunara -ArgumentList ".\examples\ecommerce_backend.lunara" -WorkingDirectory $root -PassThru -WindowStyle Hidden -RedirectStandardOutput $stdoutLog -RedirectStandardError $stderrLog

try {
    $ready = $false
    for ($i = 0; $i -lt 20; $i++) {
        $probe = curl.exe -s "http://127.0.0.1:8100/catalog"
        if ($LASTEXITCODE -eq 0 -and $probe) {
            $ready = $true
            break
        }
        Start-Sleep -Milliseconds 500
    }

    if (-not $ready) {
        throw "Ecommerce backend did not become ready"
    }

    $catalog = [string]::Join("`n", (curl.exe -s "http://127.0.0.1:8100/catalog"))
    if ($catalog -notmatch 'Lunara Blend') {
        throw "Catalog route did not return seeded products"
    }

    $registerHeaders = New-TemporaryFile
    $registerPayloadFile = Join-Path $dbRoot "ecommerce_register.json"
    '{"username":"buyer","password":"buyer-pass"}' | Set-Content -Path $registerPayloadFile -NoNewline
    $register = [string]::Join("`n", (curl.exe -s -i -c $registerHeaders.FullName -X POST -H "Content-Type: application/json" --data-binary "@$registerPayloadFile" "http://127.0.0.1:8100/register"))
    if ($register -notmatch "201 Created") {
        throw "Register route failed for ecommerce backend"
    }

    $orderPayloadFile = Join-Path $dbRoot "order_payload.json"
    '{"product_id":1,"quantity":2}' | Set-Content -Path $orderPayloadFile -NoNewline
    $order = [string]::Join("`n", (curl.exe -s -b $registerHeaders.FullName -X POST -H "Content-Type: application/json" --data-binary "@$orderPayloadFile" "http://127.0.0.1:8100/orders"))
    if ($order -notmatch '"status": "pending"') {
        throw "Order creation failed"
    }

    $myOrders = [string]::Join("`n", (curl.exe -s -b $registerHeaders.FullName "http://127.0.0.1:8100/orders/me"))
    if ($myOrders -notmatch '"quantity": 2') {
        throw "Customer order history failed"
    }

    $managerHeaders = New-TemporaryFile
    $managerPayloadFile = Join-Path $dbRoot "manager_login.json"
    '{"username":"manager","password":"manager-pass"}' | Set-Content -Path $managerPayloadFile -NoNewline
    $managerLogin = [string]::Join("`n", (curl.exe -s -i -c $managerHeaders.FullName -X POST -H "Content-Type: application/json" --data-binary "@$managerPayloadFile" "http://127.0.0.1:8100/login"))
    if ($managerLogin -notmatch "200 OK") {
        throw "Manager login failed"
    }

    $adminOrders = [string]::Join("`n", (curl.exe -s -b $managerHeaders.FullName "http://127.0.0.1:8100/admin/orders"))
    if ($adminOrders -notmatch '"user_id": 2') {
        throw "Admin orders route did not return created order"
    }

    $statusPayloadFile = Join-Path $dbRoot "status_update.json"
    '{"status":"paid"}' | Set-Content -Path $statusPayloadFile -NoNewline
    $statusUpdate = [string]::Join("`n", (curl.exe -s -b $managerHeaders.FullName -X POST -H "Content-Type: application/json" --data-binary "@$statusPayloadFile" "http://127.0.0.1:8100/admin/orders/1/status"))
    if ($statusUpdate -notmatch '"status": "paid"') {
        throw "Order status update failed"
    }

    $metrics = [string]::Join("`n", (curl.exe -s -b $managerHeaders.FullName "http://127.0.0.1:8100/admin/metrics"))
    if ($metrics -notmatch '"orders": 1') {
        throw "Metrics route did not report order count"
    }

    Write-Host "Ecommerce backend smoke test passed"
}
finally {
    if ($null -ne $server -and -not $server.HasExited) {
        Stop-Process -Id $server.Id -Force
    }
}
