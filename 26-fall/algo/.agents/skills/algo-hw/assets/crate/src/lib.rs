//! {{TITLE}}
//!
//! An algorithm is a unit struct which implements the Solution trait. See
//! solution.rs for details. The Naive struct in naive.rs is the reference
//! every other algorithm is checked against.
//!
//! TODO(algo-hw): list the algorithms here once they exist.

mod algorithm;
pub mod generator;
mod solution;

pub use algorithm::{{ALGORITHM}};
pub use solution::Solution;

#[cfg(test)]
mod tests {
    use super::*;
    // The PRNG and the random input come from generator.rs; never duplicate
    // them here.
    use crate::generator::Rng;

    /// Placeholder until the real tests land: a hand-computed case with a
    /// known answer, a boundary case (n = 1, duplicates, collinear, ...), and
    /// randomized agreement of every algorithm with the naive reference, on
    /// inputs from `crate::generator::generate_with_seed`. See
    /// references/rust-conventions.md.
    #[test]
    #[allow(clippy::type_complexity)] // the signature is the task's, not ours
    fn signature_compiles() {
        let _: fn({{PARAM_TYPES}}) -> {{OUT}} = {{ALGORITHM}}::{{OP}};
        assert_eq!(Rng::new(7).next_u64(), Rng::new(7).next_u64());
    }
}
