param(
    [Parameter(Position = 0, Mandatory = $true)]
    [ValidateSet("build", "flash", "rtt", "run")]
    [string]$Command,
    [Parameter(Position = 1, ValueFromRemainingArguments = $true)]
    [string[]]$Arguments = @()
)

$ErrorActionPreference = "Stop"
$workflow = Join-Path $PSScriptRoot ("Workflows\Application\{0}.ps1" -f $Command)

switch ($Command) {
    "build" {
        if ($Arguments.Count -ne 0) { throw "Usage: toolkit.bat build" }
        $workflowArguments = @()
    }
    "flash" {
        if ($Arguments.Count -gt 1 -or ($Arguments.Count -eq 1 -and $Arguments[0] -notin @("run", "prepare"))) {
            throw "Usage: toolkit.bat flash [run|prepare]"
        }
        $workflowArguments = if ($Arguments.Count -eq 1) { @("-Mode", $Arguments[0]) } else { @() }
    }
    default {
        $seconds = 0
        if ($Arguments.Count -gt 1 -or ($Arguments.Count -eq 1 -and (-not [int]::TryParse($Arguments[0], [ref]$seconds) -or $seconds -lt 1))) {
            throw "Usage: toolkit.bat $Command [seconds]"
        }
        $workflowArguments = if ($Arguments.Count -eq 1) { @("-Seconds", $Arguments[0]) } else { @() }
    }
}

& powershell.exe -NoProfile -ExecutionPolicy Bypass -File $workflow @workflowArguments
exit $LASTEXITCODE
