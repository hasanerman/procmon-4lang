# procmon-4lang - Cross-Platform Process and Resource Monitor

The same process monitor, written four times: in C, C++, Rust and C#. All four implement the same command line contract, print the same table and pass the same tests. Then the difference between them is measured.

Status: all four languages are complete, they build, their tests pass and the benchmarks have been taken.

## Purpose

A command line tool that lists running processes, shows their CPU and RAM usage and thread count, filters them by name, and terminates a selected PID. The same tool is written separately in four languages; the goal is to see side by side how each language talks to the operating system, manages resources and handles errors.

This repository is the first of a 12-project series in which I solve the same problem in four languages and compare them by measurement rather than by opinion. Each project lives in its own repository. Rules that hold across the series:

- Language comparisons are measured, never guessed; the method and the environment are always stated.
- Errors are never swallowed silently: return codes in C, `std::expected` in C++, `Result` in Rust, exceptions in C#.
- Tests are mandatory; each language uses its own test tooling.
- Compiler warnings are treated as errors (`/W4 /WX`, `-Wall -Wextra -Werror`, `clippy -D warnings`, `TreatWarningsAsErrors`).

## Scope

Implemented:
- Process list: PID, name, RAM (working set / RSS), thread count, CPU percentage
- Periodic refresh with screen clearing (1000 ms by default)
- Filtering by name (case-insensitive substring search)
- Sorting by CPU, RAM, PID or name
- Termination by PID, with a confirmation prompt and meaningful exit codes (Windows: `TerminateProcess`; Linux: `SIGTERM` first, then `SIGKILL` after 200 ms)

Out of scope: graphical interface, network usage, disk I/O statistics, remote machine monitoring.

## Command line interface (shared contract for all four languages)

```
procmon [--interval <ms>] [--filter <text>] [--sort cpu|mem|pid|name] [--top <n>] [--kill <pid>] [--once] [--help]
```

Defaults: `--interval 1000`, `--sort cpu`, `--top 25`. `--interval` must be between 50 and 60000 ms, `--top` between 1 and 500. `--once` prints a single snapshot and exits.

Exit codes: 0 success, 1 invalid argument, 2 permission error or protected/own PID, 3 target process not found.

Sample output (runtime messages are currently in Turkish):

```
PID     NAME                              MEM(MB)  THREADS    CPU%
------------------------------------------------------------------
4812    chrome.exe                          569.5       62     2.1
1394    code.exe                            506.2       28     0.4

toplam 235 surec (filtre: c), gosterilen 2
```

## File count and layout

