function New-ToolkitLogDirectory {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    if ([string]::IsNullOrWhiteSpace($Path)) {
        throw "Log directory path must not be empty."
    }

    $resolvedPath = [System.IO.Path]::GetFullPath($Path)
    if (Test-Path -LiteralPath $resolvedPath -PathType Leaf) {
        throw "Log directory path points to a file: $resolvedPath"
    }
    if (-not (Test-Path -LiteralPath $resolvedPath -PathType Container)) {
        New-Item -ItemType Directory -Path $resolvedPath -Force -ErrorAction Stop | Out-Null
    }

    return $resolvedPath
}

function Write-ToolkitLog {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,

        [Parameter(Mandatory = $true)]
        [string]$Message
    )

    $directory = Split-Path -Parent ([System.IO.Path]::GetFullPath($Path))
    New-ToolkitLogDirectory -Path $directory | Out-Null
    Add-Content -LiteralPath $Path -Value $Message -Encoding UTF8
}

function Write-ToolkitProcessResultLog {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path,

        [Parameter(Mandatory = $true)]
        [object]$Result
    )

    New-ToolkitLogDirectory -Path (Split-Path -Parent ([System.IO.Path]::GetFullPath($Path))) | Out-Null
    $lines = @(
        "ExitCode=$($Result.ExitCode)",
        "TimedOut=$($Result.TimedOut)",
        "ProcessId=$($Result.ProcessId)"
    )
    if (-not [string]::IsNullOrEmpty([string]$Result.Stdout)) {
        $lines += "[stdout]"
        $lines += [string]$Result.Stdout
    }
    if (-not [string]::IsNullOrEmpty([string]$Result.Stderr)) {
        $lines += "[stderr]"
        $lines += [string]$Result.Stderr
    }
    [System.IO.File]::WriteAllText($Path, ($lines -join [Environment]::NewLine), (New-Object System.Text.UTF8Encoding($false)))
}
