use std::time::Duration;

use crate::error::AppError;

const DEFAULT_INTERVAL_MS: u64 = 1000;
const MIN_INTERVAL_MS: u64 = 50;
const MAX_INTERVAL_MS: u64 = 60_000;
const DEFAULT_TOP: usize = 25;
const MAX_TOP: usize = 500;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum SortKey {
    Cpu,
    Mem,
    Pid,
    Name,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Options {
    pub interval: Duration,
    pub filter: String,
    pub sort: SortKey,
    pub top: usize,
    pub kill_pid: Option<u32>,
    pub once: bool,
    pub help: bool,
}

impl Default for Options {
    fn default() -> Self {
        Self {
            interval: Duration::from_millis(DEFAULT_INTERVAL_MS),
            filter: String::new(),
            sort: SortKey::Cpu,
            top: DEFAULT_TOP,
            kill_pid: None,
            once: false,
            help: false,
        }
    }
}

pub fn usage() -> &'static str {
    "procmon [--interval <ms>] [--filter <metin>] [--sort cpu|mem|pid|name]\n\
     \x20       [--top <n>] [--kill <pid>] [--once] [--help]\n"
}

pub fn parse<I, S>(arguments: I) -> Result<Options, AppError>
where
    I: IntoIterator<Item = S>,
    S: AsRef<str>,
{
    let mut options = Options::default();
    let mut iterator = arguments.into_iter();

    while let Some(raw) = iterator.next() {
        let argument = raw.as_ref();

        match argument {
            "--help" | "-h" => {
                options.help = true;
                return Ok(options);
            }
            "--once" => {
                options.once = true;
                continue;
            }
            "--interval" | "--filter" | "--sort" | "--top" | "--kill" => {}
            other => return Err(AppError::Usage(format!("bilinmeyen arguman: {other}"))),
        }

        let value = iterator
            .next()
            .ok_or_else(|| AppError::Usage(format!("{argument} icin deger eksik")))?;
        let value = value.as_ref();

        match argument {
            "--filter" => options.filter = value.to_string(),
            "--sort" => options.sort = parse_sort(value)?,
            "--interval" => {
                let millis = parse_number(value)?;
                if !(MIN_INTERVAL_MS..=MAX_INTERVAL_MS).contains(&millis) {
                    return Err(AppError::Usage(
                        "interval 50 ile 60000 arasinda olmalidir".to_string(),
                    ));
                }
                options.interval = Duration::from_millis(millis);
            }
            "--top" => {
                let top = parse_number(value)? as usize;
                if top == 0 || top > MAX_TOP {
                    return Err(AppError::Usage("top 1 ile 500 arasinda olmalidir".to_string()));
                }
                options.top = top;
            }
            _ => {
                let pid = parse_number(value)?;
                if pid > u64::from(u32::MAX) {
                    return Err(AppError::Usage("pid degeri cok buyuk".to_string()));
                }
                options.kill_pid = Some(pid as u32);
            }
        }
    }

    Ok(options)
}

fn parse_number(value: &str) -> Result<u64, AppError> {
    value
        .parse::<u64>()
        .map_err(|_| AppError::Usage(format!("sayi bekleniyordu: {value}")))
}

fn parse_sort(value: &str) -> Result<SortKey, AppError> {
    match value {
        "cpu" => Ok(SortKey::Cpu),
        "mem" => Ok(SortKey::Mem),
        "pid" => Ok(SortKey::Pid),
        "name" => Ok(SortKey::Name),
        _ => Err(AppError::Usage(
            "sort cpu, mem, pid veya name olmalidir".to_string(),
        )),
    }
}
