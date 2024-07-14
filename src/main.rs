use anyhow::{anyhow, Context};
use clap::Parser;
use glob::glob;
use rayon::prelude::*;
use std::borrow::Cow;
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicUsize, Ordering};

/// Convert malformed DDNet skins into ones which will not cause an error when loaded in the DDNet client.
/// Return codes:
///     0: Success
///    >0: Partial failure (x skins failed)
///    -1: indicates full failure
///    -2: other error
#[derive(Parser, Debug)]
#[command(version, verbatim_doc_comment)]
struct Args {
    /// Output file or directory
    #[arg(short, long)]
    output: PathBuf,

    /// Semicolon-separated list of input files or directories
    #[arg(short, long, value_delimiter = ';', num_args = 1..)]
    input: Vec<String>,
}

fn process_image(in_file: &Path, out_file: &Path) -> anyhow::Result<()> {
    let mut img = image::open(in_file)?;

    println!(
        "'{}' -> '{}'",
        in_file.to_string_lossy(),
        out_file.to_string_lossy()
    );

    img = img.resize_to_fill(
        (img.width() + 7) & !7u32,
        (img.height() + 3) & !3u32,
        image::imageops::FilterType::Lanczos3,
    );

    img.save_with_format(out_file, image::ImageFormat::Png)?;
    Ok(())
}

fn process_files(file: PathBuf, output: impl AsRef<Path>) -> anyhow::Result<()> {
    if !file.is_file() {
        anyhow::bail!("Input must be pre-existing a file ({file:?}).")
    }

    // move check up
    let mut o = Cow::Borrowed(output.as_ref());
    if o.is_dir() {
        o.to_mut().push(file.file_name().context("1234567")?);
    }

    process_image(&std::path::absolute(&file)?, &std::path::absolute(o)?)
        .with_context(|| anyhow!("Unable to convert file {file:?}."))?;

    Ok(())
}

enum ProcessResults {
    Success,
    PartialFailure(usize),
    TotalFailure,
}

fn try_main() -> anyhow::Result<ProcessResults> {
    let args = Args::parse();

    let in_file_expressions = args.input;
    let mut all_files = vec![];

    for file in in_file_expressions {
        for entry in glob(&file).map_err(|f| anyhow!("Failed to read glob pattern: {f}"))? {
            match entry {
                Ok(path) => {
                    if !path.is_file() {
                        eprintln!("Ignoring non-file directory item '{path:?}'");
                    }
                    all_files.push(path);
                }
                Err(e) => anyhow::bail!("{e:?}"),
            }
        }
    }

    let total_files = all_files.len();
    match total_files {
        0 => {
            anyhow::bail!("Input expression(s) yielded no files.")
        }
        1 => {}
        _ => {
            if args.output.is_file() || !args.output.exists() {
                anyhow::bail!(
                    "Output must be a pre-existing directory when multiple inputs are provided."
                )
            }
        }
    }

    let failures = AtomicUsize::new(0);
    all_files
        .into_par_iter()
        .for_each(|file| match process_files(file, &args.output) {
            Ok(_) => {}
            Err(e) => {
                failures.fetch_add(1, Ordering::SeqCst);
                eprintln!("{e:?}")
            }
        });

    let x = failures.load(Ordering::SeqCst);
    let results = match x {
        0 => ProcessResults::Success,
        _ if total_files == x => ProcessResults::TotalFailure,
        x => ProcessResults::PartialFailure(x),
    };

    Ok(results)
}

fn main() {
    match try_main() {
        Ok(ProcessResults::Success) => std::process::exit(0),
        Ok(ProcessResults::PartialFailure(x)) => std::process::exit(x.try_into().unwrap()),
        Ok(ProcessResults::TotalFailure) => std::process::exit(-1),
        Err(e) => {
            eprintln!("{e:?}");
            std::process::exit(-2)
        }
    }
}
