use crate::error::AppError;

pub fn is_protected(pid: u32) -> bool {
    #[cfg(windows)]
    {
        pid == 0 || pid == 4
    }
    #[cfg(unix)]
    {
        pid == 0 || pid == 1
    }
}

pub fn self_pid() -> u32 {
    std::process::id()
}

pub fn kill(pid: u32) -> Result<(), AppError> {
    if is_protected(pid) {
        return Err(AppError::Denied(format!(
            "pid {pid} korumali bir sistem sureci"
        )));
    }
    if pid == self_pid() {
        return Err(AppError::Denied(
            "kendi surecini sonlandiramazsin".to_string(),
        ));
    }
    platform_kill(pid)
}

#[cfg(windows)]
mod windows_impl {
    use windows_sys::Win32::Foundation::{CloseHandle, GetLastError, ERROR_ACCESS_DENIED, ERROR_INVALID_PARAMETER, HANDLE};
    use windows_sys::Win32::System::Threading::{OpenProcess, TerminateProcess, PROCESS_TERMINATE};

    use crate::error::AppError;

    struct OwnedHandle(HANDLE);

    impl Drop for OwnedHandle {
        fn drop(&mut self) {
            if !self.0.is_null() {
                unsafe { CloseHandle(self.0) };
            }
        }
    }

    pub fn kill(pid: u32) -> Result<(), AppError> {
        let raw = unsafe { OpenProcess(PROCESS_TERMINATE, 0, pid) };
        if raw.is_null() {
            let code = unsafe { GetLastError() };
            return Err(if code == ERROR_INVALID_PARAMETER {
                AppError::NotFound(format!("pid {pid} bulunamadi"))
            } else {
                AppError::Denied(format!("pid {pid} icin yetki yok"))
            });
        }

        let handle = OwnedHandle(raw);
        let ok = unsafe { TerminateProcess(handle.0, 1) };
        if ok == 0 {
            let code = unsafe { GetLastError() };
            return Err(if code == ERROR_ACCESS_DENIED {
                AppError::Denied(format!("pid {pid} icin yetki yok"))
            } else {
                AppError::NotFound(format!("pid {pid} sonlandirilamadi"))
            });
        }
        Ok(())
    }
}

#[cfg(unix)]
mod unix_impl {
    use std::time::Duration;

    use crate::error::AppError;

    const TERM_GRACE: Duration = Duration::from_millis(200);

    fn last_error_is_permission() -> bool {
        std::io::Error::last_os_error().raw_os_error() == Some(libc::EPERM)
    }

    pub fn kill(pid: u32) -> Result<(), AppError> {
        let target = pid as libc::pid_t;

        if unsafe { libc::kill(target, libc::SIGTERM) } != 0 {
            return Err(if last_error_is_permission() {
                AppError::Denied(format!("pid {pid} icin yetki yok"))
            } else {
                AppError::NotFound(format!("pid {pid} bulunamadi"))
            });
        }

        std::thread::sleep(TERM_GRACE);

        if unsafe { libc::kill(target, 0) } == 0
            && unsafe { libc::kill(target, libc::SIGKILL) } != 0
            && last_error_is_permission()
        {
            return Err(AppError::Denied(format!("pid {pid} icin yetki yok")));
        }
        Ok(())
    }
}

#[cfg(windows)]
use windows_impl::kill as platform_kill;

#[cfg(unix)]
use unix_impl::kill as platform_kill;
