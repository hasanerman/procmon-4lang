use crate::source::ProcessInfo;

const NAME_COLUMN: usize = 30;
const BYTES_PER_MB: f64 = 1024.0 * 1024.0;

#[cfg(windows)]
pub fn enable_ansi() {
    use windows_sys::Win32::System::Console::{
        GetConsoleMode, GetStdHandle, SetConsoleMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING,
        STD_OUTPUT_HANDLE,
    };

    unsafe {
        let handle = GetStdHandle(STD_OUTPUT_HANDLE);
        let mut mode = 0u32;
        if GetConsoleMode(handle, &mut mode) != 0 {
            SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }
}

#[cfg(not(windows))]
pub fn enable_ansi() {}

pub fn clear() {
    print!("\x1b[2J\x1b[H");
}

pub fn truncate_chars(text: &str, limit: usize) -> String {
    text.chars().take(limit).collect()
}

pub fn table(processes: &[ProcessInfo], filter: &str, top: usize) -> String {
    let shown = processes.len().min(top);
    let mut output = String::new();

    output.push_str(&format!(
        "{:<7} {:<width$} {:>10} {:>8} {:>7}\n",
        "PID",
        "NAME",
        "MEM(MB)",
        "THREADS",
        "CPU%",
        width = NAME_COLUMN
    ));
    output.push_str(&"-".repeat(7 + 1 + NAME_COLUMN + 1 + 10 + 1 + 8 + 1 + 7));
    output.push('\n');

    for info in processes.iter().take(shown) {
        let memory = match info.mem_bytes {
            Some(bytes) => format!("{:.1}", bytes as f64 / BYTES_PER_MB),
            None => "-".to_string(),
        };
        let threads = match info.threads {
            Some(count) => count.to_string(),
            None => "-".to_string(),
        };
        output.push_str(&format!(
            "{:<7} {:<width$} {:>10} {:>8} {:>7.1}\n",
            info.pid,
            truncate_chars(&info.name, NAME_COLUMN),
            memory,
            threads,
            info.cpu_percent,
            width = NAME_COLUMN
        ));
    }

    output.push_str(&format!("\ntoplam {} surec", processes.len()));
    if !filter.is_empty() {
        output.push_str(&format!(" (filtre: {filter})"));
    }
    output.push_str(&format!(", gosterilen {shown}\n"));
    output
}
