param(
    [string]$Version = "0.1.0",
    [string]$BuildDir = "build-installer",
    [string]$OutDir = "dist\windows"
)

$ErrorActionPreference = "Stop"

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
$buildDir = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $BuildDir))
$outDir = [System.IO.Path]::GetFullPath((Join-Path $repoRoot $OutDir))
$stageRoot = Join-Path $outDir "stage"
$payloadDir = Join-Path $stageRoot "payload"
$bundleDir = Join-Path $stageRoot "bundle"
$installerExe = Join-Path $outDir ("PyLua-Setup-" + $Version + ".exe")
$sedPath = Join-Path $stageRoot "PyLuaSetup.sed"

Write-Host "Configuring Release build..."
cmake -S $repoRoot -B $buildDir | Out-Host

Write-Host "Building Release artifacts..."
cmake --build $buildDir --config Release | Out-Host

if (Test-Path $stageRoot) {
    Remove-Item -Recurse -Force $stageRoot
}
New-Item -ItemType Directory -Force -Path $payloadDir | Out-Null
New-Item -ItemType Directory -Force -Path $bundleDir | Out-Null
New-Item -ItemType Directory -Force -Path $outDir | Out-Null

Write-Host "Installing staged payload..."
cmake --install $buildDir --config Release --prefix $payloadDir | Out-Host

Compress-Archive -Path (Join-Path $payloadDir "*") -DestinationPath (Join-Path $bundleDir "payload.zip") -Force
Copy-Item (Join-Path $PSScriptRoot "install.ps1") (Join-Path $bundleDir "install.ps1") -Force
Copy-Item (Join-Path $PSScriptRoot "uninstall.ps1") (Join-Path $bundleDir "uninstall.ps1") -Force
Copy-Item (Join-Path $PSScriptRoot "install.cmd") (Join-Path $bundleDir "install.cmd") -Force
Copy-Item (Join-Path $PSScriptRoot "uninstall.cmd") (Join-Path $bundleDir "uninstall.cmd") -Force
Copy-Item (Join-Path $PSScriptRoot "LICENSE.txt") (Join-Path $bundleDir "LICENSE.txt") -Force
Set-Content -Path (Join-Path $bundleDir "VERSION.txt") -Value $Version -Encoding ASCII

$files = Get-ChildItem -Path $bundleDir -Recurse -File | Sort-Object FullName
$relativeFiles = foreach ($file in $files) {
    $file.FullName.Substring($bundleDir.Length + 1)
}

$stringLines = @()
$sourceLines = @()
for ($i = 0; $i -lt $relativeFiles.Count; $i++) {
    $name = "FILE$($i)"
    $stringLines += "$name=`"$($relativeFiles[$i])`""
    $sourceLines += "%$name%="
}

$sed = @"
[Version]
Class=IEXPRESS
SEDVersion=3
[Options]
PackagePurpose=InstallApp
ShowInstallProgramWindow=1
HideExtractAnimation=0
UseLongFileName=1
InsideCompressed=0
CAB_FixedSize=0
CAB_ResvCodeSigning=0
RebootMode=N
InstallPrompt=
DisplayLicense=
FinishMessage=PyLua installation package is ready.
TargetName=$installerExe
FriendlyName=PyLua Setup $Version
AppLaunched=install.cmd
PostInstallCmd=<None>
AdminQuietInstCmd=install.cmd -Quiet
UserQuietInstCmd=install.cmd -Quiet
SourceFiles=SourceFiles
[SourceFiles]
SourceFiles0=$bundleDir\
[SourceFiles0]
$(($sourceLines -join "`r`n"))
[Strings]
$(($stringLines -join "`r`n"))
"@

Set-Content -Path $sedPath -Value $sed -Encoding ASCII

Write-Host "Building setup executable with IExpress..."
& iexpress.exe /N $sedPath | Out-Host

if (-not (Test-Path $installerExe)) {
    throw "Installer was not generated: $installerExe"
}

Write-Host "Installer ready:"
Write-Host $installerExe
