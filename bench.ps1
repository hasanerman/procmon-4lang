param(
    [int]$Runs = 7
)

$ErrorActionPreference = "Stop"
$root = $PSScriptRoot

$targets = @(
    @{ Name = "C";    Path = Join-Path $root "c\build\procmon.exe" }
    @{ Name = "C++";  Path = Join-Path $root "cpp\build\Release\procmon.exe" }
    @{ Name = "Rust"; Path = Join-Path $root "rust\target\release\procmon.exe" }
    @{ Name = "C#";   Path = Join-Path $root "csharp\src\ProcMon\bin\Release\net10.0\procmon.exe" }
)

$results = foreach ($target in $targets) {
    if (-not (Test-Path $target.Path)) {
        Write-Warning "$($target.Name): ikili bulunamadi -> $($target.Path)"
        continue
    }

    $durations = @()
    $peak = 0

    for ($i = 0; $i -lt $Runs; $i++) {
        $output = [System.IO.Path]::GetTempFileName()
        $start = [System.Diagnostics.Stopwatch]::StartNew()
        $process = Start-Process -FilePath $target.Path -ArgumentList "--once", "--top", "1" -PassThru -NoNewWindow -RedirectStandardOutput $output
        while (-not $process.HasExited) {
            $process.Refresh()
            try { $peak = [Math]::Max($peak, $process.PeakWorkingSet64) } catch { }
            Start-Sleep -Milliseconds 1
        }
        $start.Stop()
        try { $peak = [Math]::Max($peak, $process.PeakWorkingSet64) } catch { }
        Remove-Item $output -ErrorAction SilentlyContinue

        if ($i -gt 0) { $durations += $start.Elapsed.TotalMilliseconds }
    }

    [pscustomobject]@{
        Dil             = $target.Name
        "Sure (ms)"     = [Math]::Round(($durations | Measure-Object -Average).Average, 1)
        "Min (ms)"      = [Math]::Round(($durations | Measure-Object -Minimum).Minimum, 1)
        "Ikili (KB)"    = [Math]::Round((Get-Item $target.Path).Length / 1KB, 1)
        "Peak RAM (MB)" = [Math]::Round($peak / 1MB, 1)
    }
}

$results | Format-Table -AutoSize
