param(
    [string]$InstallDir = "",
    [switch]$Quiet
)

$ErrorActionPreference = "Stop"

function Write-Step {
    param([string]$Message)
    if (-not $Quiet) {
        Write-Host $Message
    }
}

function Get-SourceFile {
    param([string]$Name)
    return Join-Path $script:SourceRoot $Name
}

function Get-DefaultInstallDir {
    if (-not [string]::IsNullOrWhiteSpace($script:RequestedInstallDir)) {
        return [System.IO.Path]::GetFullPath($script:RequestedInstallDir)
    }
    if ($env:PYLUA_INSTALL_DIR) {
        return [System.IO.Path]::GetFullPath($env:PYLUA_INSTALL_DIR)
    }
    if ($env:PYLUA_HOME) {
        return [System.IO.Path]::GetFullPath($env:PYLUA_HOME)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $env:LOCALAPPDATA "Programs\PyLua"))
}

function Get-InstallerVersion {
    $versionFile = Get-SourceFile "VERSION.txt"
    if (Test-Path $versionFile) {
        return (Get-Content $versionFile -Raw).Trim()
    }
    return "0.1.0"
}

function Get-LicenseText {
    $licenseFile = Get-SourceFile "LICENSE.txt"
    if (Test-Path $licenseFile) {
        return Get-Content $licenseFile -Raw
    }
    return "PyLua Preview License`r`n`r`nThis build is provided as a preview runtime for evaluation and development use."
}

function Get-ManifestPath {
    param([string]$TargetDir)
    return Join-Path $TargetDir "install_manifest.json"
}

function Read-InstallManifest {
    param([string]$TargetDir)
    $manifestPath = Get-ManifestPath $TargetDir
    if (-not (Test-Path $manifestPath)) {
        return $null
    }
    try {
        return Get-Content $manifestPath -Raw | ConvertFrom-Json
    } catch {
        return $null
    }
}

function Get-InstallMode {
    param(
        [string]$TargetDir,
        [string]$NewVersion
    )

    $manifest = Read-InstallManifest $TargetDir
    $binExe = Join-Path $TargetDir "bin\pylua.exe"

    if (-not $manifest -and -not (Test-Path $binExe)) {
        return @{
            Mode = "install"
            Label = "Fresh install"
            Details = "PyLua bu klasore ilk kez kurulacak."
            ExistingVersion = ""
        }
    }

    $existingVersion = ""
    if ($manifest -and $manifest.version) {
        $existingVersion = [string]$manifest.version
    }

    if ([string]::IsNullOrWhiteSpace($existingVersion)) {
        return @{
            Mode = "repair"
            Label = "Repair / overwrite"
            Details = "Bu klasorde mevcut bir PyLua kurulumu bulundu. Dosyalar yenilenerek tekrar kurulacak."
            ExistingVersion = ""
        }
    }

    try {
        $newSemVer = [version]$NewVersion
        $oldSemVer = [version]$existingVersion
        if ($oldSemVer -lt $newSemVer) {
            return @{
                Mode = "upgrade"
                Label = "Upgrade"
                Details = "Mevcut PyLua $existingVersion kurulumu bulundu. $NewVersion surumune upgrade yapilacak."
                ExistingVersion = $existingVersion
            }
        }
        if ($oldSemVer -gt $newSemVer) {
            return @{
                Mode = "downgrade"
                Label = "Downgrade"
                Details = "Mevcut PyLua $existingVersion kurulumu bulundu. $NewVersion surumune geri alinacak."
                ExistingVersion = $existingVersion
            }
        }
    } catch {
    }

    return @{
        Mode = "repair"
        Label = "Reinstall"
        Details = "PyLua $existingVersion zaten kurulu. Bu kurulum ayni surumu yeniden kuracak."
        ExistingVersion = $existingVersion
    }
}

function Ensure-PathSetting {
    param(
        [string]$BinDir,
        [bool]$Enabled
    )

    $currentPath = [Environment]::GetEnvironmentVariable("Path", "User")
    if (-not $currentPath) {
        $currentPath = ""
    }

    $entries = New-Object System.Collections.Generic.List[string]
    foreach ($entry in ($currentPath -split ';')) {
        if (-not [string]::IsNullOrWhiteSpace($entry) -and $entry -ne $BinDir) {
            $entries.Add($entry)
        }
    }

    if ($Enabled) {
        $entries.Add($BinDir)
    }

    [Environment]::SetEnvironmentVariable("Path", ($entries -join ';'), "User")
}

