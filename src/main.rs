use clap::Parser;
use image::{GenericImageView, ImageBuffer};
use image::{codecs::png::PngEncoder, io::Reader as ImageReader, ImageEncoder};
use rayon::prelude::*;
use std::fs;
use std::path::PathBuf;

#[derive(Parser, Debug)]
#[command(version, about = "Convert malformed DDNet skins into ones which will not cause an error when loaded in the DDNet client.", long_about = None)]
struct Args {
    /// Output file or directory
    #[arg(short, long)]
    output: PathBuf,

    /// Semicolon-separated list of input file(s)
    #[arg(short, long, value_delimiter = ';', num_args = 1..)]
    input: Vec<PathBuf>,
}

fn process_image(in_file: &PathBuf, out_file: &PathBuf) -> anyhow::Result<()> {
    let mut img = image::open(in_file).unwrap();

    println!(
        "'{}' -> '{}'",
        in_file.to_string_lossy(),
        out_file.to_string_lossy()
    );

    println!("{}x{} -> {}x{}", img.width(), img.height(), (img.width() + 7) & !7u32, (img.height() + 3) & !3u32);
    img = img.resize_to_fill((img.width() + 7) & !7u32, (img.height() + 3) & !3u32, image::imageops::FilterType::Lanczos3);

    img.save_with_format(out_file, image::ImageFormat::Png).unwrap();
    Ok(())
}

#[tokio::main]
async fn main() -> anyhow::Result<()> {
    let args = Args::parse();

    let in_files = args.input;
    if in_files.len() > 1 {
        if args.output.is_file() || !args.output.exists() {
            panic!("Output must be a pre-existing directory when multiple inputs are provided.")
        }
    }

    //in_files.into_par_iter().for_each(|file| {
    in_files.into_iter().for_each(|file| {
        if !file.is_file() {
            panic!(
                "Input must be pre-existing a file ({}).",
                file.clone().into_os_string().into_string().unwrap()
            )
        }

        // move check up
        let output : PathBuf = match args.output.is_dir() {
            true => {
                let mut o = args.output.clone();
                o.push(file.file_name().unwrap());    
                o
            },
            false => { args.output.to_owned() }
        };

        process_image(&std::path::absolute(&file).unwrap(), &std::path::absolute(&output).unwrap()).unwrap();
    });

    Ok(())
}