51 files in total: 1 document, 1 benchmark script, 1 build-all script, 3 repository files and 45 source files (C 13, C++ 14, Rust 10, C# 8).

```
procmon-4lang/
  README.md
  bench.ps1                  (PowerShell script that benchmarks all four binaries)
  buildeverything.bat        (builds and tests all four languages with one command)
  LICENSE
  .gitignore
  .gitattributes
  c/                         (13 files)
    Makefile                 (GNU make, Linux)
    build.bat                (MSVC, Windows; "build.bat test" also runs the tests)
    src/main.c               (argument flow, kill flow, monitoring loop)
    src/args.c
    src/args.h
    src/proc.h               (shared interface and ProcessInfo)
    src/proc_common.c        (list, filter, sort, CPU percentage)
    src/proc_win.c           (Windows: Toolhelp32 + GetProcessTimes)
    src/proc_linux.c         (Linux: /proc scan)
    src/render.c
    src/render.h
    tests/test_args.c
    tests/test_model.c
  cpp/                       (14 files)
    CMakeLists.txt
    src/main.cpp
    src/Args.hpp
    src/Args.cpp
    src/ProcessInfo.hpp
    src/ProcessSource.hpp    (abstract interface, Sample, error enums)
    src/WinProcessSource.cpp
    src/LinuxProcessSource.cpp
    src/ProcessModel.hpp
    src/ProcessModel.cpp
    src/Renderer.hpp
    src/Renderer.cpp
    tests/test_args.cpp
    tests/test_model.cpp
  rust/                      (10 files)
    Cargo.toml
    Cargo.lock
    src/lib.rs               (module tree; library target needed for integration tests)
    src/main.rs
    src/cli.rs
    src/source.rs            (sysinfo wrapper + Windows thread counting)
    src/kill.rs              (platform-specific termination, unsafe FFI)
    src/render.rs
    src/error.rs
    tests/cli_test.rs
  csharp/                    (8 files)
    ProcMon.slnx
    src/ProcMon/ProcMon.csproj
    src/ProcMon/Program.cs
    src/ProcMon/Args.cs
    src/ProcMon/ProcessSampler.cs
    src/ProcMon/Renderer.cs
    tests/ProcMon.Tests/ProcMon.Tests.csproj
    tests/ProcMon.Tests/ArgsTests.cs
```

Differences from the original plan and why:
- C gained `proc_common.c` and `render.h`, so that filtering, sorting and the CPU calculation stay platform-independent and testable.
- A single `Makefile` was not enough for C: the development machine has MSVC but no GNU make. `build.bat` covers MSVC, `Makefile` covers Linux/GCC.
- C++ splits out `ProcessInfo.hpp`, `ProcessModel.*` and `Renderer.hpp`; the tests reach the application logic through the `procmon_core` library target.
- Rust gained `src/lib.rs`: integration tests under `tests/` can only import a library target.
- The C# solution file is `.slnx` rather than `.sln`, because that is what the .NET 10 SDK produces by default.

## Architecture

Every language has the same three layers:

1. Source layer: fetches raw process data from the operating system. Platform-specific code lives here and nowhere else.
2. Model layer: the `ProcessInfo { pid, name, mem_bytes, threads, cpu_time, cpu_percent }` structure, filtering, sorting and the CPU percentage calculation.
3. Render layer: prints the table to the terminal and clears the screen.

Flow: `parse args -> loop { sample -> apply cpu -> filter -> sort -> take top N -> render -> sleep(interval) }`.

CPU percentage: the processor time delta between two samples, divided by the elapsed wall clock time and the core count. It cannot be computed from a single sample; the first round always shows `0.0` and every round after that is correct. That is why the previous sample is kept in a PID-keyed map.

### Language-specific architecture notes

- **C:** the platform split goes through `proc.h` with two separate `.c` files, each guarded by `#ifdef _WIN32`. The dynamic array grows by doubling through manual `realloc`. The snapshot function uses `goto cleanup` to keep a single exit point.
- **C++:** an abstract `ProcessSource` class, two concrete classes and a `makeProcessSource()` factory. `HANDLE` is wrapped in a `UniqueHandle` RAII type (non-copyable, movable). The error path is carried by `std::expected<T, E>`, which is why C++23 is required. Sorting uses `std::ranges::sort`.
- **Rust:** platform code is abstracted by the `sysinfo` crate; since `sysinfo` does not expose thread counts on Windows, they are collected separately through Toolhelp32. Termination calls `windows-sys` and `libc` directly in `kill.rs`; every `unsafe` block lives there, wrapped around an `OwnedHandle` that implements `Drop`.
- **C#:** `Process.GetProcesses()` plus LINQ for filtering and sorting. Every `Process` object is disposed through `using`. CPU uses the `TotalProcessorTime` delta, elapsed time uses `Stopwatch.GetElapsedTime`. The models are `record` types, so they are copied with `with`.

## Things to watch out for

General:
- Access to some processes is denied (system processes, processes of other users). This is not an error but a normal condition; the field shows `-` and the program does not crash.
- A process may exit while the list is being read (a race). Every read treats failure as normal.
- Passing your own PID to `--kill` is rejected (exit code 2).
- PID 0, 4 (Windows System) and 1 (Linux init) cannot be terminated; the tool rejects them up front.
- The screen is cleared with an ANSI escape sequence (`\x1b[2J\x1b[H`). On Windows this requires `ENABLE_VIRTUAL_TERMINAL_PROCESSING` in the console mode (C, C++, Rust); in C# the .NET console sets this itself, so only the output encoding is switched to UTF-8.
- The name column is cut at 30 characters; UTF-8 process names are never split mid-character (C and C++ inspect the lead byte, Rust uses `chars()`, C# uses `StringInfo`).

**C:**
- If `CreateToolhelp32Snapshot` returns `INVALID_HANDLE_VALUE` it is an error; `CloseHandle` is called on every path.
- `PROCESSENTRY32.dwSize` must be set before the call, otherwise `Process32First` fails.
- On Linux the second field of `/proc/<pid>/stat` (the name in parentheses) may contain spaces; the parser looks for the last `)`.
- If `realloc` fails the old pointer must not be lost; a temporary variable holds the result.
- Because MSVC builds with `/W4 /WX`, signed/unsigned conversions have to be written out explicitly.

**C++:**
- `UniqueHandle` deletes the copy constructor and writes the move constructor with `std::exchange`; otherwise the same `HANDLE` would be closed twice.
- Windows process names are UTF-16; the code uses `Process32FirstW`/`Process32NextW` and converts to UTF-8 with `WideCharToMultiByte`.
- `std::expected` requires MSVC 14.4x or GCC 13+; CMake asks for `CMAKE_CXX_STANDARD 23`.

**Rust:**
- `sysinfo` does not report a correct CPU value unless at least `MINIMUM_CPU_UPDATE_INTERVAL` (200 ms) passes between two refreshes; the program waits that long before the first sample, which is why its `--once` run is slower than the others.
- `unsafe` blocks exist only in `kill.rs` and in the Toolhelp32 call inside `source.rs`; raw handles are closed by an `OwnedHandle` that implements `Drop`.
- Ctrl+C uses a global `static AtomicBool`; storing a boxed closure was avoided because of the `clippy::type_complexity` warning.

**C#:**
- `Process.GetProcesses()` returns fresh objects on every call; without `using` their handles leak.
- `Process.WorkingSet64` and `Threads` throw `Win32Exception` on access denial; they are handled with filtered `catch` clauses.
- With `TreatWarningsAsErrors` enabled, analyzer warnings stop the build (`CA1305` makes `CultureInfo.InvariantCulture` mandatory).

## Language comparison (measured values)

Environment: Windows 10 Pro 19045, x64, MSVC 14.51, rustc 1.95.0, .NET 10.0.400. Command: `--once --top 1`, average of 6 runs after discarding the first of 7 (`bench.ps1`). The system had 233 - 269 processes at the time.

| Topic | C | C++23 | Rust | C# (.NET 10) |
|---|---|---|---|---|
| Process list source | Toolhelp32 / `/proc` | Same, with RAII | `sysinfo` + Toolhelp32 (threads) | `Process.GetProcesses()` |
| Resource management | `goto cleanup`, manual `CloseHandle` | `UniqueHandle` destructor | `Drop` (`OwnedHandle`) | `using` / `Dispose()` |
| Error handling | return code + `GetLastError` | `std::expected<T, E>` | `Result<T, AppError>` + `?` | exception (`ArgumentParseException`) |
| Dynamic list | manual `realloc` | `std::vector` | `Vec<T>` | `List<T>` / LINQ |
| Source lines (tests included) | 1265 | 1089 | 806 | 765 |
| Binary size (release) | 158.5 KB | 269.5 KB | 355.0 KB | 158.5 KB apphost + 25.0 KB dll (framework-dependent) |
| `--once` runtime (average) | 31.4 ms | 32.6 ms | 244.8 ms | 62.8 ms |
| `--once` runtime (best) | 26.2 ms | 29.7 ms | 242.2 ms | 61.3 ms |
| Peak RAM (peak working set) | 3.8 MB | 4.3 MB | 8.4 MB | 23.5 MB |
| Memory safety | programmer's responsibility | largely handled by RAII | guaranteed by the compiler | GC + managed |

How to read these numbers:

1. **The gap between C and C++ is not speed, it is size.** Virtual functions, `std::vector` and `std::format` cost no measurable time on this workload; the price showed up as 110 KB of extra binary. C++ delivers the same performance with far less manual cleanup.
2. **Rust's 244 ms is a library decision, not a language cost.** `sysinfo` insists on 200 ms between two samples to produce a correct CPU percentage. Remove that wait and the runtime lands in the same band as C and C++. The crate roughly halved the amount of code, but it also dictated the behaviour.
3. **C#'s 63 ms and 23 MB are the fixed price of the runtime** (JIT plus runtime loading, and a pre-allocated GC heap). In exchange, C# did the same job with the least code: 765 lines.
4. **Line counts show the price of abstraction:** C 1265 -> C++ 1089 -> Rust 806 -> C# 765. The lower you go, the closer the programmer stands to the operating system.

All four versions also kept a constant handle count during a 10-second continuous refresh run, so none of them leak. What differs is *how* the leak is prevented: manually in C, by a destructor in C++, by `Drop` in Rust, by `using` in C#.

## Development stages

1. List: fetch PID, name and RAM, print once (`--once`).
2. Extend: thread count and CPU percentage (delta of two samples).
3. Loop: periodic refresh, screen clearing, clean exit on Ctrl+C.
4. Filter and sort: `--filter`, `--sort`, `--top`.
5. Termination: `--kill` with a `y/N` confirmation and proper exit codes.
6. Test and measure: unit tests, a live snapshot test, comparison via `bench.ps1`.

## Requirements

| Requirement | Needed for | Note |
|---|---|---|
| Visual Studio Build Tools (MSVC, C++ workload) | C and C++ | ships with `cl` and `cmake` |
| CMake 3.24+ | C++ | the version bundled with Visual Studio works |
| Rust 1.75+ (`rustup`) | Rust | `cargo` and `clippy` |
| .NET SDK 10 | C# | `dotnet` |
| GCC/Clang and GNU make | C and C++ on Linux | not needed on Windows |

## Building and running

To build all four languages with one command, from the repository root:

```
buildeverything.bat            build only
buildeverything.bat test       build and run the tests of all four languages
```

The script locates the MSVC environment itself through `vswhere`, so there is no need to open a Developer Command Prompt. If a toolchain is missing (no `cargo`, for example) that language is skipped and the rest still builds. If any component fails, the exit code is 1.

To build a single language:

```
C (Windows):   cd c && .\build.bat            binary: c\build\procmon.exe
C (Linux):     cd c && make                   binary: c/build/procmon
C++:           cd cpp && cmake -S . -B build && cmake --build build --config Release
Rust:          cd rust && cargo build --release
C#:            cd csharp && dotnet build -c Release
```

Run examples:

```
c\build\procmon.exe --once --top 10 --sort mem
cpp\build\Release\procmon.exe --filter chrome
rust\target\release\procmon.exe --interval 500
csharp\src\ProcMon\bin\Release\net10.0\procmon.exe --kill 1234
```

## Testing and verification

```
C (Windows):   cd c && .\build.bat test
C (Linux):     cd c && make test          (memory check: make asan)
C++:           ctest --test-dir cpp\build -C Release --output-on-failure
Rust:          cd rust && cargo test && cargo clippy --all-targets -- -D warnings
C#:            cd csharp && dotnet test -c Release
```

Latest run: both C test binaries passed, both C++ test targets passed, 11 Rust tests passed, 25 C# tests passed. `clippy -D warnings` is clean, the C# build reports 0 warnings with `TreatWarningsAsErrors`, and C and C++ report 0 warnings with `/W4 /WX`.

Covered cases:
- Argument parsing: defaults, full command line, all four sort keys, unknown argument, missing value, out-of-range `--interval` and `--top`, non-numeric and trailing-garbage values, `--kill`, `--help`.
- Model: case-insensitive filter, empty filter, non-matching filter, all four sort orders, CPU percentage from two samples (with 1 and 2 cores), percentage staying 0 with a single sample.
- Rendering: `--top` and the filter text appearing in the footer, unknown values printed as `-`, UTF-8 names cut on a character boundary.
- Live system: finding your own PID in the snapshot, the protected-PID check, rejecting a kill on your own PID.

Kill flow verified manually (Windows):

```
$p = Start-Process ping -ArgumentList "-n","60","127.0.0.1" -PassThru -WindowStyle Hidden
cmd /c "echo y| c\build\procmon.exe --kill $($p.Id)"     -> terminated, exit 0
c\build\procmon.exe --kill 4                             -> protected system process, exit 2
c\build\procmon.exe --kill 999999                        -> not found, exit 3
c\build\procmon.exe --sort disk                          -> invalid argument, exit 1
```

Benchmarking: `pwsh .\bench.ps1` (7 runs by default, the first one discarded).

## Acceptance criteria

| Criterion | Status |
|---|---|
| Same CLI contract and same table format in all four languages | done |
| No crash when access to a process is denied | done, the field shows `-` |
| Kill rejected for own PID and protected PIDs | done, exit code 2 |
| Exit codes 0/1/2/3 verified | done |
| Unit and live tests pass in every language | done |
| No compiler or linter warnings | done |
| Comparison table filled with real measurements | done |
| No handle or memory growth during a long run | done; ran for 10 seconds at `--interval 50` (roughly 200 rounds), values below |

Leak check (start at second 2, end at second 10, `--interval 50 --top 5`):

| Language | Handles | Working set (MB) |
|---|---|---|
| C | 69 -> 69 | 3.3 -> 3.3 |
| C++ | 70 -> 70 | 3.8 -> 3.8 |
| Rust | 261 -> 261 | 7.1 -> 7.2 |
| C# | 249 -> 249 | 36.9 -> 35.2 |

The higher handle counts in Rust and C# are constant: the runtime opens them at startup and they do not grow during the loop. The working set dropping in C# is the GC stepping in.

## License

MIT. See [LICENSE](LICENSE).
