function Invoke-ToolkitConfigurationBatch {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Files
    )

    foreach ($file in $Files) {
        if (-not (Test-Path -LiteralPath $file -PathType Leaf)) {
            throw "Toolkit configuration file not found: $file"
        }
    }

    $calls = $Files | ForEach-Object {
        'call ""' + ($_ -replace '"', '""') + '""'
    }
    $command = ($calls -join " & ") + " & set"

    $startInfo = New-Object System.Diagnostics.ProcessStartInfo
    $startInfo.FileName = if ($env:ComSpec) { $env:ComSpec } else { "cmd.exe" }
    $startInfo.Arguments = '/d /s /c "' + ($command -replace '"', '""') + '"'
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true

    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $startInfo
    [void]$process.Start()
    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()
    $process.WaitForExit()
    $stdout = $stdoutTask.GetAwaiter().GetResult()
    $stderr = $stderrTask.GetAwaiter().GetResult()

    if ($process.ExitCode -ne 0) {
        throw "Toolkit configuration loading failed with exit code $($process.ExitCode): $stderr"
    }

    $values = [ordered]@{}
    foreach ($line in ($stdout -split "`r?`n")) {
        if ($line -match '^(?<Name>[^=]+)=(?<Value>.*)$') {
            $values[$matches.Name] = $matches.Value
        }
    }

    return [PSCustomObject]$values
}

function Import-ToolkitConfiguration {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ToolsRoot
    )

    $resolvedToolsRoot = (Resolve-Path -LiteralPath $ToolsRoot -ErrorAction Stop).Path
    $configRoot = Join-Path $resolvedToolsRoot "Config"
    $projectDefaults = Join-Path $configRoot "project.defaults.bat"
    $projectLocal = Join-Path $configRoot "project.local.bat"
    $toolchainLocal = Join-Path $configRoot "toolchain.local.bat"

    if (-not (Test-Path -LiteralPath $projectDefaults -PathType Leaf)) {
        throw "Project defaults configuration not found: $projectDefaults"
    }
    if (-not (Test-Path -LiteralPath $toolchainLocal -PathType Leaf)) {
        throw "Machine toolchain configuration not found: $toolchainLocal"
    }

    $files = @($projectDefaults)
    if (Test-Path -LiteralPath $projectLocal -PathType Leaf) {
        $files += $projectLocal
    }
    $files += $toolchainLocal

    $configuration = Invoke-ToolkitConfigurationBatch -Files $files
    $projectRoot = (Resolve-Path -LiteralPath (Join-Path $resolvedToolsRoot "..")).Path
    $configuration | Add-Member -NotePropertyName TOOLS_ROOT -NotePropertyValue $resolvedToolsRoot
    $configuration | Add-Member -NotePropertyName CONFIG_ROOT -NotePropertyValue $configRoot
    $configuration | Add-Member -NotePropertyName PROJECT_ROOT -NotePropertyValue $projectRoot
    return $configuration
}

function Assert-ToolkitRequiredValue {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Configuration,

        [Parameter(Mandatory = $true)]
        [string]$Name
    )

    $property = $Configuration.PSObject.Properties[$Name]
    if ($null -eq $property -or [string]::IsNullOrWhiteSpace([string]$property.Value)) {
        throw "Required toolkit configuration value is missing: $Name"
    }

    return [string]$property.Value
}
