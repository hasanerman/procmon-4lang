use std::io::{self, BufRead, Write};
use std::process::ExitCode;
use std::sync::atomic::{AtomicBool, Ordering};

use procmon::cli::{self, Options};
use procmon::error::{AppError, EXIT_OK, EXIT_USAGE};
use procmon::kill;
use procmon::render;
use procmon::source::{self, ProcessSource};

static RUNNING: AtomicBool = AtomicBool::new(true);

fn main() -> ExitCode {
    let arguments: Vec<String> = std::env::args().skip(1).collect();

    let options = match cli::parse(&arguments) {
        Ok(options) => options,
        Err(error) => {
            eprintln!("hata: {error}\n{}", cli::usage());
            return ExitCode::from(EXIT_USAGE as u8);
        }
    };

    if options.help {
        print!("{}", cli::usage());
        return ExitCode::from(EXIT_OK as u8);
    }

    let result = if let Some(pid) = options.kill_pid {
        run_kill(pid)
    } else {
        run_monitor(&options)
    };

    match result {
        Ok(()) => ExitCode::from(EXIT_OK as u8),
        Err(error) => {
            eprintln!("{error}");
            ExitCode::from(error.exit_code() as u8)
        }
    }
}

fn run_kill(pid: u32) -> Result<(), AppError> {
    if kill::is_protected(pid) {
        return Err(AppError::Denied(format!(
            "pid {pid} korumali bir sistem sureci"
        )));
    }
    if pid == kill::self_pid() {
        return Err(AppError::Denied(
            "kendi surecini sonlandiramazsin".to_string(),
        ));
    }

    let mut monitor = ProcessSource::new();
    let processes = monitor.sample();
    let target = source::find(&processes, pid)
        .ok_or_else(|| AppError::NotFound(format!("pid {pid} bulunamadi")))?;

    print!("pid {pid} ({}) sonlandirilsin mi? [y/N] ", target.name);
    io::stdout().flush().ok();

    let mut answer = String::new();
    io::stdin().lock().read_line(&mut answer).ok();
    if !matches!(answer.trim(), "y" | "Y") {
        println!("iptal edildi");
        return Ok(());
    }

    kill::kill(pid)?;
    println!("pid {pid} sonlandirildi");
    Ok(())
}

fn run_monitor(options: &Options) -> Result<(), AppError> {
    install_interrupt_handler();
    render::enable_ansi();

    let mut monitor = ProcessSource::new();
    let interval = options.interval.max(ProcessSource::minimum_interval());

    std::thread::sleep(ProcessSource::minimum_interval());

    while RUNNING.load(Ordering::SeqCst) {
        let mut processes = source::filter_by_name(monitor.sample(), &options.filter);
        source::sort_processes(&mut processes, options.sort);

        if !options.once {
            render::clear();
        }
        print!("{}", render::table(&processes, &options.filter, options.top));
        io::stdout().flush().ok();

        if options.once {
            break;
        }
        std::thread::sleep(interval);
    }

    Ok(())
}

#[cfg(windows)]
fn install_interrupt_handler() {
    use windows_sys::Win32::Foundation::BOOL;
    use windows_sys::Win32::System::Console::SetConsoleCtrlHandler;

    unsafe extern "system" fn on_ctrl_event(_control_type: u32) -> BOOL {
        RUNNING.store(false, Ordering::SeqCst);
        1
    }

    unsafe {
        SetConsoleCtrlHandler(Some(on_ctrl_event), 1);
    }
}

#[cfg(not(windows))]
fn install_interrupt_handler() {
    extern "C" fn on_sigint(_signal: libc::c_int) {
        RUNNING.store(false, Ordering::SeqCst);
    }

    unsafe {
        libc::signal(libc::SIGINT, on_sigint as *const () as libc::sighandler_t);
    }
}
