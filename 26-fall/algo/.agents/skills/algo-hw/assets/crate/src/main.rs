//! Command line interface. The pair below (`gen`, `run`) is the common shape;
//! rename or drop subcommands to match what the assignment asks for.
//! `gen --size <size> [--seed S]` writes a random input, `run (--naive |
//! --<algorithm>)` solves an input read from stdin.
//!
//! The data format is the size on the first line, then one item per line (or
//! per pair of tokens) -- adjust `parse_input` and the printing in `cmd_gen`.

use std::io::{self, Read};

use {{CRATE}}::generator::{DEFAULT_SEED, Input, Size, generate_with_seed};
use {{CRATE}}::{{{ALGORITHM}}, Solution};

const USAGE: &str = "\
usage:
  {{NAME}} gen --size {{SIZE_HELP}} [--seed S]
  {{NAME}} run (--naive | --<algorithm>)
";

fn main() {
    let mut args = std::env::args().skip(1);
    let result = match args.next().as_deref() {
        Some("gen") => cmd_gen(args),
        Some("run") => cmd_run(args),
        _ => {
            eprint!("{USAGE}");
            std::process::exit(2);
        }
    };
    if let Err(message) = result {
        eprintln!("error: {message}");
        std::process::exit(1);
    }
}

fn cmd_gen(mut args: impl Iterator<Item = String>) -> Result<(), String> {
    let mut size = None;
    let mut seed = DEFAULT_SEED;
    while let Some(arg) = args.next() {
        match arg.as_str() {
            "--size" => size = Some(next_value(&mut args, "--size")?),
            "--seed" => seed = next_value(&mut args, "--seed")?,
            other => return Err(format!("unexpected argument `{other}`")),
        }
    }
    let size_text: String = size.ok_or("missing required option `--size`")?;
    let size = parse_size(&size_text)?;
    println!("{}", size_line(size));
    let input = generate_with_seed(size, seed);
    // TODO(algo-hw): write `input` in the task's data format, e.g. one item per
    // line when `Input` is a `Vec`.
    println!("{input:?}");
    Ok(())
}

fn cmd_run(args: impl Iterator<Item = String>) -> Result<(), String> {
    let mut algorithm = None;
    for arg in args {
        match arg.as_str() {
            "--naive" => algorithm = Some("naive"),
            other => return Err(format!("unexpected argument `{other}`")),
        }
    }
    algorithm.ok_or("missing required option `--naive` or an algorithm")?;

    let mut input = String::new();
    io::stdin()
        .read_to_string(&mut input)
        .map_err(|error| error.to_string())?;
    let items = parse_input(&input)?;

    // TODO(algo-hw): dispatch on `algorithm` once the other algorithms exist.
    let answer = solve::<{{ALGORITHM}}>(&items);
    // TODO(algo-hw): print the answer in the format the task asks for.
    println!("{answer:?}");
    Ok(())
}

/// Runs one algorithm on the parsed input.
///
/// TODO(algo-hw): when the operation takes several arguments, pass the parts
/// of `input` here, e.g. `input.0, input.1`.
fn solve<S: Solution>(input: &Input) -> {{OUT}} {
    S::{{OP}}({{CALL_ARGS}})
}

/// Parses `n` followed by the task's items.
fn parse_input(input: &str) -> Result<Input, String> {
    let mut tokens = input.split_whitespace();
    let n: usize = tokens
        .next()
        .ok_or("empty input")?
        .parse()
        .map_err(|_| "the first value must be the number of items".to_string())?;
    // TODO(algo-hw): parse the task's items. For large lattice inputs replace
    // this with the `Scanner<R: BufRead>` from references/rust-conventions.md.
    let _ = (tokens, n);
    todo!("parse the task's items")
}

/// Renders the size for the data file's first line. Follow the format the
/// assignment (or the judge) specifies when it is not simply one number.
fn size_line(size: Size) -> String {
    {{SIZE_LINE}}
}

/// Parses `--size`, e.g. `{{SIZE_HELP}}`.
fn parse_size(text: &str) -> Result<Size, String> {
    {{SIZE_PARSE}}
}

/// Parses the value of an option such as `--size 1000`.
fn next_value<T: std::str::FromStr>(
    args: &mut impl Iterator<Item = String>,
    option: &str,
) -> Result<T, String> {
    let value = args.next().ok_or_else(|| format!("`{option}` needs a value"))?;
    value
        .parse()
        .map_err(|_| format!("`{option}` has an invalid value `{value}`"))
}
