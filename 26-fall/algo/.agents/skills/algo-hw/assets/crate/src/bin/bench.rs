//! Wall-clock benchmark of the algorithms in this task.
//!
//! Usage:
//!     cargo run --release --bin bench > report/bench.csv
//!
//! Prints `<size>,algorithm,time_ms,check` rows: `time_ms` is the mean runtime
//! of one run after repeating each (algorithm, size) pair until `BUDGET` is
//! spent, and `check` is `ok` when the output equalled the naive reference, `no`
//! when it did not, and `na` when no reference was run for that size. A size
//! with several numbers gets one CSV column per number.

use std::time::{Duration, Instant};

use {{CRATE}}::generator::{DEFAULT_SEED, Input, Size, generate_with_seed};
use {{CRATE}}::{{{ALGORITHM}}, Solution};

/// The measured input sizes, smallest first. Widen `Size` in generator.rs and
/// this list together when the size has several numbers, e.g.
/// `const SIZES: [(usize, usize); 3] = [(1000, 10), (1000, 100), (1000, 1000)];`
const SIZES: [Size; {{SIZES_LEN}}] = [{{SIZES}}];

/// How the sizes appear in the CSV, one column per number.
const SIZE_HEADER: &str = "{{SIZE_HEADER}}";

/// Renders one size as its CSV columns.
fn size_columns(size: Size) -> String {
    {{SIZE_FORMAT}}
}

/// Runs one algorithm on one generated input.
///
/// TODO(algo-hw): when the operation takes several arguments, pass the parts
/// of `input` here, e.g. `input.0, input.1`.
fn run<S: Solution>(input: &Input) -> {{OUT}} {
    S::{{OP}}({{CALL_ARGS}})
}

/// Signature of one benchmarked algorithm.
type Bench = fn(&Input) -> {{OUT}};

/// Repeat each (algorithm, size) pair until this much time was spent on it; an
/// algorithm slower than the budget still runs once.
const BUDGET: Duration = Duration::from_millis(500);

/// Give up on an algorithm that spends more than this on one size.
const HARD_LIMIT: Duration = Duration::from_secs(300);

/// Name, how many of `SIZES` to run, and function of every algorithm. The
/// reference algorithm comes first: its output is what the others are checked
/// against.
///
/// TODO(algo-hw): add the remaining algorithms. An algorithm that is too slow
/// or too memory hungry at large sizes gets a smaller count, whatever its
/// complexity.
const ALGORITHMS: [(&str, usize, Bench); 1] =
    [("naive", {{SIZES_LEN}}, run::<{{ALGORITHM}}>)];

/// Runs `f` on `input` until `BUDGET` is spent; returns the mean milliseconds
/// of one run, the number of runs, the last output, and the total time spent.
fn measure(f: Bench, input: &Input) -> (f64, usize, {{OUT}}, Duration) {
    let mut runs = 0;
    let mut total = Duration::ZERO;
    let mut out = f(input);
    while total < BUDGET {
        let start = Instant::now();
        out = f(input);
        total += start.elapsed();
        runs += 1;
    }
    (total.as_secs_f64() * 1e3 / runs as f64, runs, out, total)
}

fn main() {
    println!("{SIZE_HEADER},algorithm,time_ms,check");
    let mut alive = [true; ALGORITHMS.len()];

    for (index, &size) in SIZES.iter().enumerate() {
        eprintln!("--- size = {} ---", size_columns(size));
        let input = generate_with_seed(size, DEFAULT_SEED);
        let mut reference: Option<{{OUT}}> = None;

        for (i, &(name, count, run)) in ALGORITHMS.iter().enumerate() {
            if !alive[i] || index >= count {
                continue;
            }
            let (time_ms, runs, out, total) = measure(run, &input);
            let check = match &reference {
                None => "na",
                Some(want) if want == &out => "ok",
                Some(_) => "no",
            };
            eprintln!("size = {}: {name:<12} {time_ms:>12.4} ms over {runs:>4} run(s), {check}",
                      size_columns(size));
            println!("{},{name},{time_ms:.6},{check}", size_columns(size));
            if i == 0 {
                reference = Some(out);
            }
            if total > HARD_LIMIT {
                eprintln!("  spent {total:?}, giving up on larger sizes");
                alive[i] = false;
            }
        }
    }
}
