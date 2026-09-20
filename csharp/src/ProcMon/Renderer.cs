using System.Globalization;
using System.Text;

namespace ProcMon;

public static class Renderer
{
    private const int NameColumn = 30;
    private const double BytesPerMb = 1024.0 * 1024.0;

    public static void EnableAnsi()
    {
        try
        {
            Console.OutputEncoding = Encoding.UTF8;
        }
        catch (IOException)
        {
            return;
        }
    }

    public static void Clear() => Console.Write("[2J[H");

    public static string Truncate(string text, int limit)
    {
        ArgumentNullException.ThrowIfNull(text);

        if (text.Length <= limit)
        {
            return text;
        }

        var enumerator = StringInfo.GetTextElementEnumerator(text);
        var builder = new StringBuilder();

        while (enumerator.MoveNext())
        {
            var element = (string)enumerator.Current;
            if (builder.Length + element.Length > limit)
            {
                break;
            }

            builder.Append(element);
        }

        return builder.ToString();
    }

    public static string Table(IReadOnlyList<ProcessInfo> processes, string filter, int top)
    {
        ArgumentNullException.ThrowIfNull(processes);

        var shown = Math.Min(processes.Count, top);
        var builder = new StringBuilder();
        var separator = new string('-', 7 + 1 + NameColumn + 1 + 10 + 1 + 8 + 1 + 7);

        builder.Append(CultureInfo.InvariantCulture, $"{"PID",-7} {"NAME",-30} {"MEM(MB)",10} {"THREADS",8} {"CPU%",7}");
        builder.AppendLine();
        builder.AppendLine(separator);

        foreach (var info in processes.Take(shown))
        {
            var memory = info.MemoryBytes.HasValue
                ? (info.MemoryBytes.Value / BytesPerMb).ToString("F1", CultureInfo.InvariantCulture)
                : "-";
            var threads = info.Threads?.ToString(CultureInfo.InvariantCulture) ?? "-";
            var cpu = info.CpuTime.HasValue
                ? info.CpuPercent.ToString("F1", CultureInfo.InvariantCulture)
                : "-";

            builder.Append(CultureInfo.InvariantCulture, $"{info.Pid,-7} {Truncate(info.Name, NameColumn),-30} {memory,10} {threads,8} {cpu,7}");
            builder.AppendLine();
        }

        builder.AppendLine();
        builder.Append(CultureInfo.InvariantCulture, $"toplam {processes.Count} surec");
        if (!string.IsNullOrEmpty(filter))
        {
            builder.Append(CultureInfo.InvariantCulture, $" (filtre: {filter})");
        }

        builder.Append(CultureInfo.InvariantCulture, $", gosterilen {shown}");
        builder.AppendLine();

        return builder.ToString();
    }
}
