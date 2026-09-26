# Rust conventions

Extracted from `01-matmul` and `02-closest-pair`; new code keeps this shape.

## Crate

`cargo init --lib --vcs=none --name=<task-name>` in `hwNN/MM-topic/`, with

```toml
[package]
name = "<task-name>"
version = "0.1.0"
edition = "2024"

[dependencies]
```

No dependencies, ever — the machine is often offline. `--vcs=none` because the
course folder is not a git repository.

## Algorithm modules

- `src/solution.rs` holds the trait the task is about, with associated
  (static) functions. The parameter list is the task's own — never assume a
  single `&[Item]`:

  ```rust
  pub trait Solution {
      fn closest_pair_distance(points: &[Point]) -> f64;
      // two operands:
      //   fn mat_mul(a: &[&[i32]], b: &[&[i32]]) -> Vec<Vec<i32>>;
      // a slice and a number:
      //   fn kth_smallest(values: &[i32], k: usize) -> i32;
  }
  ```

- One unit struct per algorithm in its own file, e.g. `src/naive.rs`:

  ```rust
  pub struct Naive;

  impl Solution for Naive {
      fn closest_pair_distance(points: &[Point]) -> f64 { ... }
  }
  ```

- These are the user's files. Read them; never rewrite them.

## lib.rs

Module docs, `mod` declarations, `pub use` re-exports, then tests in place:

```rust
//! Comparison of <the algorithms>.
//!
//! An algorithm is a unit struct which implements the Solution trait. See
//! solution.rs for details. The Naive struct in naive.rs implements the
//! O(...) algorithm.

mod naive;
mod solution;

pub use naive::Naive;
pub use solution::Solution;

#[cfg(test)]
mod tests {
    use super::*;
    // The PRNG and the random input come from generator.rs; never duplicate
    // them here.
    use crate::generator::{Rng, generate_with_seed};
}
```

Reuse `generator.rs`. The tests call `generate` / `generate_with_seed`, so a
failure is reproducible from its seed, and the tests and the benchmark share a
single definition of "random input". Only a task without a generator module
(an early homework, or one with no random input) keeps a small private `Rng`
inside the tests, the way `01-matmul` does.

2-3 tests, each with a doc comment stating what it exercises:

1. a hand-computed case with a known answer,
2. a boundary or degenerate case (n = 1 or 2, duplicates, collinear,
   all-equal),
3. randomized agreement of every algorithm with the naive reference.

Share one generic helper (`fn test_<op><S: Solution>(lhs, rhs)`), compare
structures element by element with the size in the assertion message, and use
deterministic seeds. Pick sizes that actually stress the algorithm (powers of
two for recursive matrix code, clustered or duplicated points for geometry).

## generator.rs

```rust
/// One measured input size. Widen it when the task measures more than one
/// number, e.g. `pub type Size = (usize, usize);`.
pub type Size = usize;

/// A small xorshift64* pseudo random number generator.
pub struct Rng { state: u64 }

impl Rng {
    pub fn new(seed: u64) -> Self { ... }
    pub fn next_u64(&mut self) -> u64 { ... }
    /// Returns a uniform random `f64` in `[0, 1)`.
    pub fn next_f64(&mut self) -> f64 {
        (self.next_u64() >> 11) as f64 * (1.0 / (1u64 << 53) as f64)
    }
    /// Returns a uniform random integer in `lo..=hi`.
    pub fn next_range(&mut self, lo: i64, hi: i64) -> i64 {
        lo + (self.next_u64() % (hi - lo + 1) as u64) as i64
    }
}

pub const DEFAULT_SEED: u64 = 0x9e37_79b9_7f4a_7c15;
pub type Input = Vec<Item>;      // a tuple when the operation takes several arguments
pub fn generate(size: Size) -> Input;
pub fn generate_with_seed(size: Size, seed: u64) -> Input;
```

The module is `generator.rs`, never `gen.rs`: `gen` is a reserved keyword in
edition 2024. `Size` declares the shape of an input size and `Input` the shape
of one generated input; the benchmark, the CLI and the tests all use them.

## main.rs — only when the task has an interface

The subcommands are whatever the task needs. Generating a random input and
solving a given one (`gen` / `run`) is the common pair, but a task might want
`solve`, `check`, a single default command, or no CLI at all — never add a
subcommand the assignment does not ask for. Keep the usage text in a
`const USAGE`, parse arguments by hand, write errors to stderr with a non-zero
exit. For large inputs use a small tokenizer rather than reading the whole
input into `String`:

```rust
/// Reads whitespace separated numbers from a buffered reader.
struct Scanner<R: BufRead> {
    reader: R,
    token: Vec<u8>,
}
```

implemented with `fill_buf`/`consume` and a reusable token buffer, exposing
`next_i64`/`next_usize` (plus `next_i32` when coordinates are bounded). No
`clap`, no `rand`.

## bin/bench.rs

House shape (see `01-matmul/src/bin/bench.rs` and
`02-closest-pair/src/bin/bench.rs`):

- a doc comment carrying the exact command, e.g.
  `cargo run --release --bin bench > report/bench.csv`;
- `SIZES` const — one entry per measured size, typed with `generator::Size`, so
  a size with several numbers is a tuple — plus, per algorithm, how many of
  those sizes it runs (a prefix of the ladder; an algorithm that is too slow
  or too memory hungry gets a smaller count, whatever its complexity);
- `const BUDGET: Duration = Duration::from_millis(500);` — repeat each
  (algorithm, size) pair until the budget is spent, then report the **mean** of
  one run; an algorithm slower than the budget still runs once;
- the naive algorithm first: its output is that size's reference;
- one adapter per algorithm keeps the task's parameter list in one place:
  `fn run<S: Solution>(input: &Input) -> Out { S::op(input.0, input.1) }`, with
  `type Bench = fn(&Input) -> Out;` and `run::<Naive>` as the table entry;
- stdout `<size>,algorithm,time_ms,check`, where `check` is `ok`, `no`, or `na`
  (no reference was run); stderr carries progress and a mismatch line;
- a hard time limit that retires an algorithm which is hopeless at large sizes.

### When the input size is not a single number

Plenty of tasks scale more than one dimension: two sequence lengths for edit
distance, vertices and edges for a graph, a matrix pair, a number of points and
a number of queries. Do not force those into one column:

- one CSV column per measured dimension, listing them in the header in a fixed
  order, e.g. `n,m,algorithm,time_ms,check`;
- one ladder over the dimension the experiment actually varies, with the other
  dimensions held fixed and stated in the doc comment (or a ladder of pairs,
  e.g. `(n, m) = (1000, 10), (1000, 100), ...`);
- the driver keeps printing one row per (size combination, algorithm), so the
  reference check still works per row.

Keep the driver short enough to read in one sitting — that is what "keep bench
codes simple" buys.