function Write-PyLuaIcon {
    param([string]$Path)

    Add-Type -AssemblyName System.Drawing

    $bitmap = New-Object System.Drawing.Bitmap 64, 64
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $graphics.Clear([System.Drawing.Color]::FromArgb(19, 24, 36))

    $backgroundBrush = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
        (New-Object System.Drawing.Rectangle 0, 0, 64, 64),
        [System.Drawing.Color]::FromArgb(232, 113, 37),
        [System.Drawing.Color]::FromArgb(247, 167, 59),
        45.0
    )
    $graphics.FillEllipse($backgroundBrush, 8, 8, 48, 48)

    $font = New-Object System.Drawing.Font "Segoe UI", 28, ([System.Drawing.FontStyle]::Bold), ([System.Drawing.GraphicsUnit]::Pixel)
    $stringFormat = New-Object System.Drawing.StringFormat
    $stringFormat.Alignment = [System.Drawing.StringAlignment]::Center
    $stringFormat.LineAlignment = [System.Drawing.StringAlignment]::Center
    $graphics.DrawString("P", $font, [System.Drawing.Brushes]::White, (New-Object System.Drawing.RectangleF 0, 0, 64, 64), $stringFormat)

    $icon = [System.Drawing.Icon]::FromHandle($bitmap.GetHicon())
    $stream = [System.IO.File]::Open($Path, [System.IO.FileMode]::Create, [System.IO.FileAccess]::Write)
    try {
        $icon.Save($stream)
    } finally {
        $stream.Dispose()
        $icon.Dispose()
        $font.Dispose()
        $backgroundBrush.Dispose()
        $graphics.Dispose()
        $bitmap.Dispose()
    }
}

