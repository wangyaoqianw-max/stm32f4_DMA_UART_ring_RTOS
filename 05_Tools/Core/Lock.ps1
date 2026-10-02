function Get-ToolkitJLinkLockPath {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Configuration
    )

    $projectRoot = Assert-ToolkitRequiredValue -Configuration $Configuration -Name "PROJECT_ROOT"
    $logDirectory = Resolve-ToolkitProjectPath -ProjectRoot $projectRoot -RelativePath (Assert-ToolkitRequiredValue -Configuration $Configuration -Name "PROJECT_LOG_DIR")
    New-ToolkitLogDirectory -Path $logDirectory | Out-Null
    return Join-Path $logDirectory "toolkit_jlink.lock"
}

function Enter-ToolkitLock {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    $resolvedPath = [System.IO.Path]::GetFullPath($Path)
    $directory = Split-Path -Parent $resolvedPath
    if (-not (Test-Path -LiteralPath $directory -PathType Container)) {
        New-Item -ItemType Directory -Path $directory -Force -ErrorAction Stop | Out-Null
    }

    try {
        $stream = [System.IO.File]::Open(
            $resolvedPath,
            [System.IO.FileMode]::CreateNew,
            [System.IO.FileAccess]::Write,
            [System.IO.FileShare]::None
        )
    }
    catch [System.IO.IOException] {
        throw "Toolkit lock is already held: $resolvedPath"
    }

    try {
        $writer = New-Object System.IO.StreamWriter($stream, [System.Text.Encoding]::ASCII, 128, $true)
        $writer.WriteLine("pid=$PID")
        $writer.Flush()
        $writer.Dispose()
        return [PSCustomObject]@{
            Path = $resolvedPath
            Stream = $stream
        }
    }
    catch {
        $stream.Dispose()
        Remove-Item -LiteralPath $resolvedPath -Force -ErrorAction SilentlyContinue
        throw
    }
}

function Exit-ToolkitLock {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Lock
    )

    $path = if ($Lock -is [string]) { $Lock } else { $Lock.Path }
    if ($Lock -isnot [string] -and $null -ne $Lock.Stream) {
        $Lock.Stream.Dispose()
    }
    if (-not [string]::IsNullOrWhiteSpace($path)) {
        Remove-Item -LiteralPath $path -Force -ErrorAction SilentlyContinue
    }
}
