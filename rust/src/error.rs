use std::fmt;

pub const EXIT_OK: i32 = 0;
pub const EXIT_USAGE: i32 = 1;
pub const EXIT_DENIED: i32 = 2;
pub const EXIT_NOT_FOUND: i32 = 3;

#[derive(Debug, PartialEq, Eq)]
pub enum AppError {
    Usage(String),
    Denied(String),
    NotFound(String),
}

impl AppError {
    pub fn exit_code(&self) -> i32 {
        match self {
            AppError::Usage(_) => EXIT_USAGE,
            AppError::Denied(_) => EXIT_DENIED,
            AppError::NotFound(_) => EXIT_NOT_FOUND,
        }
    }
}

impl fmt::Display for AppError {
    fn fmt(&self, formatter: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            AppError::Usage(message) | AppError::Denied(message) | AppError::NotFound(message) => {
                write!(formatter, "{message}")
            }
        }
    }
}

impl std::error::Error for AppError {}
