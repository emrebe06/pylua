$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$root = Split-Path -Parent $root
$lunara = Join-Path $root "build\Debug\lunara.exe"
$dbRoot = Join-Path $root "build\auth_demo"
$dbPath = Join-Path $dbRoot "auth.db"
$logDir = Join-Path $root "examples\logs"

New-Item -ItemType Directory -Force -Path $dbRoot | Out-Null
New-Item -ItemType Directory -Force -Path $logDir | Out-Null
if (Test-Path $dbPath) {
    Remove-Item -Force $dbPath
}

$stdoutLog = Join-Path $logDir "auth_backend.log"
$stderrLog = Join-Path $logDir "auth_backend.err.log"
$payloadFile = Join-Path $dbRoot "auth_payload.json"
'{"username":"demo","password":"demo-pass","role":"manager"}' | Set-Content -Path $payloadFile -NoNewline

$server = Start-Process -FilePath $lunara -ArgumentList ".\examples\auth_backend.lunara" -WorkingDirectory $root -PassThru -WindowStyle Hidden -RedirectStandardOutput $stdoutLog -RedirectStandardError $stderrLog

try {
    $ready = $false
    for ($i = 0; $i -lt 20; $i++) {
        $probe = curl.exe -s "http://127.0.0.1:8098/protected"
        if ($LASTEXITCODE -eq 0 -and $probe) {
            $ready = $true
            break
        }
        Start-Sleep -Milliseconds 500
    }

    if (-not $ready) {
        throw "Auth backend did not become ready"
    }

    $registerHeaders = New-TemporaryFile
    $register = [string]::Join("`n", (curl.exe -s -i -c $registerHeaders.FullName -X POST `
        -H "Origin: http://127.0.0.1:8098" `
        -H "Content-Type: application/json" `
        --data-binary "@$payloadFile" `
        "http://127.0.0.1:8098/register"))
    if ($register -notmatch "201 Created") {
        Write-Host $register
        throw "Register route failed"
    }
    if ($register -notmatch "Set-Cookie: session=") {
        throw "Register did not issue session cookie"
    }

    $me = [string]::Join("`n", (curl.exe -s -b $registerHeaders.FullName "http://127.0.0.1:8098/me"))
    if ($me -notmatch '"username": "demo"') {
        throw "Session middleware did not attach current user"
    }

    $protected = [string]::Join("`n", (curl.exe -s -b $registerHeaders.FullName "http://127.0.0.1:8098/protected"))
    if ($protected -notmatch 'lunara-protected-area') {
        throw "Protected route did not allow authenticated user"
    }

    $admin = [string]::Join("`n", (curl.exe -s -b $registerHeaders.FullName "http://127.0.0.1:8098/admin"))
    if ($admin -notmatch '"area": "admin"') {
        throw "Role/permission guard did not allow manager"
    }

    $verifyStart = [string]::Join("`n", (curl.exe -s -b $registerHeaders.FullName -X POST "http://127.0.0.1:8098/verify-email/start"))
    if ($verifyStart -notmatch '"token": "') {
        throw "Email verification stub did not issue token"
    }
    $verifyToken = [regex]::Match($verifyStart, '"token": "([^"]+)"').Groups[1].Value
    $verifyFinish = [string]::Join("`n", (curl.exe -s "http://127.0.0.1:8098/verify-email/$verifyToken"))
    if ($verifyFinish -notmatch '"email_verified": true') {
        throw "Email verification did not mark user as verified"
    }

    $resetStartBody = Join-Path $dbRoot "reset_request.json"
    '{"username":"demo"}' | Set-Content -Path $resetStartBody -NoNewline
    $resetStart = [string]::Join("`n", (curl.exe -s -X POST -H "Content-Type: application/json" --data-binary "@$resetStartBody" "http://127.0.0.1:8098/password-reset/start"))
    if ($resetStart -notmatch '"token": "') {
        throw "Password reset stub did not issue token"
    }
    $resetToken = [regex]::Match($resetStart, '"token": "([^"]+)"').Groups[1].Value
    $resetFinishBody = Join-Path $dbRoot "reset_finish.json"
    '{"password":"demo-pass-2"}' | Set-Content -Path $resetFinishBody -NoNewline
    $resetFinish = [string]::Join("`n", (curl.exe -s -X POST -H "Content-Type: application/json" --data-binary "@$resetFinishBody" "http://127.0.0.1:8098/password-reset/$resetToken"))
    if ($resetFinish -notmatch '"ok": true') {
        throw "Password reset finish failed"
    }

    $logout = [string]::Join("`n", (curl.exe -s -i -b $registerHeaders.FullName -X POST "http://127.0.0.1:8098/logout"))
    if ($logout -notmatch "Set-Cookie: session=; Path=/; Max-Age=0") {
        throw "Logout did not clear cookie"
    }

    $loginHeaders = New-TemporaryFile
    $loginPayloadFile = Join-Path $dbRoot "login_payload.json"
    '{"username":"demo","password":"demo-pass-2"}' | Set-Content -Path $loginPayloadFile -NoNewline
    $login = [string]::Join("`n", (curl.exe -s -i -c $loginHeaders.FullName -X POST `
        -H "Content-Type: application/json" `
        --data-binary "@$loginPayloadFile" `
        "http://127.0.0.1:8098/login"))
    if ($login -notmatch "200 OK") {
        throw "Login route failed"
    }

    $unauthorized = [string]::Join("`n", (curl.exe -s "http://127.0.0.1:8098/protected"))
    if ($unauthorized -notmatch 'unauthorized') {
        throw "Protected route should reject anonymous user"
    }

    $authorized = [string]::Join("`n", (curl.exe -s -b $loginHeaders.FullName "http://127.0.0.1:8098/protected"))
    if ($authorized -notmatch '"ok": true') {
        throw "Protected route should accept logged-in user"
    }

    $tokenPayload = [string]::Join("`n", (curl.exe -s -b $loginHeaders.FullName "http://127.0.0.1:8098/issue-token"))
    $bearerToken = [regex]::Match($tokenPayload, '"token": "([^"]+)"').Groups[1].Value
    if (-not $bearerToken) {
        throw "Signed bearer token was not issued"
    }

    $bearerArea = [string]::Join("`n", (curl.exe -s -H "Authorization: Bearer $bearerToken" "http://127.0.0.1:8098/bearer-area"))
    if ($bearerArea -notmatch '"mode": "bearer"') {
        throw "Bearer middleware did not attach user"
    }

    $users = [string]::Join("`n", (curl.exe -s -b $loginHeaders.FullName "http://127.0.0.1:8098/users"))
    if ($users -notmatch '"role": "manager"') {
        throw "General sqluna query API did not return user rows"
    }

    Write-Host "Auth backend smoke test passed"
}
finally {
    if ($null -ne $server -and -not $server.HasExited) {
        Stop-Process -Id $server.Id -Force
    }
}
