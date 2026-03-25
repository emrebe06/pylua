$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $root
$lunara = Join-Path $root "build\Debug\lunara.exe"
$logDir = Join-Path $root "examples\logs"

New-Item -ItemType Directory -Force -Path $logDir | Out-Null

$stdoutLog = Join-Path $logDir "full_backend.log"
$stderrLog = Join-Path $logDir "full_backend.err.log"

$server = Start-Process -FilePath $lunara -ArgumentList ".\examples\full_web_backend.lunara" -WorkingDirectory $root -PassThru -WindowStyle Hidden -RedirectStandardOutput $stdoutLog -RedirectStandardError $stderrLog

try {
    $ready = $false
    for ($i = 0; $i -lt 20; $i++) {
        $probe = curl.exe -s "http://127.0.0.1:8096/health"
        if ($LASTEXITCODE -eq 0 -and $probe) {
            $ready = $true
            break
        }
        Start-Sleep -Milliseconds 500
    }

    if (-not $ready) {
        throw "Backend server did not become ready"
    }

    $socket = [System.Net.WebSockets.ClientWebSocket]::new()
    $uri = [Uri]"ws://127.0.0.1:8096/ws"
    $null = $socket.ConnectAsync($uri, [Threading.CancellationToken]::None).GetAwaiter().GetResult()

    $buffer = New-Object byte[] 2048
    $segment = [ArraySegment[byte]]::new($buffer)

    $welcomeResult = $socket.ReceiveAsync($segment, [Threading.CancellationToken]::None).GetAwaiter().GetResult()
    $welcome = [System.Text.Encoding]::UTF8.GetString($buffer, 0, $welcomeResult.Count)
    if ($welcome -notmatch '"kind":"open"') {
        throw "WebSocket open handler missing"
    }

    $payload = [System.Text.Encoding]::UTF8.GetBytes("backend-smoke")
    $sendBuffer = [ArraySegment[byte]]::new($payload)
    $null = $socket.SendAsync($sendBuffer, [System.Net.WebSockets.WebSocketMessageType]::Text, $true, [Threading.CancellationToken]::None).GetAwaiter().GetResult()

    $replyResult = $socket.ReceiveAsync($segment, [Threading.CancellationToken]::None).GetAwaiter().GetResult()
    $reply = [System.Text.Encoding]::UTF8.GetString($buffer, 0, $replyResult.Count)
    if ($reply -notmatch "backend-smoke") {
        throw "WebSocket message route failed"
    }

    $null = $socket.CloseAsync([System.Net.WebSockets.WebSocketCloseStatus]::NormalClosure, "done", [Threading.CancellationToken]::None).GetAwaiter().GetResult()

    $index = [string]::Join("`n", (curl.exe -s "http://127.0.0.1:8096/"))
    if ($index -notmatch "Lunara full backend") {
        throw "Static mount did not serve index.html"
    }

    $health = [string]::Join("`n", (curl.exe -s -i -H "Origin: http://127.0.0.1:8096" "http://127.0.0.1:8096/health"))
    if ($health -notmatch "Access-Control-Allow-Origin: http://127.0.0.1:8096") {
        throw "Configured CORS origin missing"
    }

    $login = [string]::Join("`n", (curl.exe -s -i -X POST `
        -H "Origin: http://127.0.0.1:8096" `
        -H "Content-Type: application/json" `
        --data-raw '{"role":"admin"}' `
        "http://127.0.0.1:8096/login"))
    if ($login -notmatch "Set-Cookie: session=lunara-backend; Path=/") {
        throw "Cookie helper did not set session header"
    }

    $profile = [string]::Join("`n", (curl.exe -s "http://127.0.0.1:8096/api/users/7"))
    if ($profile -notmatch '"user_id": "7"') {
        throw "Route params did not resolve"
    }

    Write-Host "Full backend smoke test passed"
    Write-Host "WebSocket reply:" $reply
}
finally {
    if ($null -ne $server -and -not $server.HasExited) {
        Stop-Process -Id $server.Id -Force
    }
}
