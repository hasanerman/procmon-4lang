using System.ComponentModel;
using System.Diagnostics;

namespace ProcMon;

public static class Program
{
    public const int ExitOk = 0;
    public const int ExitUsage = 1;
    public const int ExitDenied = 2;
    public const int ExitNotFound = 3;

    private static readonly CancellationTokenSource Cancellation = new();

    public static int Main(string[] arguments)
    {
        ArgumentNullException.ThrowIfNull(arguments);

        Options options;
        try
        {
            options = Args.Parse(arguments);
        }
        catch (ArgumentParseException exception)
        {
            Console.Error.WriteLine($"hata: {exception.Message}");
            Console.Error.Write(Args.Usage);
            return ExitUsage;
        }

        if (options.Help)
        {
            Console.Write(Args.Usage);
            return ExitOk;
        }

        Console.CancelKeyPress += (_, eventArgs) =>
        {
            eventArgs.Cancel = true;
            Cancellation.Cancel();
        };

        return options.KillPid is int pid ? RunKill(pid) : RunMonitor(options);
    }

    public static bool IsProtected(int pid) =>
        OperatingSystem.IsWindows() ? pid is 0 or 4 : pid is 0 or 1;

    private static int RunKill(int pid)
    {
        if (IsProtected(pid))
        {
            Console.Error.WriteLine($"pid {pid} korumali bir sistem sureci");
            return ExitDenied;
        }

        if (pid == Environment.ProcessId)
        {
            Console.Error.WriteLine("kendi surecini sonlandiramazsin");
            return ExitDenied;
        }

        Process target;
        try
        {
            target = Process.GetProcessById(pid);
        }
        catch (ArgumentException)
        {
            Console.Error.WriteLine($"pid {pid} bulunamadi");
            return ExitNotFound;
        }

        using (target)
        {
            var name = SafeName(target);
            Console.Write($"pid {pid} ({name}) sonlandirilsin mi? [y/N] ");
            var answer = Console.ReadLine()?.Trim();

            if (!string.Equals(answer, "y", StringComparison.OrdinalIgnoreCase))
            {
                Console.WriteLine("iptal edildi");
                return ExitOk;
            }

            try
            {
                target.Kill(entireProcessTree: false);
                target.WaitForExit(TimeSpan.FromSeconds(2));
            }
            catch (Win32Exception)
            {
                Console.Error.WriteLine($"pid {pid} icin yetki yok");
                return ExitDenied;
            }
            catch (InvalidOperationException)
            {
                Console.Error.WriteLine($"pid {pid} zaten sonlanmis");
                return ExitNotFound;
            }
        }

        Console.WriteLine($"pid {pid} sonlandirildi");
        return ExitOk;
    }

    private static int RunMonitor(Options options)
    {
        Renderer.EnableAnsi();

        var cores = Environment.ProcessorCount;
        Sample? previous = null;

        while (!Cancellation.IsCancellationRequested)
        {
            var current = ProcessSampler.ApplyCpuPercent(ProcessSampler.Take(), previous, cores);

            var visible = ProcessSampler.Filter(current.Processes, options.Filter);
            visible = ProcessSampler.Sort(visible, options.Sort);

            if (!options.Once)
            {
                Renderer.Clear();
            }

            Console.Write(Renderer.Table(visible, options.Filter, options.Top));
            previous = current;

            if (options.Once)
            {
                break;
            }

            try
            {
                Task.Delay(options.Interval, Cancellation.Token).GetAwaiter().GetResult();
            }
            catch (TaskCanceledException)
            {
                break;
            }
        }

        return ExitOk;
    }

    private static string SafeName(Process process)
    {
        try
        {
            return process.ProcessName;
        }
        catch (InvalidOperationException)
        {
            return "-";
        }
    }
}
