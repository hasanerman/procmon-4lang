namespace ProcMon;

public enum SortKey
{
    Cpu,
    Mem,
    Pid,
    Name,
}

public sealed record Options
{
    public const int DefaultIntervalMs = 1000;
    public const int MinIntervalMs = 50;
    public const int MaxIntervalMs = 60_000;
    public const int DefaultTop = 25;
    public const int MaxTop = 500;

    public TimeSpan Interval { get; init; } = TimeSpan.FromMilliseconds(DefaultIntervalMs);

    public string Filter { get; init; } = string.Empty;

    public SortKey Sort { get; init; } = SortKey.Cpu;

    public int Top { get; init; } = DefaultTop;

    public int? KillPid { get; init; }

    public bool Once { get; init; }

    public bool Help { get; init; }
}

public sealed class ArgumentParseException : Exception
{
    public ArgumentParseException(string message)
        : base(message)
    {
    }

    public ArgumentParseException()
    {
    }

    public ArgumentParseException(string message, Exception innerException)
        : base(message, innerException)
    {
    }
}

public static class Args
{
    public static string Usage =>
        "procmon [--interval <ms>] [--filter <metin>] [--sort cpu|mem|pid|name]" + Environment.NewLine +
        "        [--top <n>] [--kill <pid>] [--once] [--help]" + Environment.NewLine;

    public static Options Parse(IReadOnlyList<string> arguments)
    {
        ArgumentNullException.ThrowIfNull(arguments);

        var options = new Options();

        for (var index = 0; index < arguments.Count; index++)
        {
            var argument = arguments[index];

            switch (argument)
            {
                case "--help":
                case "-h":
                    return options with { Help = true };
                case "--once":
                    options = options with { Once = true };
                    continue;
                case "--interval":
                case "--filter":
                case "--sort":
                case "--top":
                case "--kill":
                    break;
                default:
                    throw new ArgumentParseException($"bilinmeyen arguman: {argument}");
            }

            if (index + 1 >= arguments.Count)
            {
                throw new ArgumentParseException($"{argument} icin deger eksik");
            }

            var value = arguments[++index];

            options = argument switch
            {
                "--filter" => options with { Filter = value },
                "--sort" => options with { Sort = ParseSort(value) },
                "--interval" => options with { Interval = ParseInterval(value) },
                "--top" => options with { Top = ParseTop(value) },
                _ => options with { KillPid = ParsePid(value) },
            };
        }

        return options;
    }

    private static SortKey ParseSort(string value) => value switch
    {
        "cpu" => SortKey.Cpu,
        "mem" => SortKey.Mem,
        "pid" => SortKey.Pid,
        "name" => SortKey.Name,
        _ => throw new ArgumentParseException("sort cpu, mem, pid veya name olmalidir"),
    };

    private static TimeSpan ParseInterval(string value)
    {
        var millis = ParseNumber(value);
        if (millis < Options.MinIntervalMs || millis > Options.MaxIntervalMs)
        {
            throw new ArgumentParseException("interval 50 ile 60000 arasinda olmalidir");
        }
        return TimeSpan.FromMilliseconds(millis);
    }

    private static int ParseTop(string value)
    {
        var top = ParseNumber(value);
        if (top is 0 or > Options.MaxTop)
        {
            throw new ArgumentParseException("top 1 ile 500 arasinda olmalidir");
        }
        return (int)top;
    }

    private static int ParsePid(string value)
    {
        var pid = ParseNumber(value);
        if (pid > int.MaxValue)
        {
            throw new ArgumentParseException("pid degeri cok buyuk");
        }
        return (int)pid;
    }

    private static long ParseNumber(string value)
    {
        if (!long.TryParse(value, System.Globalization.NumberStyles.None, System.Globalization.CultureInfo.InvariantCulture, out var number))
        {
            throw new ArgumentParseException($"sayi bekleniyordu: {value}");
        }
        return number;
    }
}
