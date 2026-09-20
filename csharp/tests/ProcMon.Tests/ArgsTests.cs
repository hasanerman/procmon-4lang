using System.Diagnostics;

using Xunit;

namespace ProcMon.Tests;

public class ArgsTests
{
    [Fact]
    public void DefaultsAreApplied()
    {
        var options = Args.Parse(Array.Empty<string>());

        Assert.Equal(TimeSpan.FromMilliseconds(1000), options.Interval);
        Assert.Equal(25, options.Top);
        Assert.Equal(SortKey.Cpu, options.Sort);
        Assert.False(options.Once);
        Assert.Null(options.KillPid);
        Assert.Equal(string.Empty, options.Filter);
    }

    [Fact]
    public void FullCommandLineIsParsed()
    {
        var options = Args.Parse(new[]
        {
            "--interval", "250", "--filter", "chrome", "--sort", "mem", "--top", "5", "--once",
        });

        Assert.Equal(TimeSpan.FromMilliseconds(250), options.Interval);
        Assert.Equal("chrome", options.Filter);
        Assert.Equal(SortKey.Mem, options.Sort);
        Assert.Equal(5, options.Top);
        Assert.True(options.Once);
    }

    [Theory]
    [InlineData("cpu", SortKey.Cpu)]
    [InlineData("mem", SortKey.Mem)]
    [InlineData("pid", SortKey.Pid)]
    [InlineData("name", SortKey.Name)]
    public void SortKeysAreParsed(string value, SortKey expected)
    {
        Assert.Equal(expected, Args.Parse(new[] { "--sort", value }).Sort);
    }

    [Theory]
    [InlineData("--zoom")]
    [InlineData("--interval")]
    [InlineData("--sort", "disk")]
    [InlineData("--interval", "10")]
    [InlineData("--interval", "999999")]
    [InlineData("--top", "0")]
    [InlineData("--top", "abc")]
    [InlineData("--top", "12x")]
    [InlineData("--kill", "-3")]
    public void BadValuesAreRejected(params string[] arguments)
    {
        Assert.Throws<ArgumentParseException>(() => Args.Parse(arguments));
    }

    [Fact]
    public void KillAndHelpAreParsed()
    {
        Assert.Equal(4321, Args.Parse(new[] { "--kill", "4321" }).KillPid);
        Assert.True(Args.Parse(new[] { "--help" }).Help);
    }
}

public class ProcessSamplerTests
{
    private static ProcessInfo Info(int pid, string name, long memory, TimeSpan cpuTime) => new()
    {
        Pid = pid,
        Name = name,
        MemoryBytes = memory,
        Threads = 4,
        CpuTime = cpuTime,
    };

    private static IReadOnlyList<ProcessInfo> Sample() => new[]
    {
        Info(10, "Chrome.exe", 300L * 1024 * 1024, TimeSpan.FromSeconds(1)),
        Info(20, "code.exe", 900L * 1024 * 1024, TimeSpan.FromSeconds(2)),
        Info(5, "alpha", 10L * 1024 * 1024, TimeSpan.Zero),
    };

    [Fact]
    public void FilterIsCaseInsensitive()
    {
        var filtered = ProcessSampler.Filter(Sample(), "CHROME");

        Assert.Single(filtered);
        Assert.Equal(10, filtered[0].Pid);
        Assert.Equal(3, ProcessSampler.Filter(Sample(), string.Empty).Count);
        Assert.Empty(ProcessSampler.Filter(Sample(), "yok-boyle"));
    }

    [Fact]
    public void SortingOrdersRows()
    {
        var byMemory = ProcessSampler.Sort(Sample(), SortKey.Mem);
        Assert.Equal(20, byMemory[0].Pid);
        Assert.Equal(5, byMemory[2].Pid);

        var byPid = ProcessSampler.Sort(Sample(), SortKey.Pid);
        Assert.Equal(5, byPid[0].Pid);
        Assert.Equal(20, byPid[2].Pid);

        var byName = ProcessSampler.Sort(Sample(), SortKey.Name);
        Assert.Equal("alpha", byName[0].Name);
        Assert.Equal("Chrome.exe", byName[1].Name);
        Assert.Equal("code.exe", byName[2].Name);
    }

    [Fact]
    public void CpuPercentUsesTwoSamples()
    {
        var previous = new Sample(new[] { Info(10, "worker", 0, TimeSpan.Zero) }, 0);
        var oneSecond = Stopwatch.Frequency;
        var current = new Sample(new[] { Info(10, "worker", 0, TimeSpan.FromMilliseconds(500)) }, oneSecond);

        var single = ProcessSampler.ApplyCpuPercent(current, previous, 1);
        Assert.InRange(single.Processes[0].CpuPercent, 49.0, 51.0);

        var dual = ProcessSampler.ApplyCpuPercent(current, previous, 2);
        Assert.InRange(dual.Processes[0].CpuPercent, 24.0, 26.0);
    }

    [Fact]
    public void CpuPercentIsZeroWithoutPreviousSample()
    {
        var current = new Sample(new[] { Info(10, "worker", 0, TimeSpan.FromMilliseconds(500)) }, Stopwatch.GetTimestamp());

        var result = ProcessSampler.ApplyCpuPercent(current, null, 1);

        Assert.Equal(0.0, result.Processes[0].CpuPercent);
    }

    [Fact]
    public void LiveSnapshotContainsSelf()
    {
        var sample = ProcessSampler.Take();

        Assert.NotEmpty(sample.Processes);
        Assert.Contains(sample.Processes, info => info.Pid == Environment.ProcessId);
    }

    [Fact]
    public void ProtectedPidsAreRecognised()
    {
        Assert.True(Program.IsProtected(0));
        Assert.False(Program.IsProtected(Environment.ProcessId));
    }
}

public class RendererTests
{
    [Fact]
    public void TableRespectsTopAndFilter()
    {
        var processes = new[]
        {
            new ProcessInfo { Pid = 1, Name = "alpha", MemoryBytes = 1024 * 1024, Threads = 2, CpuTime = TimeSpan.Zero },
            new ProcessInfo { Pid = 2, Name = "beta", MemoryBytes = null, Threads = null, CpuTime = null },
        };

        var table = Renderer.Table(processes, "al", 1);

        Assert.Contains("toplam 2 surec", table, StringComparison.Ordinal);
        Assert.Contains("(filtre: al)", table, StringComparison.Ordinal);
        Assert.Contains("gosterilen 1", table, StringComparison.Ordinal);
        Assert.Contains("alpha", table, StringComparison.Ordinal);
        Assert.DoesNotContain("beta", table, StringComparison.Ordinal);
    }

    [Fact]
    public void UnknownValuesAreShownAsDash()
    {
        var processes = new[]
        {
            new ProcessInfo { Pid = 2, Name = "beta", MemoryBytes = null, Threads = null, CpuTime = null },
        };

        var table = Renderer.Table(processes, string.Empty, 10);

        Assert.Contains("-", table, StringComparison.Ordinal);
    }

    [Fact]
    public void TruncateKeepsWholeCharacters()
    {
        Assert.Equal("abc", Renderer.Truncate("abcdef", 3));
        Assert.Equal("cati", Renderer.Truncate("cati", 10));
        Assert.Equal("üü", Renderer.Truncate("üüü", 2));
    }
}