function Show-InstallerWizard {
    param(
        [string]$Version,
        [string]$DefaultInstallDir,
        [string]$LicenseText
    )

    Add-Type -AssemblyName System.Windows.Forms
    Add-Type -AssemblyName System.Drawing

    $tempIconPath = Join-Path ([System.IO.Path]::GetTempPath()) ("pylua-installer-" + [guid]::NewGuid().ToString("N") + ".ico")
    Write-PyLuaIcon $tempIconPath

    $form = New-Object System.Windows.Forms.Form
    $form.Text = "PyLua Setup"
    $form.StartPosition = "CenterScreen"
    $form.FormBorderStyle = "FixedDialog"
    $form.MaximizeBox = $false
    $form.MinimizeBox = $false
    $form.ClientSize = New-Object System.Drawing.Size 760, 500
    $form.BackColor = [System.Drawing.Color]::White
    $form.Icon = New-Object System.Drawing.Icon $tempIconPath

    $sidebar = New-Object System.Windows.Forms.Panel
    $sidebar.Dock = "Left"
    $sidebar.Width = 220
    $sidebar.BackColor = [System.Drawing.Color]::FromArgb(24, 31, 48)
    $form.Controls.Add($sidebar)

    $brandTitle = New-Object System.Windows.Forms.Label
    $brandTitle.Text = "PyLua"
    $brandTitle.ForeColor = [System.Drawing.Color]::White
    $brandTitle.Font = New-Object System.Drawing.Font "Segoe UI", 22, ([System.Drawing.FontStyle]::Bold)
    $brandTitle.Location = New-Object System.Drawing.Point 28, 36
    $brandTitle.AutoSize = $true
    $sidebar.Controls.Add($brandTitle)

    $brandSub = New-Object System.Windows.Forms.Label
    $brandSub.Text = "Native scripting engine`r`nwith installer wizard"
    $brandSub.ForeColor = [System.Drawing.Color]::FromArgb(210, 218, 230)
    $brandSub.Font = New-Object System.Drawing.Font "Segoe UI", 10
    $brandSub.Location = New-Object System.Drawing.Point 30, 88
    $brandSub.Size = New-Object System.Drawing.Size 160, 52
    $sidebar.Controls.Add($brandSub)

    $stepLabel = New-Object System.Windows.Forms.Label
    $stepLabel.ForeColor = [System.Drawing.Color]::FromArgb(247, 167, 59)
    $stepLabel.Font = New-Object System.Drawing.Font "Segoe UI", 10, ([System.Drawing.FontStyle]::Bold)
    $stepLabel.Location = New-Object System.Drawing.Point 30, 180
    $stepLabel.Size = New-Object System.Drawing.Size 160, 100
    $sidebar.Controls.Add($stepLabel)

    $content = New-Object System.Windows.Forms.Panel
    $content.Location = New-Object System.Drawing.Point 220, 0
    $content.Size = New-Object System.Drawing.Size 540, 440
    $form.Controls.Add($content)

    $buttonPanel = New-Object System.Windows.Forms.Panel
    $buttonPanel.Dock = "Bottom"
    $buttonPanel.Height = 60
    $buttonPanel.BackColor = [System.Drawing.Color]::FromArgb(246, 247, 249)
    $form.Controls.Add($buttonPanel)

    $backButton = New-Object System.Windows.Forms.Button
    $backButton.Text = "< Back"
    $backButton.Location = New-Object System.Drawing.Point 460, 16
    $backButton.Size = New-Object System.Drawing.Size 85, 30
    $buttonPanel.Controls.Add($backButton)

    $nextButton = New-Object System.Windows.Forms.Button
    $nextButton.Text = "Next >"
    $nextButton.Location = New-Object System.Drawing.Point 552, 16
    $nextButton.Size = New-Object System.Drawing.Size 85, 30
    $buttonPanel.Controls.Add($nextButton)

    $cancelButton = New-Object System.Windows.Forms.Button
    $cancelButton.Text = "Cancel"
    $cancelButton.Location = New-Object System.Drawing.Point 644, 16
    $cancelButton.Size = New-Object System.Drawing.Size 85, 30
    $buttonPanel.Controls.Add($cancelButton)

    $welcomePanel = New-Object System.Windows.Forms.Panel
    $welcomePanel.Dock = "Fill"
    $content.Controls.Add($welcomePanel)

    $welcomeTitle = New-Object System.Windows.Forms.Label
    $welcomeTitle.Text = "Welcome to PyLua Setup"
    $welcomeTitle.Font = New-Object System.Drawing.Font "Segoe UI", 18, ([System.Drawing.FontStyle]::Bold)
    $welcomeTitle.Location = New-Object System.Drawing.Point 28, 30
    $welcomeTitle.AutoSize = $true
    $welcomePanel.Controls.Add($welcomeTitle)

    $welcomeBody = New-Object System.Windows.Forms.Label
    $welcomeBody.Text = "This wizard will install PyLua " + $Version + " on your computer."
    $welcomeBody.Font = New-Object System.Drawing.Font "Segoe UI", 10
    $welcomeBody.Location = New-Object System.Drawing.Point 30, 82
    $welcomeBody.Size = New-Object System.Drawing.Size 460, 40
    $welcomePanel.Controls.Add($welcomeBody)

    $welcomeDetails = New-Object System.Windows.Forms.Label
    $welcomeDetails.Text = "You can choose the installation folder, review the license, and decide whether PyLua should be added to PATH."
    $welcomeDetails.Font = New-Object System.Drawing.Font "Segoe UI", 10
    $welcomeDetails.Location = New-Object System.Drawing.Point 30, 126
    $welcomeDetails.Size = New-Object System.Drawing.Size 470, 60
    $welcomePanel.Controls.Add($welcomeDetails)

    $welcomeStatus = New-Object System.Windows.Forms.Label
    $welcomeStatus.Font = New-Object System.Drawing.Font "Segoe UI", 10, ([System.Drawing.FontStyle]::Bold)
    $welcomeStatus.ForeColor = [System.Drawing.Color]::FromArgb(232, 113, 37)
    $welcomeStatus.Location = New-Object System.Drawing.Point 30, 220
    $welcomeStatus.Size = New-Object System.Drawing.Size 470, 60
    $welcomePanel.Controls.Add($welcomeStatus)

    $licensePanel = New-Object System.Windows.Forms.Panel
    $licensePanel.Dock = "Fill"
    $licensePanel.Visible = $false
    $content.Controls.Add($licensePanel)

    $licenseTitle = New-Object System.Windows.Forms.Label
    $licenseTitle.Text = "License Agreement"
    $licenseTitle.Font = New-Object System.Drawing.Font "Segoe UI", 18, ([System.Drawing.FontStyle]::Bold)
    $licenseTitle.Location = New-Object System.Drawing.Point 28, 26
    $licenseTitle.AutoSize = $true
    $licensePanel.Controls.Add($licenseTitle)

    $licenseBox = New-Object System.Windows.Forms.TextBox
    $licenseBox.Multiline = $true
    $licenseBox.ScrollBars = "Vertical"
    $licenseBox.ReadOnly = $true
    $licenseBox.Text = $LicenseText
    $licenseBox.Location = New-Object System.Drawing.Point 30, 72
    $licenseBox.Size = New-Object System.Drawing.Size 470, 250
    $licensePanel.Controls.Add($licenseBox)

    $acceptCheck = New-Object System.Windows.Forms.CheckBox
    $acceptCheck.Text = "I accept the license terms"
    $acceptCheck.Location = New-Object System.Drawing.Point 30, 340
    $acceptCheck.AutoSize = $true
    $licensePanel.Controls.Add($acceptCheck)

    $optionsPanel = New-Object System.Windows.Forms.Panel
    $optionsPanel.Dock = "Fill"
    $optionsPanel.Visible = $false
    $content.Controls.Add($optionsPanel)

    $optionsTitle = New-Object System.Windows.Forms.Label
    $optionsTitle.Text = "Installation Options"
    $optionsTitle.Font = New-Object System.Drawing.Font "Segoe UI", 18, ([System.Drawing.FontStyle]::Bold)
    $optionsTitle.Location = New-Object System.Drawing.Point 28, 26
    $optionsTitle.AutoSize = $true
    $optionsPanel.Controls.Add($optionsTitle)

    $folderLabel = New-Object System.Windows.Forms.Label
    $folderLabel.Text = "Install folder"
    $folderLabel.Location = New-Object System.Drawing.Point 30, 86
    $folderLabel.AutoSize = $true
    $optionsPanel.Controls.Add($folderLabel)

    $folderText = New-Object System.Windows.Forms.TextBox
    $folderText.Location = New-Object System.Drawing.Point 30, 112
    $folderText.Size = New-Object System.Drawing.Size 370, 24
    $folderText.Text = $DefaultInstallDir
    $optionsPanel.Controls.Add($folderText)

    $browseButton = New-Object System.Windows.Forms.Button
    $browseButton.Text = "Browse..."
    $browseButton.Location = New-Object System.Drawing.Point 410, 110
    $browseButton.Size = New-Object System.Drawing.Size 90, 28
    $optionsPanel.Controls.Add($browseButton)

    $pathCheck = New-Object System.Windows.Forms.CheckBox
    $pathCheck.Text = "Add PyLua bin folder to PATH"
    $pathCheck.Location = New-Object System.Drawing.Point 30, 164
    $pathCheck.Checked = $true
    $pathCheck.AutoSize = $true
    $optionsPanel.Controls.Add($pathCheck)

    $modeLabel = New-Object System.Windows.Forms.Label
    $modeLabel.Location = New-Object System.Drawing.Point 30, 214
    $modeLabel.Size = New-Object System.Drawing.Size 470, 72
    $modeLabel.ForeColor = [System.Drawing.Color]::FromArgb(232, 113, 37)
    $modeLabel.Font = New-Object System.Drawing.Font "Segoe UI", 10, ([System.Drawing.FontStyle]::Bold)
    $optionsPanel.Controls.Add($modeLabel)

    $summaryLabel = New-Object System.Windows.Forms.Label
    $summaryLabel.Location = New-Object System.Drawing.Point 30, 308
    $summaryLabel.Size = New-Object System.Drawing.Size 470, 80
    $summaryLabel.Font = New-Object System.Drawing.Font "Segoe UI", 10
    $optionsPanel.Controls.Add($summaryLabel)

    $folderDialog = New-Object System.Windows.Forms.FolderBrowserDialog
    $folderDialog.Description = "Choose where PyLua should be installed"

    function Update-InstallStatus {
        $target = $folderText.Text.Trim()
        if ([string]::IsNullOrWhiteSpace($target)) {
            $modeLabel.Text = "Choose an installation folder."
            $summaryLabel.Text = ""
            $welcomeStatus.Text = ""
            return
        }

        $status = Get-InstallMode -TargetDir $target -NewVersion $Version
        $modeLabel.Text = $status.Label + " - " + $status.Details
        $summaryLabel.Text = "Version: $Version`r`nFolder: $target`r`nAdd to PATH: " + ($(if ($pathCheck.Checked) { "Yes" } else { "No" }))
        $welcomeStatus.Text = $status.Details
    }

    $browseButton.Add_Click({
        $folderDialog.SelectedPath = $folderText.Text
        if ($folderDialog.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) {
            $folderText.Text = $folderDialog.SelectedPath
            Update-InstallStatus
        }
    })

    $folderText.Add_TextChanged({ Update-InstallStatus })
    $pathCheck.Add_CheckedChanged({ Update-InstallStatus })

    $panels = @($welcomePanel, $licensePanel, $optionsPanel)
    $titles = @("Step 1 of 3`r`nWelcome", "Step 2 of 3`r`nLicense", "Step 3 of 3`r`nInstall options")
    $script:CurrentStep = 0

    function Show-Step {
        param([int]$Step)
        for ($i = 0; $i -lt $panels.Count; $i++) {
            $panels[$i].Visible = ($i -eq $Step)
        }
        $stepLabel.Text = $titles[$Step]
        $backButton.Enabled = $Step -gt 0
        $nextButton.Text = if ($Step -eq ($panels.Count - 1)) { "Install" } else { "Next >" }
        if ($Step -eq 2) {
            Update-InstallStatus
        }
    }

    $cancelButton.Add_Click({
        $form.DialogResult = [System.Windows.Forms.DialogResult]::Cancel
        $form.Close()
    })

    $backButton.Add_Click({
        if ($script:CurrentStep -gt 0) {
            $script:CurrentStep--
            Show-Step $script:CurrentStep
        }
    })

    $nextButton.Add_Click({
        if ($script:CurrentStep -eq 1 -and -not $acceptCheck.Checked) {
            [System.Windows.Forms.MessageBox]::Show("You need to accept the license terms to continue.", "PyLua Setup",
                [System.Windows.Forms.MessageBoxButtons]::OK, [System.Windows.Forms.MessageBoxIcon]::Warning) | Out-Null
            return
        }

        if ($script:CurrentStep -eq 2) {
            $target = $folderText.Text.Trim()
            if ([string]::IsNullOrWhiteSpace($target)) {
                [System.Windows.Forms.MessageBox]::Show("Choose an installation folder.", "PyLua Setup",
                    [System.Windows.Forms.MessageBoxButtons]::OK, [System.Windows.Forms.MessageBoxIcon]::Warning) | Out-Null
                return
            }

            $form.Tag = @{
                InstallDir = [System.IO.Path]::GetFullPath($target)
                AddToPath = [bool]$pathCheck.Checked
            }
            $form.DialogResult = [System.Windows.Forms.DialogResult]::OK
            $form.Close()
            return
        }

        $script:CurrentStep++
        Show-Step $script:CurrentStep
    })

    Show-Step 0
    Update-InstallStatus

    try {
        $result = $form.ShowDialog()
        if ($result -eq [System.Windows.Forms.DialogResult]::OK) {
            return $form.Tag
        }
        return $null
    } finally {
        if (Test-Path $tempIconPath) {
            Remove-Item $tempIconPath -Force -ErrorAction SilentlyContinue
        }
        $form.Dispose()
    }
}

