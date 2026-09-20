use std::time::Duration;

use procmon::cli::{self, SortKey};
use procmon::error::AppError;
use procmon::kill;
use procmon::render;
use procmon::source::{self, ProcessInfo, ProcessSource};

fn info(pid: u32, name: &str, mem: u64, cpu: f32) -> ProcessInfo {
    ProcessInfo {
        pid,
        name: name.to_string(),
        mem_bytes: Some(mem),
        threads: Some(4),
        cpu_percent: cpu,
    }
}

fn sample() -> Vec<ProcessInfo> {
    vec![
        info(10, "Chrome.exe", 300 * 1024 * 1024, 12.5),
        info(20, "code.exe", 900 * 1024 * 1024, 1.0),
        info(5, "alpha", 10 * 1024 * 1024, 40.0),
    ]
}

#[test]
fn defaults_are_applied() {
    let options = cli::parse::<[&str; 0], &str>([]).unwrap();

    assert_eq!(options.interval, Duration::from_millis(1000));
    assert_eq!(options.top, 25);
    assert_eq!(options.sort, SortKey::Cpu);
    assert!(!options.once);
    assert_eq!(options.kill_pid, None);
    assert!(options.filter.is_empty());
}

#[test]
fn full_command_line_is_parsed() {
    let options = cli::parse([
        "--interval", "250", "--filter", "chrome", "--sort", "mem", "--top", "5", "--once",
    ])
    .unwrap();

    assert_eq!(options.interval, Duration::from_millis(250));
    assert_eq!(options.filter, "chrome");
    assert_eq!(options.sort, SortKey::Mem);
    assert_eq!(options.top, 5);
    assert!(options.once);
}

#[test]
fn sort_keys_are_parsed() {
    assert_eq!(cli::parse(["--sort", "cpu"]).unwrap().sort, SortKey::Cpu);
    assert_eq!(cli::parse(["--sort", "mem"]).unwrap().sort, SortKey::Mem);
    assert_eq!(cli::parse(["--sort", "pid"]).unwrap().sort, SortKey::Pid);
    assert_eq!(cli::parse(["--sort", "name"]).unwrap().sort, SortKey::Name);
    assert!(cli::parse(["--sort", "disk"]).is_err());
}

#[test]
fn bad_values_are_rejected() {
    for arguments in [
        vec!["--zoom"],
        vec!["--interval"],
        vec!["--interval", "10"],
        vec!["--interval", "999999"],
        vec!["--top", "0"],
        vec!["--top", "abc"],
        vec!["--top", "12x"],
        vec!["--kill", "-3"],
    ] {
        let parsed = cli::parse(arguments.clone());
        assert!(parsed.is_err(), "kabul edilmemeliydi: {arguments:?}");
        assert!(matches!(parsed.unwrap_err(), AppError::Usage(_)));
    }
}

#[test]
fn kill_and_help_are_parsed() {
    let options = cli::parse(["--kill", "4321"]).unwrap();
    assert_eq!(options.kill_pid, Some(4321));
    assert!(cli::parse(["--help"]).unwrap().help);
}

#[test]
fn filter_is_case_insensitive() {
    let filtered = source::filter_by_name(sample(), "CHROME");
    assert_eq!(filtered.len(), 1);
    assert_eq!(filtered[0].pid, 10);

    assert_eq!(source::filter_by_name(sample(), "").len(), 3);
    assert!(source::filter_by_name(sample(), "yok-boyle").is_empty());
}

#[test]
fn sorting_orders_rows() {
    let mut processes = sample();

    source::sort_processes(&mut processes, SortKey::Mem);
    assert_eq!(processes[0].pid, 20);
    assert_eq!(processes[2].pid, 5);

    source::sort_processes(&mut processes, SortKey::Cpu);
    assert_eq!(processes[0].pid, 5);

    source::sort_processes(&mut processes, SortKey::Pid);
    assert_eq!(processes[0].pid, 5);
    assert_eq!(processes[2].pid, 20);

    source::sort_processes(&mut processes, SortKey::Name);
    assert_eq!(processes[0].name, "alpha");
    assert_eq!(processes[1].name, "Chrome.exe");
    assert_eq!(processes[2].name, "code.exe");
}

#[test]
fn table_respects_top_and_filter() {
    let mut processes = sample();
    source::sort_processes(&mut processes, SortKey::Pid);

    let output = render::table(&processes, "chrome", 2);
    assert!(output.contains("toplam 3 surec"));
    assert!(output.contains("(filtre: chrome)"));
    assert!(output.contains("gosterilen 2"));
    assert!(output.contains("alpha"));
    assert!(!output.contains("code.exe"));
}

#[test]
fn truncate_keeps_whole_characters() {
    assert_eq!(render::truncate_chars("abcdef", 3), "abc");
    assert_eq!(render::truncate_chars("cati", 10), "cati");
    assert_eq!(render::truncate_chars("üüü", 2), "üü");
}

#[test]
fn live_snapshot_contains_self() {
    let mut monitor = ProcessSource::new();
    let processes = monitor.sample();

    assert!(!processes.is_empty());
    assert!(source::find(&processes, kill::self_pid()).is_some());
}

#[test]
fn protected_and_self_pids_are_rejected() {
    assert!(kill::is_protected(0));
    assert!(matches!(kill::kill(0), Err(AppError::Denied(_))));
    assert!(matches!(
        kill::kill(kill::self_pid()),
        Err(AppError::Denied(_))
    ));
}
