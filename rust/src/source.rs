use std::collections::HashMap;

use sysinfo::{Pid, ProcessRefreshKind, ProcessesToUpdate, System};

use crate::cli::SortKey;

#[derive(Debug, Clone, PartialEq)]
pub struct ProcessInfo {
    pub pid: u32,
    pub name: String,
    pub mem_bytes: Option<u64>,
    pub threads: Option<u32>,
    pub cpu_percent: f32,
}

pub struct ProcessSource {
    system: System,
    cpu_count: usize,
}

impl ProcessSource {
    pub fn new() -> Self {
        let mut system = System::new();
        system.refresh_processes_specifics(
            ProcessesToUpdate::All,
            true,
            ProcessRefreshKind::nothing().with_cpu().with_memory(),
        );
        let cpu_count = std::thread::available_parallelism()
            .map(|value| value.get())
            .unwrap_or(1);

        Self { system, cpu_count }
    }

    pub fn minimum_interval() -> std::time::Duration {
        sysinfo::MINIMUM_CPU_UPDATE_INTERVAL
    }

    pub fn sample(&mut self) -> Vec<ProcessInfo> {
        self.system.refresh_processes_specifics(
            ProcessesToUpdate::All,
            true,
            ProcessRefreshKind::nothing().with_cpu().with_memory(),
        );

        let threads = thread_counts();
        let cores = self.cpu_count as f32;

        self.system
            .processes()
            .iter()
            .map(|(pid, process)| {
                let id = pid.as_u32();
                ProcessInfo {
                    pid: id,
                    name: process.name().to_string_lossy().to_string(),
                    mem_bytes: Some(process.memory()),
                    threads: thread_count_for(&threads, process, id),
                    cpu_percent: (process.cpu_usage() / cores).min(100.0),
                }
            })
            .collect()
    }
}

impl Default for ProcessSource {
    fn default() -> Self {
        Self::new()
    }
}

pub fn filter_by_name(processes: Vec<ProcessInfo>, needle: &str) -> Vec<ProcessInfo> {
    if needle.is_empty() {
        return processes;
    }
    let needle = needle.to_lowercase();
    processes
        .into_iter()
        .filter(|info| info.name.to_lowercase().contains(&needle))
        .collect()
}

pub fn sort_processes(processes: &mut [ProcessInfo], key: SortKey) {
    match key {
        SortKey::Cpu => processes.sort_by(|a, b| {
            b.cpu_percent
                .partial_cmp(&a.cpu_percent)
                .unwrap_or(std::cmp::Ordering::Equal)
                .then(a.pid.cmp(&b.pid))
        }),
        SortKey::Mem => processes.sort_by(|a, b| {
            b.mem_bytes
                .unwrap_or(0)
                .cmp(&a.mem_bytes.unwrap_or(0))
                .then(a.pid.cmp(&b.pid))
        }),
        SortKey::Pid => processes.sort_by_key(|info| info.pid),
        SortKey::Name => processes.sort_by(|a, b| {
            a.name
                .to_lowercase()
                .cmp(&b.name.to_lowercase())
                .then(a.pid.cmp(&b.pid))
        }),
    }
}

pub fn find(processes: &[ProcessInfo], pid: u32) -> Option<&ProcessInfo> {
    processes.iter().find(|info| info.pid == pid)
}

#[cfg(windows)]
fn thread_counts() -> HashMap<u32, u32> {
    use windows_sys::Win32::Foundation::{CloseHandle, INVALID_HANDLE_VALUE};
    use windows_sys::Win32::System::Diagnostics::ToolHelp::{
        CreateToolhelp32Snapshot, Process32First, Process32Next, PROCESSENTRY32, TH32CS_SNAPPROCESS,
    };

    let mut counts = HashMap::new();

    unsafe {
        let snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if snapshot == INVALID_HANDLE_VALUE {
            return counts;
        }

        let mut entry: PROCESSENTRY32 = std::mem::zeroed();
        entry.dwSize = std::mem::size_of::<PROCESSENTRY32>() as u32;

        if Process32First(snapshot, &mut entry) != 0 {
            loop {
                counts.insert(entry.th32ProcessID, entry.cntThreads);
                if Process32Next(snapshot, &mut entry) == 0 {
                    break;
                }
            }
        }
        CloseHandle(snapshot);
    }

    counts
}

#[cfg(not(windows))]
fn thread_counts() -> HashMap<u32, u32> {
    HashMap::new()
}

#[cfg(windows)]
fn thread_count_for(
    counts: &HashMap<u32, u32>,
    _process: &sysinfo::Process,
    pid: u32,
) -> Option<u32> {
    counts.get(&pid).copied()
}

#[cfg(not(windows))]
fn thread_count_for(
    _counts: &HashMap<u32, u32>,
    process: &sysinfo::Process,
    _pid: u32,
) -> Option<u32> {
    process.tasks().map(|tasks| tasks.len() as u32)
}

pub fn pid_from(value: u32) -> Pid {
    Pid::from_u32(value)
}
