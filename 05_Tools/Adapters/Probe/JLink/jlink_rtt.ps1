function Invoke-JLinkRtt {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Configuration,

        [Parameter(Mandatory = $true)]
        [int]$Seconds,

        [Parameter(Mandatory = $true)]
        [string]$OutputPath,

        [Parameter(Mandatory = $true)]
        [string]$DiagnosticPath
    )

    if ($Seconds -lt 1) {
        throw "RTT capture duration must be positive: $Seconds"
    }
    $logger = Assert-ToolkitRequiredValue -Configuration $Configuration -Name "JLINK_RTT_LOGGER"
    $device = Assert-ToolkitRequiredValue -Configuration $Configuration -Name "JLINK_DEVICE"
    $interface = Assert-ToolkitRequiredValue -Configuration $Configuration -Name "JLINK_IF"
    $speed = Assert-ToolkitRequiredValue -Configuration $Configuration -Name "JLINK_SPEED"
    $channel = Assert-ToolkitRequiredValue -Configuration $Configuration -Name "JLINK_RTT_CHANNEL"

    Remove-Item -LiteralPath $OutputPath -Force -ErrorAction SilentlyContinue
    $result = Invoke-ToolkitProcess -FilePath $logger -Arguments @(
        "-Device", $device,
        "-If", $interface,
        "-Speed", $speed,
        "-RTTChannel", $channel,
        $OutputPath
    ) -TimeoutMilliseconds ($Seconds * 1000) -WorkingDirectory $Configuration.PROJECT_ROOT
    Write-ToolkitProcessResultLog -Path $DiagnosticPath -Result $result

    if ($result.TimedOut -and (Test-Path -LiteralPath $OutputPath -PathType Leaf) -and ((Get-Item -LiteralPath $OutputPath).Length -gt 0)) {
        $result.ExitCode = 0
    }
    elseif ($result.TimedOut -and (-not (Test-Path -LiteralPath $OutputPath -PathType Leaf))) {
        $result.ExitCode = 31
    }
    elseif ($result.TimedOut -and (Test-Path -LiteralPath $OutputPath -PathType Leaf) -and ((Get-Item -LiteralPath $OutputPath).Length -eq 0)) {
        $result.ExitCode = 32
    }
    return $result
}
