use clap::Parser;
use simplex::{AugMat, LPResult, solve_lp};

#[derive(Parser)]
#[command(
    version,
    about = "\
Solve a linear program using the simplex method.

INPUT FORMAT
  Line 1:   n m          (n variables, m constraints)
  Line 2:   c1 c2 … cn   (objective coefficients)
  Lines 3…: a1 a2 … an b (constraint row, RHS last)

EXAMPLE (from Homework 3, Problem 2)
  3 3
  1 2 4
  3 1 5 10
  1 4 1 8
  2 0 2 7
"
)]
struct Args {
    /// Input file (default: stdin). Use - to read from stdin explicitly.
    #[arg(default_value = "-")]
    input: String,
}

struct Scanner<R: std::io::BufRead> {
    reader: R,
    buffer: Vec<String>,
}

impl<R: std::io::BufRead> Scanner<R> {
    fn new(reader: R) -> Self {
        Scanner {
            reader,
            buffer: Vec::new(),
        }
    }

    fn next<T: std::str::FromStr>(&mut self) -> T {
        loop {
            if let Some(token) = self.buffer.pop() {
                return token.parse().ok().expect("failed to parse token");
            }
            let mut line = String::new();
            self.reader
                .read_line(&mut line)
                .expect("failed to read line");
            self.buffer = line.split_whitespace().rev().map(String::from).collect();
        }
    }
}

fn main() {
    let args = Args::parse();
    let reader: Box<dyn std::io::BufRead> = if args.input == "-" {
        Box::new(std::io::BufReader::new(std::io::stdin()))
    } else {
        Box::new(std::io::BufReader::new(
            std::fs::File::open(&args.input).expect("cannot open file"),
        ))
    };
    let mut sc = Scanner::new(reader);

    let n = sc.next::<usize>();
    let m = sc.next::<usize>();
    let obj = (0..n).map(|_| sc.next::<f64>()).collect::<Vec<_>>();
    let mut cons = AugMat::new(m, n);
    (0..m).for_each(|i| {
        let coefs = (0..n).map(|_| sc.next::<f64>()).collect::<Vec<_>>();
        let rhs = sc.next::<f64>();
        cons.set_row(i, &coefs, rhs);
    });

    match solve_lp(n, m, cons, obj) {
        LPResult::Optimal { ans, sln } => {
            println!("{}", ans);
            println!(
                "{}",
                sln.iter().map(f64::to_string).collect::<Vec<_>>().join(" ")
            );
        }
        LPResult::Infeasible => println!("Infeasible"),
        LPResult::Unbounded => println!("Unbounded"),
    }
}
