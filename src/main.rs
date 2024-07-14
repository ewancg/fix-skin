use anyhow::{anyhow, Context};
use clap::Parser;
use glob::glob;
use rayon::prelude::*;
use std::borrow::Cow;
use std::fmt::Debug;
use std::path::{Path, PathBuf};
use std::sync::atomic::{AtomicUsize, Ordering};

// todo
// don't quote anyhow bail
// scaling filter cmd line arg?
// positional args

/// Convert malformed DDNet skins into ones which will not cause an error when loaded in the DDNet client.
/// Return codes:
///     0: Full success
///   > 0: Partial failure (x skins could not be converted)
///    -1: Full failure (all skins could nod be converted)
///    -2: Other error
#[derive(Parser, Debug)]
#[command(version, verbatim_doc_comment)]
struct Args {
    /// Output file or directory
    #[arg(short = 'o', long)]
    output: PathBuf,

    /// Semicolon-separated list of input file expressions (wildcard supported)
    #[arg(short = 'i', long, value_delimiter = ';', num_args = 1..)]
    input: Vec<String>,

    #[arg(short = 'v', long)]
    verbose: bool,
}

fn convert_skin(
    file: &Path,
    output: &Path,
) -> anyhow::Result<()> {
    // should be caught by glob
    //if !file.is_file() {
    //    anyhow::bail!("Input must be pre-existing a file ({file:?}).")
    //}

    let mut img = image::open(&file).map_err(|e| anyhow!("Image '{}' failed to load: {e}", std::path::absolute(&file).unwrap().display()))?;

    img = img.resize_to_fill(
        (img.width() + 7) & !7u32,
        (img.height() + 3) & !3u32,
        image::imageops::FilterType::Lanczos3,
    );

    img.save_with_format(output, image::ImageFormat::Png)?;

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

    let successes = AtomicUsize::new(0);

    all_files.into_par_iter().for_each(|file| {
        let mut o = Cow::Borrowed(&args.output);
        if args.output.is_dir() {
            o.to_mut().push(file.file_name().expect("What the fuck"));
        }
        match convert_skin(file.as_path(), o.as_path()) {
            Ok(_) => {
                if args.verbose {
                    println!("Successfully converted: {file:?} -> {o:?}");
                }
                successes.fetch_add(1, Ordering::SeqCst);
            }
            Err(e) => {
                eprintln!("{e:?}")
            }
        }
    });

    let x = successes.load(Ordering::SeqCst);
    let results = match total_files - x {
        0 => ProcessResults::Success,
        _ if total_files == x => ProcessResults::TotalFailure,
        x => ProcessResults::PartialFailure(x),
    };

    println!("Successfully converted {} skins.", x);

    Ok(results)
}

fn main() {
    match try_main() {
        Ok(ProcessResults::Success) => std::process::exit(0),
        Ok(ProcessResults::PartialFailure(x)) => std::process::exit(x.try_into().unwrap()),
        Ok(ProcessResults::TotalFailure) => std::process::exit(-1),
        Err(e) => {
            eprintln!("{e:#?}");
            std::process::exit(-2)
        }
    }
}
