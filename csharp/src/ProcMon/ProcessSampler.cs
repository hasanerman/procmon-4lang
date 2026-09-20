using System.ComponentModel;
using System.Diagnostics;

namespace ProcMon;

public sealed record ProcessInfo
{
    public required int Pid { get; init; }

    public required string Name { get; init; }

    public long? MemoryBytes { get; init; }

    public int? Threads { get; init; }

    public TimeSpan? CpuTime { get; init; }

    public double CpuPercent { get; init; }
}

public sealed record Sample(IReadOnlyList<ProcessInfo> Processes, long TimestampTicks);

public static class ProcessSampler
{
    public static Sample Take()
    {
        var processes = new List<ProcessInfo>(256);
        var timestamp = Stopwatch.GetTimestamp();

        foreach (var process in Process.GetProcesses())
        {
            using (process)
            {
                processes.Add(Describe(process));
            }
        }

        return new Sample(processes, timestamp);
    }

    public static IReadOnlyList<ProcessInfo> Filter(IReadOnlyList<ProcessInfo> processes, string needle)
    {
        ArgumentNullException.ThrowIfNull(processes);

        if (string.IsNullOrEmpty(needle))
        {
            return processes;
        }

        return processes
            .Where(info => info.Name.Contains(needle, StringComparison.OrdinalIgnoreCase))
            .ToList();
    }

    public static IReadOnlyList<ProcessInfo> Sort(IReadOnlyList<ProcessInfo> processes, SortKey key)
    {
        ArgumentNullException.ThrowIfNull(processes);

        return key switch
        {
            SortKey.Cpu => processes.OrderByDescending(info => info.CpuPercent).ThenBy(info => info.Pid).ToList(),
            SortKey.Mem => processes.OrderByDescending(info => info.MemoryBytes ?? 0).ThenBy(info => info.Pid).ToList(),
            SortKey.Pid => processes.OrderBy(info => info.Pid).ToList(),
            SortKey.Name => processes
                .OrderBy(info => info.Name, StringComparer.OrdinalIgnoreCase)
                .ThenBy(info => info.Pid)
                .ToList(),
            _ => processes,
        };
    }

    public static Sample ApplyCpuPercent(Sample current, Sample? previous, int cpuCount)
    {
        ArgumentNullException.ThrowIfNull(current);

        if (previous is null || previous.Processes.Count == 0)
        {
            return current;
        }

        var elapsed = Stopwatch.GetElapsedTime(previous.TimestampTicks, current.TimestampTicks);
        if (elapsed <= TimeSpan.Zero)
        {
            return current;
        }

        var cores = cpuCount <= 0 ? 1 : cpuCount;
        var before = previous.Processes
            .Where(info => info.CpuTime.HasValue)
            .GroupBy(info => info.Pid)
            .ToDictionary(group => group.Key, group => group.First().CpuTime!.Value);

        var updated = current.Processes.Select(info =>
        {
            if (!info.CpuTime.HasValue || !before.TryGetValue(info.Pid, out var earlier))
            {
                return info;
            }

            var busy = info.CpuTime.Value - earlier;
            if (busy < TimeSpan.Zero)
            {
                return info;
            }

            var percent = busy.TotalMilliseconds * 100.0 / (elapsed.TotalMilliseconds * cores);
            return info with { CpuPercent = Math.Min(100.0, percent) };
        }).ToList();

        return current with { Processes = updated };
    }

    private static ProcessInfo Describe(Process process)
    {
        long? memory = null;
        int? threads = null;
        TimeSpan? cpuTime = null;
        var name = "-";

        try
        {
            name = process.ProcessName;
        }
        catch (InvalidOperationException)
        {
            name = "-";
        }

        try
        {
            memory = process.WorkingSet64;
            threads = process.Threads.Count;
        }
        catch (Exception exception) when (exception is Win32Exception or InvalidOperationException or NotSupportedException)
        {
            memory = null;
        }

        try
        {
            cpuTime = process.TotalProcessorTime;
        }
        catch (Exception exception) when (exception is Win32Exception or InvalidOperationException or NotSupportedException)
        {
            cpuTime = null;
        }

        return new ProcessInfo
        {
            Pid = SafePid(process),
            Name = name,
            MemoryBytes = memory,
            Threads = threads,
            CpuTime = cpuTime,
        };
    }

    private static int SafePid(Process process)
    {
        try
        {
            return process.Id;
        }
        catch (InvalidOperationException)
        {
            return 0;
        }
    }
}
