function Invoke-KeilBuild {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Configuration,

        [Parameter(Mandatory = $true)]
        [string]$LogPath
    )

    $keil = Assert-ToolkitRequiredValue -Configuration $Configuration -Name "KEIL_UV4"
    $projectRelativePath = Assert-ToolkitRequiredValue -Configuration $Configuration -Name "PROJECT_KEIL_PROJECT_FILE"
    $target = Assert-ToolkitRequiredValue -Configuration $Configuration -Name "PROJECT_KEIL_TARGET"
    $projectPath = Resolve-ToolkitProjectPath -ProjectRoot $Configuration.PROJECT_ROOT -RelativePath $projectRelativePath
    if (-not (Test-Path -LiteralPath $projectPath -PathType Leaf)) {
        throw "Keil project file not found: $projectPath"
    }

    $result = Invoke-ToolkitProcess -FilePath $keil -Arguments @(
        "-b",
        $projectPath,
        "-t",
        $target,
        "-o",
        $LogPath
    ) -WorkingDirectory $Configuration.PROJECT_ROOT
    Write-ToolkitProcessResultLog -Path $LogPath -Result $result
    return $result
}