function Invoke-Install {
    param(
        [string]$TargetDir,
        [string]$Version,
        [bool]$AddToPath
    )

    $installDir = [System.IO.Path]::GetFullPath($TargetDir)
    $binDir = Join-Path $installDir "bin"
    $startMenuDir = Join-Path $env:APPDATA "Microsoft\Windows\Start Menu\Programs\PyLua"
    $payloadZip = Get-SourceFile "payload.zip"

    if (-not (Test-Path $payloadZip)) {
        throw "payload.zip not found next to installer files."
    }

    $installState = Get-InstallMode -TargetDir $installDir -NewVersion $Version

    Write-Step "Installing PyLua to $installDir"
    New-Item -ItemType Directory -Force -Path $installDir | Out-Null

    foreach ($oldItem in @("bin", "docs", "examples", "include", "lib", "python", "website")) {
        $oldPath = Join-Path $installDir $oldItem
        if (Test-Path $oldPath) {
            Remove-Item -Path $oldPath -Recurse -Force
        }
    }

    $expandRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("pylua-payload-" + [guid]::NewGuid().ToString("N"))
    New-Item -ItemType Directory -Force -Path $expandRoot | Out-Null
    try {
        Expand-Archive -Path $payloadZip -DestinationPath $expandRoot -Force
        $payloadSource = Join-Path $expandRoot "payload"
        if (-not (Test-Path $payloadSource)) {
            $payloadSource = $expandRoot
        }

        Get-ChildItem -Path $payloadSource -Force | ForEach-Object {
            Copy-Item -Path $_.FullName -Destination $installDir -Recurse -Force
        }
    } finally {
        if (Test-Path $expandRoot) {
            Remove-Item -Path $expandRoot -Recurse -Force
        }
    }

    Copy-Item -Path (Get-SourceFile "uninstall.ps1") -Destination (Join-Path $installDir "uninstall.ps1") -Force
    Copy-Item -Path (Get-SourceFile "uninstall.cmd") -Destination (Join-Path $installDir "uninstall.cmd") -Force

    $iconPath = Join-Path $installDir "PyLua.ico"
    Write-PyLuaIcon $iconPath

    [Environment]::SetEnvironmentVariable("PYLUA_HOME", $installDir, "User")
    Ensure-PathSetting -BinDir $binDir -Enabled $AddToPath
    if ($AddToPath) {
        Write-Step "Added $binDir to user PATH"
    }

    New-Item -ItemType Directory -Force -Path $startMenuDir | Out-Null
    $shell = New-Object -ComObject WScript.Shell

    $guideShortcut = $shell.CreateShortcut((Join-Path $startMenuDir "PyLua Guide.lnk"))
    $guideShortcut.TargetPath = Join-Path $installDir "docs\PYLUA_GUIDE.md"
    $guideShortcut.IconLocation = $iconPath
    $guideShortcut.Save()

    $cmdShortcut = $shell.CreateShortcut((Join-Path $startMenuDir "PyLua CLI.lnk"))
    $cmdShortcut.TargetPath = $env:ComSpec
    $cmdShortcut.Arguments = "/k set ""PYLUA_HOME=$installDir"" && set ""PATH=$binDir;%PATH%"""
    $cmdShortcut.WorkingDirectory = $installDir
    $cmdShortcut.IconLocation = $iconPath
    $cmdShortcut.Save()

    $uninstallShortcut = $shell.CreateShortcut((Join-Path $startMenuDir ("Uninstall PyLua " + $Version + ".lnk")))
    $uninstallShortcut.TargetPath = Join-Path $installDir "uninstall.cmd"
    $uninstallShortcut.WorkingDirectory = $installDir
    $uninstallShortcut.IconLocation = $iconPath
    $uninstallShortcut.Save()

    $manifest = @{
        app = "PyLua"
        version = $Version
        install_dir = $installDir
        add_to_path = $AddToPath
        install_mode = $installState.Mode
        existing_version = $installState.ExistingVersion
        installed_at = (Get-Date).ToString("s")
    } | ConvertTo-Json
    Set-Content -Path (Get-ManifestPath $installDir) -Value $manifest -Encoding UTF8

    & (Join-Path $binDir "pylua.exe") version | Out-Null

    Write-Step "PyLua installation complete."
    Write-Step "You may need to open a new terminal for PATH changes to apply."

    return $installState
}

$script:SourceRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$script:RequestedInstallDir = $InstallDir
$version = Get-InstallerVersion
$licenseText = Get-LicenseText
$defaultInstallDir = Get-DefaultInstallDir

$selection = $null
if ($Quiet) {
    $selection = @{
        InstallDir = $defaultInstallDir
        AddToPath = $true
    }
} else {
    $selection = Show-InstallerWizard -Version $version -DefaultInstallDir $defaultInstallDir -LicenseText $licenseText
}

if (-not $selection) {
    exit 1
}

$result = Invoke-Install -TargetDir $selection.InstallDir -Version $version -AddToPath ([bool]$selection.AddToPath)

if (-not $Quiet) {
    Add-Type -AssemblyName System.Windows.Forms
    [System.Windows.Forms.MessageBox]::Show(
        "PyLua $version installed successfully.`r`nMode: $($result.Label)`r`nFolder: $($selection.InstallDir)",
        "PyLua Setup",
        [System.Windows.Forms.MessageBoxButtons]::OK,
        [System.Windows.Forms.MessageBoxIcon]::Information
    ) | Out-Null
}
