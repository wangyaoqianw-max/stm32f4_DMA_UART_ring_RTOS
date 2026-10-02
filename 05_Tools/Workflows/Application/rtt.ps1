param(
    [string]$ToolsRoot = "",

    [int]$Seconds = 0,

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
    . (Join-Path $frameworkRoot "Adapters\Probe\JLink\jlink_rtt.ps1")
    $configuration = Import-ToolkitConfiguration -ToolsRoot $ToolsRoot
    $keilTarget = Assert-ToolkitRequiredValue -Configuration $configuration -Name "PROJECT_KEIL_TARGET"
    if ($Seconds -lt 1) {
        $Seconds = if ($configuration.PSObject.Properties["RTT_CAPTURE_SECONDS"]) { [int]$configuration.RTT_CAPTURE_SECONDS } else { 10 }
    }
    $logDirectory = Resolve-ToolkitProjectPath -ProjectRoot $configuration.PROJECT_ROOT -RelativePath (Assert-ToolkitRequiredValue -Configuration $configuration -Name "PROJECT_LOG_DIR")
    New-ToolkitLogDirectory -Path $logDirectory | Out-Null
    $lock = Enter-ToolkitLock -Path (Get-ToolkitJLinkLockPath -Configuration $configuration)
    $result = Invoke-JLinkRtt -Configuration $configuration -Seconds $Seconds -OutputPath (Join-Path $logDirectory ("{0}_rtt.log" -f $keilTarget)) -DiagnosticPath (Join-Path $logDirectory ("{0}_rtt_logger.log" -f $keilTarget))
    if ($result.ExitCode -eq 0) {
        Write-Host "[RTT][PASS] RTT data captured."
        $exitCode = 0
    }
    else {
        Write-Host "[RTT][FAIL] RTT capture returned ERRORLEVEL=$($result.ExitCode)."
        $exitCode = 30
    }
}
catch {
    Write-Host "[RTT][ERROR] $($_.Exception.Message)"
    $exitCode = if ($_.Exception.Message -like "Toolkit lock is already held:*") { 30 } else { 10 }
}
finally {
    if ($null -ne $lock) {
        Exit-ToolkitLock -Lock $lock
    }
}

exit $exitCode
