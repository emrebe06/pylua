$ErrorActionPreference = "Stop"

$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$root = Split-Path -Parent $root
$lunara = Join-Path $root "build\Debug\lunara.exe"
$demoRoot = Join-Path $root "examples\live_bridge_demo"
$logs = Join-Path $demoRoot "logs"

New-Item -ItemType Directory -Force -Path $logs | Out-Null

$frontendLog = Join-Path $logs "frontend.log"
$frontendErr = Join-Path $logs "frontend.err.log"
$apiLog = Join-Path $logs "api.log"
$apiErr = Join-Path $logs "api.err.log"
$wsLog = Join-Path $logs "ws.log"
$wsErr = Join-Path $logs "ws.err.log"

$frontend = Start-Process -FilePath $lunara -ArgumentList ".\examples\live_bridge_demo\frontend_server.lunara" -WorkingDirectory $root -PassThru -WindowStyle Hidden -RedirectStandardOutput $frontendLog -RedirectStandardError $frontendErr
$api = Start-Process -FilePath $lunara -ArgumentList ".\examples\live_bridge_demo\api_server.lunara" -WorkingDirectory $root -PassThru -WindowStyle Hidden -RedirectStandardOutput $apiLog -RedirectStandardError $apiErr
$ws = Start-Process -FilePath $lunara -ArgumentList ".\examples\live_bridge_demo\websocket_server.lunara" -WorkingDirectory $root -PassThru -WindowStyle Hidden -RedirectStandardOutput $wsLog -RedirectStandardError $wsErr

try {
    $ready = $false
    for ($i = 0; $i -lt 20; $i++) {
        $probe = curl.exe -s "http://127.0.0.1:8093/health"
        if ($LASTEXITCODE -eq 0 -and $probe) {
            $ready = $true
            break
        }
        Start-Sleep -Milliseconds 500
    }
    if (-not $ready) {
        throw "API server did not become ready"
    }

    $health = [string]::Join("`n", (curl.exe -s -i -H "Origin: http://127.0.0.1:8081" "http://127.0.0.1:8093/health"))
    if ($health -notmatch "Access-Control-Allow-Origin: http://127.0.0.1:8081") {
        Write-Host $health
        throw "CORS allow-origin check failed"
    }

    $preflight = [string]::Join("`n", (curl.exe -s -i -X OPTIONS `
        -H "Origin: http://127.0.0.1:8081" `
        -H "Access-Control-Request-Method: POST" `
        -H "Access-Control-Request-Headers: Content-Type" `
        "http://127.0.0.1:8093/api/echo"))
    if ($preflight -notmatch "204 No Content") {
        throw "Preflight did not return 204"
    }

    $postBody = '{"ping":"pong"}'
    $echo = [string]::Join("`n", (curl.exe -s -i -X POST `
        -H "Origin: http://127.0.0.1:8081" `
        -H "Content-Type: application/json" `
        --data-raw $postBody `
        "http://127.0.0.1:8093/api/echo"))
    if ($echo -notmatch "pong") {
        Write-Host $echo
        throw "POST echo did not return JSON payload"
    }

    $socket = [System.Net.WebSockets.ClientWebSocket]::new()
    $uri = [Uri]"ws://127.0.0.1:8094"
    $null = $socket.ConnectAsync($uri, [Threading.CancellationToken]::None).GetAwaiter().GetResult()

    $buffer = New-Object byte[] 2048
    $segment = [ArraySegment[byte]]::new($buffer)

    $welcomeResult = $socket.ReceiveAsync($segment, [Threading.CancellationToken]::None).GetAwaiter().GetResult()
    $welcome = [System.Text.Encoding]::UTF8.GetString($buffer, 0, $welcomeResult.Count)
    if ($welcome -notmatch "welcome") {
        throw "WebSocket welcome message missing"
    }

    $payload = [System.Text.Encoding]::UTF8.GetBytes("bridge-smoke")
    $sendBuffer = [ArraySegment[byte]]::new($payload)
    $null = $socket.SendAsync($sendBuffer, [System.Net.WebSockets.WebSocketMessageType]::Text, $true, [Threading.CancellationToken]::None).GetAwaiter().GetResult()

    $replyResult = $socket.ReceiveAsync($segment, [Threading.CancellationToken]::None).GetAwaiter().GetResult()
    $reply = [System.Text.Encoding]::UTF8.GetString($buffer, 0, $replyResult.Count)
    if ($reply -notmatch "bridge-smoke") {
        throw "WebSocket echo failed"
    }

    $closeBuffer = [ArraySegment[byte]]::new([byte[]]::new(0))
    $null = $socket.CloseAsync([System.Net.WebSockets.WebSocketCloseStatus]::NormalClosure, "done", [Threading.CancellationToken]::None).GetAwaiter().GetResult()

    Write-Host "Smoke test passed"
    Write-Host "Health headers found"
    Write-Host "WebSocket reply:" $reply
}
finally {
    foreach ($proc in @($frontend, $api, $ws)) {
        if ($null -ne $proc -and -not $proc.HasExited) {
            Stop-Process -Id $proc.Id -Force
        }
    }
}
