param(
    [string]$ToolsRoot = "",

    [ValidateSet("run", "prepare")]
    [string]$Mode = "run",

    [ValidateSet("application")]
    [string]$Target = "application"
)

$ErrorActionPreference = "Stop"
$frameworkRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..\")).Path
if ([string]::IsNullOrWhiteSpace($ToolsRoot)) {
    $ToolsRoot = $frameworkRoot
}

$lock = $null
$exitCode = 10
try {
    Import-Module -Name (Join-Path $frameworkRoot "Core\Toolkit.Core.psm1") -Force
    . (Join-Path $frameworkRoot "Adapters\Probe\JLink\jlink_flash.ps1")
    $configuration = Import-ToolkitConfiguration -ToolsRoot $ToolsRoot
    $keilTarget = Assert-ToolkitRequiredValue -Configuration $configuration -Name "PROJECT_KEIL_TARGET"
    $logDirectory = Resolve-ToolkitProjectPath -ProjectRoot $configuration.PROJECT_ROOT -RelativePath (Assert-ToolkitRequiredValue -Configuration $configuration -Name "PROJECT_LOG_DIR")
    New-ToolkitLogDirectory -Path $logDirectory | Out-Null
    $lock = Enter-ToolkitLock -Path (Get-ToolkitJLinkLockPath -Configuration $configuration)
    $result = Invoke-JLinkFlash -Configuration $configuration -Mode $Mode -LogPath (Join-Path $logDirectory ("{0}_flash.log" -f $keilTarget))
    if ($result.ExitCode -eq 0) {
        Write-Host "[FLASH][PASS] J-Link programming command completed successfully."
        $exitCode = 0
    }
    else {
        Write-Host "[FLASH][FAIL] J-Link returned ERRORLEVEL=$($result.ExitCode)."
        $exitCode = 30
    }
}
catch {
    Write-Host "[FLASH][ERROR] $($_.Exception.Message)"
    $exitCode = if ($_.Exception.Message -like "Toolkit lock is already held:*") { 30 } else { 10 }
}
finally {
    if ($null -ne $lock) {
        Exit-ToolkitLock -Lock $lock
    }
}

exit $exitCode
