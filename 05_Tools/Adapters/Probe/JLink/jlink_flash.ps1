function Invoke-JLinkFlash {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Configuration,

        [ValidateSet("run", "prepare")]
        [string]$Mode = "run",

        [Parameter(Mandatory = $true)]
        [string]$LogPath
    )

    $jlink = Assert-ToolkitRequiredValue -Configuration $Configuration -Name "JLINK_EXE"
    $hexRelativePath = Assert-ToolkitRequiredValue -Configuration $Configuration -Name "PROJECT_APP_HEX"
    $device = Assert-ToolkitRequiredValue -Configuration $Configuration -Name "JLINK_DEVICE"
    $interface = Assert-ToolkitRequiredValue -Configuration $Configuration -Name "JLINK_IF"
    $speed = Assert-ToolkitRequiredValue -Configuration $Configuration -Name "JLINK_SPEED"
    $hexPath = Resolve-ToolkitProjectPath -ProjectRoot $Configuration.PROJECT_ROOT -RelativePath $hexRelativePath
    if (-not (Test-Path -LiteralPath $hexPath -PathType Leaf)) {
        throw "Firmware HEX file not found: $hexPath"
    }

    $commandFile = Join-Path ([System.IO.Path]::GetTempPath()) ("toolkit_flash_" + [Guid]::NewGuid().ToString("N") + ".jlink")
    try {
        $commands = @(
            "EoE 1",
            ('loadfile "{0}"' -f $hexPath)
        )
        if ($Mode -eq "run") {
            $commands += "r"
            $commands += "h"
            $commands += "g"
        }
        $commands += "exit"
        [System.IO.File]::WriteAllLines($commandFile, $commands, [System.Text.Encoding]::ASCII)

        $result = Invoke-ToolkitProcess -FilePath $jlink -Arguments @(
            "-device", $device,
            "-if", $interface,
            "-speed", $speed,
            "-AutoConnect", "1",
            "-ExitOnError", "1",
            "-NoGui", "1",
            "-CommandFile", $commandFile
        ) -WorkingDirectory $Configuration.PROJECT_ROOT
        Write-ToolkitProcessResultLog -Path $LogPath -Result $result
        return $result
    }
    finally {
        Remove-Item -LiteralPath $commandFile -Force -ErrorAction SilentlyContinue
    }
}
