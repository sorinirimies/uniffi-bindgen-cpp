//! Command line interface.

use anyhow::{Context, Result, bail};
use camino::Utf8PathBuf;
use clap::{Parser, Subcommand};
use uniffi_bindgen::cargo_metadata::CrateConfigSupplier;

use crate::codegen::{CppBindingGenerator, write_runtime};

/// C++ bindings generator for UniFFI.
#[derive(Debug, Parser)]
#[command(name = "uniffi-bindgen-cpp", version)]
pub(crate) struct Cli {
    #[command(subcommand)]
    command: Command,
}

#[derive(Debug, Subcommand)]
enum Command {
    /// Generate C++ bindings.
    Generate {
        /// A cdylib/staticlib built with UniFFI (library mode), or a UDL file.
        source: Utf8PathBuf,

        /// Directory to write the generated files to.
        #[arg(short, long)]
        out_dir: Option<Utf8PathBuf>,

        /// A `uniffi.toml` that overrides the one in the crate.
        #[arg(short, long)]
        config: Option<Utf8PathBuf>,

        /// Only generate bindings for this crate (library mode).
        #[arg(long = "crate")]
        crate_name: Option<String>,

        /// The library to read definitions from when `source` is a UDL file.
        #[arg(long)]
        library: Option<Utf8PathBuf>,

        /// Do not run `clang-format` on the output.
        #[arg(long)]
        no_format: bool,

        /// Do not include dependencies when running `cargo metadata`.
        #[arg(long)]
        metadata_no_deps: bool,
    },
    /// Write only the runtime header, for projects that generate through another tool.
    Runtime {
        /// Directory to write `uniffi_runtime.hpp` to.
        out_dir: Utf8PathBuf,
    },
}

/// Parse the arguments and run.
pub fn run() -> Result<()> {
    match Cli::parse().command {
        Command::Generate {
            source,
            out_dir,
            config,
            crate_name,
            library,
            no_format,
            metadata_no_deps,
        } => generate(
            &source,
            out_dir,
            config.as_deref(),
            crate_name,
            library.as_deref(),
            !no_format,
            metadata_no_deps,
        ),
        Command::Runtime { out_dir } => write_runtime(&out_dir),
    }
}

fn generate(
    source: &camino::Utf8Path,
    out_dir: Option<Utf8PathBuf>,
    config: Option<&camino::Utf8Path>,
    crate_name: Option<String>,
    library: Option<&camino::Utf8Path>,
    format: bool,
    metadata_no_deps: bool,
) -> Result<()> {
    let generator = CppBindingGenerator;
    if uniffi_bindgen::is_cdylib(source) {
        let out_dir = out_dir.context("--out-dir is required when generating from a library")?;
        let supplier = CrateConfigSupplier::from_cargo_metadata_command(metadata_no_deps)?;
        uniffi_bindgen::library_mode::generate_bindings(
            source, crate_name, &generator, &supplier, config, &out_dir, format,
        )?;
        return Ok(());
    }
    if source.extension() != Some("udl") {
        bail!("`{source}` is neither a library nor a .udl file");
    }
    uniffi_bindgen::generate_external_bindings(
        &generator,
        source,
        config,
        out_dir.as_deref(),
        library,
        crate_name.as_deref(),
        format,
    )
}
