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
    . (Join-Path $frameworkRoot "Adapters\Build\Keil\keil_build.ps1")
    . (Join-Path $frameworkRoot "Adapters\Probe\JLink\jlink_flash.ps1")
    . (Join-Path $frameworkRoot "Adapters\Probe\JLink\jlink_rtt.ps1")
    $configuration = Import-ToolkitConfiguration -ToolsRoot $ToolsRoot
    $keilTarget = Assert-ToolkitRequiredValue -Configuration $configuration -Name "PROJECT_KEIL_TARGET"
    $logDirectory = Resolve-ToolkitProjectPath -ProjectRoot $configuration.PROJECT_ROOT -RelativePath (Assert-ToolkitRequiredValue -Configuration $configuration -Name "PROJECT_LOG_DIR")
    New-ToolkitLogDirectory -Path $logDirectory | Out-Null

    Write-Host "[CYCLE] Step 1/3: Build"
    $buildResult = Invoke-KeilBuild -Configuration $configuration -LogPath (Join-Path $logDirectory ("{0}_build.log" -f $keilTarget))
    if ($buildResult.ExitCode -ge 2) {
        Write-Host "[CYCLE][FAIL] Build failed."
        $exitCode = 20
    }
    else {
        $lock = Enter-ToolkitLock -Path (Get-ToolkitJLinkLockPath -Configuration $configuration)

        Write-Host "[CYCLE] Step 2/3: Flash"
        $flashResult = Invoke-JLinkFlash -Configuration $configuration -Mode "run" -LogPath (Join-Path $logDirectory ("{0}_flash.log" -f $keilTarget))
        if ($flashResult.ExitCode -ne 0) {
            Write-Host "[CYCLE][FAIL] Flash failed."
            $exitCode = 30
        }
        else {
            Write-Host "[CYCLE] Step 3/3: RTT capture"
            if ($Seconds -lt 1) {
                $Seconds = if ($configuration.PSObject.Properties["RTT_CAPTURE_SECONDS"]) { [int]$configuration.RTT_CAPTURE_SECONDS } else { 10 }
            }
            $rttResult = Invoke-JLinkRtt -Configuration $configuration -Seconds $Seconds -OutputPath (Join-Path $logDirectory ("{0}_rtt.log" -f $keilTarget)) -DiagnosticPath (Join-Path $logDirectory ("{0}_rtt_logger.log" -f $keilTarget))
            if ($rttResult.ExitCode -ne 0) {
                Write-Host "[CYCLE][FAIL] RTT capture failed."
                $exitCode = 30
            }
            elseif ($buildResult.ExitCode -eq 1) {
                Write-Host "[CYCLE][WARN] Build, flash, and RTT capture completed with Keil warnings."
                $exitCode = 1
            }
            else {
                Write-Host "[CYCLE][PASS] Build, flash, and RTT capture commands completed."
                $exitCode = 0
            }
        }
    }
}
catch {
    Write-Host "[CYCLE][ERROR] $($_.Exception.Message)"
    $exitCode = if ($_.Exception.Message -like "Toolkit lock is already held:*") { 30 } else { 10 }
}
finally {
    if ($null -ne $lock) {
        Exit-ToolkitLock -Lock $lock
    }
}

exit $exitCode
