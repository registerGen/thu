//! Random input for the tests and the benchmark, without external crates.

/// One measured input size. The scaffold sets this from `--size-arity`; widen
/// it when the task measures more than one number, e.g.
/// `pub type Size = (usize, usize);` for (points, queries).
pub type Size = {{SIZE_TYPE}};

/// One generated input: whatever the task's algorithms consume. Widen it when
/// the operation takes several arguments, e.g.
/// `pub type Input = (Vec<Vec<i32>>, Vec<Vec<i32>>);` for two operands.
pub type Input = {{INPUT_TYPE}};

/// Seed used by [`generate`].
pub const DEFAULT_SEED: u64 = 0x9e37_79b9_7f4a_7c15;

/// A small xorshift64* pseudo random number generator. It is deterministic, so
/// the same seed always produces the same input.
pub struct Rng {
    state: u64,
}

impl Rng {
    /// Creates a generator. A zero seed would be stuck at zero, so it is
    /// replaced by [`DEFAULT_SEED`].
    pub fn new(seed: u64) -> Self {
        Self {
            state: if seed == 0 { DEFAULT_SEED } else { seed },
        }
    }

    /// Returns the next 64 random bits.
    pub fn next_u64(&mut self) -> u64 {
        let mut x = self.state;
        x ^= x >> 12;
        x ^= x << 25;
        x ^= x >> 27;
        self.state = x;
        x.wrapping_mul(0x2545_f491_4f6c_dd1d)
    }

    /// Returns a uniform random `f64` in `[0, 1)`.
    pub fn next_f64(&mut self) -> f64 {
        (self.next_u64() >> 11) as f64 * (1.0 / (1u64 << 53) as f64)
    }

    /// Returns a uniform random integer in `lo..=hi`.
    pub fn next_range(&mut self, lo: i64, hi: i64) -> i64 {
        debug_assert!(lo <= hi);
        lo + (self.next_u64() % (hi - lo + 1) as u64) as i64
    }
}

/// Generates one input of the given size with [`DEFAULT_SEED`].
pub fn generate(size: Size) -> Input {
    generate_with_seed(size, DEFAULT_SEED)
}

/// Generates one input of the given size with the given seed.
pub fn generate_with_seed(size: Size, seed: u64) -> Input {
    let mut rng = Rng::new(seed);
    // TODO(algo-hw): build the task's random input from `size`. Examples for
    // `Input = Vec<Item>`:
    //   uniform 2D points: (0..size).map(|_| (rng.next_f64(), rng.next_f64())).collect()
    //   lattice values:    rng.next_range(-bound, bound)
    //   matrix entries small enough that the sums stay far from overflow.
    // For two operands return a tuple, e.g. `(a, b)`.
    let _ = (size, &mut rng);
    todo!("generate the task's random input")
}
