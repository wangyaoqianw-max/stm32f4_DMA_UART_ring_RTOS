param(
    [string]$ToolsRoot = "",

    [ValidateSet("application")]
    [string]$Target = "application"
)

$ErrorActionPreference = "Stop"
$frameworkRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..\")).Path
if ([string]::IsNullOrWhiteSpace($ToolsRoot)) {
    $ToolsRoot = $frameworkRoot
}

try {
    Import-Module -Name (Join-Path $frameworkRoot "Core\Toolkit.Core.psm1") -Force
    . (Join-Path $frameworkRoot "Adapters\Build\Keil\keil_build.ps1")
    $configuration = Import-ToolkitConfiguration -ToolsRoot $ToolsRoot
    $keilTarget = Assert-ToolkitRequiredValue -Configuration $configuration -Name "PROJECT_KEIL_TARGET"
    $logDirectory = Resolve-ToolkitProjectPath -ProjectRoot $configuration.PROJECT_ROOT -RelativePath (Assert-ToolkitRequiredValue -Configuration $configuration -Name "PROJECT_LOG_DIR")
    New-ToolkitLogDirectory -Path $logDirectory | Out-Null
    $result = Invoke-KeilBuild -Configuration $configuration -LogPath (Join-Path $logDirectory ("{0}_build.log" -f $keilTarget))
}
catch {
    Write-Host "[BUILD][ERROR] $($_.Exception.Message)"
    exit 10
}

if ($result.ExitCode -eq 0) {
    Write-Host "[BUILD][PASS] Keil build completed without errors or warnings."
    exit 0
}
if ($result.ExitCode -eq 1) {
    Write-Host "[BUILD][WARN] Keil build completed with warnings."
    exit 1
}

Write-Host "[BUILD][FAIL] Keil returned ERRORLEVEL=$($result.ExitCode)."
exit 20
