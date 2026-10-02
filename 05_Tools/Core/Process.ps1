function ConvertTo-ToolkitProcessArgumentString {
    param(
        [string[]]$Arguments
    )

    $quotedArguments = foreach ($argument in $Arguments) {
        $text = if ($null -eq $argument) { "" } else { [string]$argument }
        if ($text -notmatch '[\s"]') {
            $text
            continue
        }
        $builder = New-Object System.Text.StringBuilder
        [void]$builder.Append('"')
        $backslashes = 0

        foreach ($character in $text.ToCharArray()) {
            if ($character -eq '\') {
                $backslashes++
                continue
            }

            if ($character -eq '"') {
                [void]$builder.Append(('\' * (($backslashes * 2) + 1)))
                [void]$builder.Append('"')
            }
            else {
                if ($backslashes -gt 0) {
                    [void]$builder.Append(('\' * $backslashes))
                }
                [void]$builder.Append($character)
            }
            $backslashes = 0
        }

        if ($backslashes -gt 0) {
            [void]$builder.Append(('\' * ($backslashes * 2)))
        }
        [void]$builder.Append('"')
        $builder.ToString()
    }

    return ($quotedArguments -join ' ')
}

function Stop-ToolkitOwnedProcess {
    param(
        [System.Diagnostics.Process]$Process,

        [int]$ProcessId = 0
    )

    $ownedProcess = $Process
    if ($null -eq $ownedProcess -and $ProcessId -gt 0) {
        try {
            $ownedProcess = [System.Diagnostics.Process]::GetProcessById($ProcessId)
        }
        catch {
            return
        }
    }
    if ($null -eq $ownedProcess) {
        return
    }

    try {
        $ownedProcess.Refresh()
        if (-not $ownedProcess.HasExited) {
            Stop-Process -Id $ownedProcess.Id -Force -ErrorAction SilentlyContinue
            $ownedProcess.WaitForExit()
        }
    }
    catch {
    }
}

function Start-ToolkitProcess {
    param(
        [Parameter(Mandatory = $true)]
        [string]$FilePath,

        [string[]]$Arguments = @(),

        [string]$WorkingDirectory = ""
    )

    $processFilePath = $FilePath
    $processArguments = $Arguments
    $extension = [System.IO.Path]::GetExtension($FilePath)
    if ($extension -eq ".bat" -or $extension -eq ".cmd") {
        $processFilePath = if ($env:ComSpec) { $env:ComSpec } else { "cmd.exe" }
        $processArguments = @("/d", "/c", "call", $FilePath) + $Arguments
    }

    $startInfo = New-Object System.Diagnostics.ProcessStartInfo
    $startInfo.FileName = $processFilePath
    $startInfo.Arguments = ConvertTo-ToolkitProcessArgumentString -Arguments $processArguments
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $true
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    if (-not [string]::IsNullOrWhiteSpace($WorkingDirectory)) {
        $startInfo.WorkingDirectory = $WorkingDirectory
    }

    $process = New-Object System.Diagnostics.Process
    $process.StartInfo = $startInfo
    [void]$process.Start()
    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()

    return [PSCustomObject]@{
        Process = $process
        StdoutTask = $stdoutTask
        StderrTask = $stderrTask
        ProcessId = $process.Id
    }
}

function Get-ToolkitProcessResult {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Capture,

        [bool]$TimedOut = $false
    )

    $process = $Capture.Process
    try {
        $process.WaitForExit()
    }
    catch {
    }

    $stdout = $Capture.StdoutTask.GetAwaiter().GetResult()
    $stderr = $Capture.StderrTask.GetAwaiter().GetResult()
    return [PSCustomObject]@{
        ExitCode = $process.ExitCode
        Stdout = $stdout
        Stderr = $stderr
        TimedOut = $TimedOut
        ProcessId = $Capture.ProcessId
    }
}

function Complete-ToolkitProcess {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Capture,

        [int]$TimeoutMilliseconds = 60000
    )

    if ($TimeoutMilliseconds -lt 1) {
        throw "Process timeout must be positive: $TimeoutMilliseconds"
    }

    $timedOut = -not $Capture.Process.WaitForExit($TimeoutMilliseconds)
    if ($timedOut) {
        Stop-ToolkitOwnedProcess -Process $Capture.Process
    }

    return Get-ToolkitProcessResult -Capture $Capture -TimedOut $timedOut
}

function Stop-ToolkitProcess {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Capture
    )

    Stop-ToolkitOwnedProcess -Process $Capture.Process
    return Get-ToolkitProcessResult -Capture $Capture -TimedOut $true
}

function Invoke-ToolkitProcess {
    param(
        [Parameter(Mandatory = $true)]
        [string]$FilePath,

        [string[]]$Arguments = @(),

        [int]$TimeoutMilliseconds = 60000,

        [string]$WorkingDirectory = ""
    )

    if ($TimeoutMilliseconds -lt 1) {
        throw "Process timeout must be positive: $TimeoutMilliseconds"
    }

    $capture = Start-ToolkitProcess -FilePath $FilePath -Arguments $Arguments -WorkingDirectory $WorkingDirectory
    return Complete-ToolkitProcess -Capture $capture -TimeoutMilliseconds $TimeoutMilliseconds
}

function Test-ToolkitTcpPort {
    param(
        [Parameter(Mandatory = $true)]
        [string]$HostName,

        [Parameter(Mandatory = $true)]
        [int]$PortNumber
    )

    if (($PortNumber -lt 1) -or ($PortNumber -gt 65535)) {
        return $false
    }

    $client = New-Object System.Net.Sockets.TcpClient
    $asyncResult = $null
    try {
        $asyncResult = $client.BeginConnect($HostName, $PortNumber, $null, $null)
        if (-not $asyncResult.AsyncWaitHandle.WaitOne(250)) {
            return $false
        }
        $client.EndConnect($asyncResult)
        return $true
    }
    catch {
        return $false
    }
    finally {
        $client.Close()
    }
}
