//! The `uniffi-bindgen-cpp` binary: parses the arguments and reports errors.

use std::process::ExitCode;

fn main() -> ExitCode {
    match uniffi_bindgen_cpp::run() {
        Ok(()) => ExitCode::SUCCESS,
        Err(error) => {
            eprintln!("error: {error:#}");
            ExitCode::FAILURE
        }
    }
}
