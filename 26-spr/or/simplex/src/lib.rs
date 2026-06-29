use std::ops::{Add, AddAssign, Div, DivAssign, Mul, MulAssign, Neg, Sub, SubAssign};

use num_rational::{BigRational, Rational32, Rational64};
use num_traits::{One, Zero};

#[cfg(feature = "web")]
pub mod wasm;

// ---------------------------------------------------------------------------
// Scalar trait
// ---------------------------------------------------------------------------

/// Numeric type the simplex solver can work with.
/// `eps()` returns the "zero threshold": 0 for exact types, 1e-9 for f64.
pub trait Scalar:
    Clone
    + PartialOrd
    + Zero
    + One
    + Neg<Output = Self>
    + Add<Output = Self>
    + Sub<Output = Self>
    + Mul<Output = Self>
    + Div<Output = Self>
    + AddAssign
    + SubAssign
    + MulAssign
    + DivAssign
    + std::fmt::Debug
{
    fn eps() -> Self;
}

impl Scalar for f64 {
    fn eps() -> Self {
        1e-9
    }
}
impl Scalar for Rational32 {
    fn eps() -> Self {
        Self::zero()
    }
}
impl Scalar for Rational64 {
    fn eps() -> Self {
        Self::zero()
    }
}
impl Scalar for BigRational {
    fn eps() -> Self {
        Self::zero()
    }
}

// ---------------------------------------------------------------------------
// AugMat
// ---------------------------------------------------------------------------

#[derive(Debug, Clone)]
pub struct AugMat<T = f64> {
    pub m: usize,
    pub n: usize,
    pub data: Vec<Vec<T>>,
    pub aug: Vec<T>,
}

impl<T: Scalar> AugMat<T> {
    pub fn new(m: usize, n: usize) -> Self {
        AugMat {
            m,
            n,
            data: vec![vec![T::zero(); n]; m],
            aug: vec![T::zero(); m],
        }
    }

    pub fn set_row(&mut self, i: usize, coeffs: &[T], rhs: T) {
        assert!(i < self.m);
        assert_eq!(coeffs.len(), self.n);
        self.data[i].clone_from_slice(coeffs);
        self.aug[i] = rhs;
    }

    pub fn get_row(&self, i: usize) -> (&[T], &T) {
        assert!(i < self.m);
        (&self.data[i], &self.aug[i])
    }

    fn two_rows(&mut self, i: usize, j: usize) -> (&mut [T], &[T]) {
        assert!(i != j);
        if i < j {
            let (a, b) = self.data.split_at_mut(j);
            (&mut a[i], &b[0])
        } else {
            let (a, b) = self.data.split_at_mut(i);
            (&mut b[0], &a[j])
        }
    }

    fn scale(&mut self, i: usize, scalar: T) {
        self.data[i].iter_mut().for_each(|x| *x *= scalar.clone());
        self.aug[i] *= scalar;
    }

    fn add_to(&mut self, i: usize, j: usize, scalar: T) {
        let (row_i, row_j) = self.two_rows(i, j);
        let row_j: Vec<T> = row_j.to_vec();
        row_i.iter_mut().zip(row_j.iter()).for_each(|(x, y)| {
            *x += y.clone() * scalar.clone();
        });
        let aug_j = self.aug[j].clone();
        self.aug[i] += aug_j * scalar;
    }

    fn add_to_vec(&self, vec: &mut [T], rhs: &mut T, j: usize, scalar: T) {
        vec.iter_mut().zip(self.data[j].iter()).for_each(|(x, y)| {
            *x += y.clone() * scalar.clone();
        });
        *rhs += self.aug[j].clone() * scalar;
    }
}

// ---------------------------------------------------------------------------
// Public result type
// ---------------------------------------------------------------------------

#[derive(Debug, Clone)]
pub enum LPResult<T = f64> {
    Optimal { ans: T, sln: Vec<T> },
    Infeasible,
    Unbounded,
}

// ---------------------------------------------------------------------------
// Step types (public API)
// ---------------------------------------------------------------------------

/// A snapshot of the simplex tableau at one point in time.
#[derive(Debug, Clone)]
pub struct SimplexTable<T = f64> {
    /// Constraint matrix rows (m × n).
    pub cons: Vec<Vec<T>>,
    /// Right-hand sides (m entries).
    pub aug: Vec<T>,
    /// Negated objective coefficients (n entries).
    pub obj: Vec<T>,
    /// Current objective value (accumulated RHS).
    pub obj_rhs: T,
    /// Basic variable index for each constraint row.
    pub bv: Vec<usize>,
}

/// One simplex iteration: the tableau *before* the pivot, and which pivot was chosen.
/// The last entry has `pivot == None` (no more pivots; this is the final tableau).
#[derive(Debug, Clone)]
pub struct SimplexStep<T = f64> {
    pub table: SimplexTable<T>,
    /// `(row, col)` of the pivot element, or `None` for the terminal snapshot.
    pub pivot: Option<(usize, usize)>,
}

/// A symbolic Big-M expression `m_coeff * M + constant`.
#[derive(Debug, Clone)]
pub struct BigMValue<T = f64> {
    pub m_coeff: T,
    pub constant: T,
}

/// A Big-M tableau snapshot with symbolic objective coefficients.
#[derive(Debug, Clone)]
pub struct BigMTable<T = f64> {
    pub cons: Vec<Vec<T>>,
    pub aug: Vec<T>,
    pub obj: Vec<BigMValue<T>>,
    pub obj_rhs: BigMValue<T>,
    pub bv: Vec<usize>,
}

/// One Big-M iteration. The tableau is captured before the indicated pivot.
#[derive(Debug, Clone)]
pub struct BigMStep<T = f64> {
    pub table: BigMTable<T>,
    pub pivot: Option<(usize, usize)>,
}

// ---------------------------------------------------------------------------
// Internal solver
// ---------------------------------------------------------------------------

#[derive(Debug, Clone)]
struct LPData<T> {
    n: usize,
    m: usize,
    cons: AugMat<T>,
    obj: Vec<T>,
    rhs: T,
    bv: Vec<usize>,
}

impl<T: Scalar> LPData<T> {
    fn new(n: usize, m: usize, cons: AugMat<T>, obj: Vec<T>, rhs: T, bv: Vec<usize>) -> Self {
        assert!(n >= 1 && m >= 1);
        assert_eq!(cons.m, m);
        assert_eq!(cons.n, n);
        assert_eq!(obj.len(), n);
        assert_eq!(bv.len(), m);
        LPData {
            n,
            m,
            cons,
            obj,
            rhs,
            bv,
        }
    }

    fn snapshot(&self) -> SimplexTable<T> {
        SimplexTable {
            cons: self.cons.data.clone(),
            aug: self.cons.aug.clone(),
            obj: self.obj.clone(),
            obj_rhs: self.rhs.clone(),
            bv: self.bv.clone(),
        }
    }

    fn pivot_col(&self) -> Option<usize> {
        let eps = T::eps();
        self.obj
            .iter()
            .enumerate()
            .min_by(|x, y| x.1.partial_cmp(y.1).unwrap())
            .filter(|x| *x.1 < -eps)
            .map(|x| x.0)
    }

    fn pivot_row(&self, col: usize) -> Option<usize> {
        let eps = T::eps();
        (0..self.m)
            .filter_map(|i| {
                if self.cons.data[i][col] > eps {
                    Some((i, self.cons.aug[i].clone() / self.cons.data[i][col].clone()))
                } else {
                    None
                }
            })
            .min_by(|x, y| x.1.partial_cmp(&y.1).unwrap())
            .map(|x| x.0)
    }

    fn change_basis(&mut self, row: usize, col: usize) {
        let pivot = self.cons.data[row][col].clone();
        let obj_col = self.obj[col].clone();

        self.bv[row] = col;
        self.cons.scale(row, T::one() / pivot);

        let scalars: Vec<T> = (0..self.m)
            .map(|i| -self.cons.data[i][col].clone())
            .collect();
        for (i, s) in scalars.into_iter().enumerate() {
            if i != row {
                self.cons.add_to(i, row, s);
            }
        }

        self.cons
            .add_to_vec(&mut self.obj, &mut self.rhs, row, -obj_col);
    }

    /// Run simplex; returns `None` if unbounded.
    fn run(&mut self) -> Option<()> {
        while let Some(col) = self.pivot_col() {
            self.change_basis(self.pivot_row(col)?, col);
        }
        Some(())
    }

    /// Run simplex collecting steps; returns `(unbounded, steps)`.
    fn run_with_steps(&mut self) -> (bool, Vec<SimplexStep<T>>) {
        let mut steps = Vec::new();
        loop {
            let col = match self.pivot_col() {
                Some(c) => c,
                None => {
                    steps.push(SimplexStep {
                        table: self.snapshot(),
                        pivot: None,
                    });
                    return (false, steps);
                }
            };
            let row = match self.pivot_row(col) {
                Some(r) => r,
                None => {
                    steps.push(SimplexStep {
                        table: self.snapshot(),
                        pivot: None,
                    });
                    return (true, steps);
                }
            };
            steps.push(SimplexStep {
                table: self.snapshot(),
                pivot: Some((row, col)),
            });
            self.change_basis(row, col);
        }
    }

    fn extract(&self, orig_n: usize) -> LPResult<T> {
        let mut sln = vec![T::zero(); self.n];
        self.bv.iter().enumerate().for_each(|(i, &j)| {
            sln[j] = self.cons.aug[i].clone();
        });
        sln.truncate(orig_n);
        LPResult::Optimal {
            ans: self.rhs.clone(),
            sln,
        }
    }
}

// ---------------------------------------------------------------------------
// Shared phase-2 setup (used by both two-phase and big-M after phase 1)
// ---------------------------------------------------------------------------

/// Build phase-1 tableau for two-phase method.
/// Returns (lp1, n_orig_plus_slack, arts_count).
fn build_phase1<T: Scalar>(n: usize, m: usize, cons: &AugMat<T>) -> (LPData<T>, usize) {
    let eps = T::eps();
    let arts: usize = cons.aug.iter().filter(|x| **x < -eps.clone()).count();
    let total = n + m + arts;
    let mut cons1: AugMat<T> = AugMat::new(m, total);
    let mut bv1 = vec![0usize; m];
    let mut art_idx = n + m;

    for (i, bv) in bv1.iter_mut().enumerate() {
        if cons.aug[i] < -eps.clone() {
            for j in 0..n {
                cons1.data[i][j] = -cons.data[i][j].clone();
            }
            cons1.aug[i] = -cons.aug[i].clone();
            cons1.data[i][n + i] = -T::one();
            cons1.data[i][art_idx] = T::one();
            *bv = art_idx;
            art_idx += 1;
        } else {
            for j in 0..n {
                cons1.data[i][j] = cons.data[i][j].clone();
            }
            cons1.aug[i] = cons.aug[i].clone();
            cons1.data[i][n + i] = T::one();
            *bv = n + i;
        }
    }

    let mut obj1: Vec<T> = std::iter::repeat_with(T::zero)
        .take(n + m)
        .chain(std::iter::repeat_with(T::one).take(arts))
        .collect();
    let mut rhs1 = T::zero();

    for i in 0..m {
        if cons.aug[i] < -eps.clone() {
            cons1.add_to_vec(&mut obj1, &mut rhs1, i, -T::one());
        }
    }

    (LPData::new(total, m, cons1, obj1, rhs1, bv1), n + m)
}

/// Transition phase-1 result into a phase-2 `LPData`.
fn phase1_to_phase2<T: Scalar>(
    lp1: LPData<T>,
    n: usize,
    m: usize,
    obj: Vec<T>,
) -> Option<LPData<T>> {
    let eps = T::eps();
    let nm = n + m;

    // Drive artificial variables out of the basis if possible.
    let mut lp1 = lp1;
    for i in 0..m {
        if lp1.bv[i] >= nm {
            if let Some(col) = (0..nm).find(|&j| {
                let v = &lp1.cons.data[i][j];
                *v > eps.clone() || *v < -eps.clone()
            }) {
                lp1.change_basis(i, col);
            }
        }
    }

    let mut cons2: AugMat<T> = AugMat::new(m, nm);
    for i in 0..m {
        cons2.data[i].clone_from_slice(&lp1.cons.data[i][..nm]);
        cons2.aug[i] = lp1.cons.aug[i].clone();
    }

    let mut obj2: Vec<T> = obj
        .iter()
        .map(|x| -x.clone())
        .chain(std::iter::repeat_with(T::zero).take(m))
        .collect();
    let mut rhs2 = T::zero();

    for i in 0..m {
        let col = lp1.bv[i];
        if col < nm {
            let x = -obj2[col].clone();
            lp1.cons.add_to_vec(&mut obj2, &mut rhs2, i, x);
        }
    }

    Some(LPData::new(nm, m, cons2, obj2, rhs2, lp1.bv))
}

// ---------------------------------------------------------------------------
// Two-phase method
// ---------------------------------------------------------------------------

pub fn solve_lp_two_phase<T: Scalar>(
    n: usize,
    m: usize,
    cons: AugMat<T>,
    obj: Vec<T>,
) -> LPResult<T> {
    let eps = T::eps();
    let (mut lp1, nm) = build_phase1(n, m, &cons);
    lp1.run().expect("Phase 1 should not be unbounded");
    if lp1.rhs < -eps {
        return LPResult::Infeasible;
    }
    let lp2 = match phase1_to_phase2(lp1, n, m, obj) {
        Some(lp) => lp,
        None => return LPResult::Infeasible,
    };
    let _ = nm;
    lp2.optimize(n)
}

pub fn solve_lp_two_phase_steps<T: Scalar>(
    n: usize,
    m: usize,
    cons: AugMat<T>,
    obj: Vec<T>,
) -> (LPResult<T>, Vec<SimplexStep<T>>) {
    let eps = T::eps();
    let (mut lp1, _nm) = build_phase1(n, m, &cons);
    let (unbounded, mut steps) = lp1.run_with_steps();
    assert!(!unbounded, "Phase 1 should not be unbounded");
    if lp1.rhs < -eps {
        return (LPResult::Infeasible, steps);
    }
    let lp2 = match phase1_to_phase2(lp1, n, m, obj) {
        Some(lp) => lp,
        None => return (LPResult::Infeasible, steps),
    };
    let (result, phase2_steps) = lp2.optimize_with_steps(n);
    steps.extend(phase2_steps);
    (result, steps)
}

// ---------------------------------------------------------------------------
// Big-M method
// ---------------------------------------------------------------------------

/// Build the big-M tableau (single phase).
fn build_big_m<T: Scalar>(n: usize, m: usize, cons: &AugMat<T>, obj: &[T], big_m: T) -> LPData<T> {
    let eps = T::eps();
    let arts: usize = cons.aug.iter().filter(|x| **x < -eps.clone()).count();
    // Variables: x[0..n], slacks[n..n+m], artificials[n+m..n+m+arts]
    let total = n + m + arts;
    let mut cons1: AugMat<T> = AugMat::new(m, total);
    let mut bv1 = vec![0usize; m];
    let mut art_idx = n + m;

    for (i, bv) in bv1.iter_mut().enumerate() {
        if cons.aug[i] < -eps.clone() {
            for j in 0..n {
                cons1.data[i][j] = -cons.data[i][j].clone();
            }
            cons1.aug[i] = -cons.aug[i].clone();
            cons1.data[i][n + i] = -T::one();
            cons1.data[i][art_idx] = T::one();
            *bv = art_idx;
            art_idx += 1;
        } else {
            for j in 0..n {
                cons1.data[i][j] = cons.data[i][j].clone();
            }
            cons1.aug[i] = cons.aug[i].clone();
            cons1.data[i][n + i] = T::one();
            *bv = n + i;
        }
    }

    // Objective: maximize obj^T x - M * sum(artificials)
    // In negated form: -obj[j] for j<n, 0 for slacks, +M for artificials
    let mut obj_neg: Vec<T> = obj.iter().map(|x| -x.clone()).collect();
    obj_neg.extend(std::iter::repeat_with(T::zero).take(m));
    obj_neg.extend(std::iter::repeat_with(|| big_m.clone()).take(arts));

    let mut rhs = T::zero();

    // Eliminate artificials from objective using current basis rows
    for i in 0..m {
        if bv1[i] >= n + m {
            let factor = -obj_neg[bv1[i]].clone();
            cons1.add_to_vec(&mut obj_neg, &mut rhs, i, factor);
        }
    }

    LPData::new(total, m, cons1, obj_neg, rhs, bv1)
}

impl<T: Scalar> BigMValue<T> {
    /// Compare expressions by their value as M tends to positive infinity.
    fn cmp_at_infinity(&self, other: &Self) -> std::cmp::Ordering {
        self.m_coeff
            .partial_cmp(&other.m_coeff)
            .unwrap()
            .then_with(|| self.constant.partial_cmp(&other.constant).unwrap())
    }

    fn is_negative(&self) -> bool {
        self.m_coeff < T::zero() || (self.m_coeff == T::zero() && self.constant < -T::eps())
    }

    fn add_scaled_row(&mut self, value: &T, scalar: &BigMValue<T>) {
        self.m_coeff += value.clone() * scalar.m_coeff.clone();
        self.constant += value.clone() * scalar.constant.clone();
    }
}

struct BigMLPData<T> {
    n: usize,
    m: usize,
    cons: AugMat<T>,
    obj: Vec<BigMValue<T>>,
    rhs: BigMValue<T>,
    bv: Vec<usize>,
}

impl<T: Scalar> BigMLPData<T> {
    fn snapshot(&self) -> BigMTable<T> {
        BigMTable {
            cons: self.cons.data.clone(),
            aug: self.cons.aug.clone(),
            obj: self.obj.clone(),
            obj_rhs: self.rhs.clone(),
            bv: self.bv.clone(),
        }
    }

    fn pivot_col(&self) -> Option<usize> {
        self.obj
            .iter()
            .enumerate()
            .min_by(|(_, left), (_, right)| left.cmp_at_infinity(right))
            .filter(|(_, value)| value.is_negative())
            .map(|(index, _)| index)
    }

    fn pivot_row(&self, col: usize) -> Option<usize> {
        let eps = T::eps();
        (0..self.m)
            .filter_map(|i| {
                if self.cons.data[i][col] > eps {
                    Some((i, self.cons.aug[i].clone() / self.cons.data[i][col].clone()))
                } else {
                    None
                }
            })
            .min_by(|(_, left), (_, right)| left.partial_cmp(right).unwrap())
            .map(|(index, _)| index)
    }

    fn change_basis(&mut self, row: usize, col: usize) {
        let pivot = self.cons.data[row][col].clone();
        let obj_col = self.obj[col].clone();
        self.bv[row] = col;
        self.cons.scale(row, T::one() / pivot);

        let scalars: Vec<T> = (0..self.m)
            .map(|i| -self.cons.data[i][col].clone())
            .collect();
        for (i, scalar) in scalars.into_iter().enumerate() {
            if i != row {
                self.cons.add_to(i, row, scalar);
            }
        }

        let factor = BigMValue {
            m_coeff: -obj_col.m_coeff,
            constant: -obj_col.constant,
        };
        for (value, row_value) in self.obj.iter_mut().zip(self.cons.data[row].iter()) {
            value.add_scaled_row(row_value, &factor);
        }
        self.rhs.add_scaled_row(&self.cons.aug[row], &factor);
    }

    fn run_with_steps(&mut self) -> (bool, Vec<BigMStep<T>>) {
        let mut steps = Vec::new();
        loop {
            let col = match self.pivot_col() {
                Some(col) => col,
                None => {
                    steps.push(BigMStep {
                        table: self.snapshot(),
                        pivot: None,
                    });
                    return (false, steps);
                }
            };
            let row = match self.pivot_row(col) {
                Some(row) => row,
                None => {
                    steps.push(BigMStep {
                        table: self.snapshot(),
                        pivot: None,
                    });
                    return (true, steps);
                }
            };
            steps.push(BigMStep {
                table: self.snapshot(),
                pivot: Some((row, col)),
            });
            self.change_basis(row, col);
        }
    }
}

fn build_symbolic_big_m<T: Scalar>(
    n: usize,
    m: usize,
    cons: &AugMat<T>,
    obj: &[T],
) -> BigMLPData<T> {
    let initial = build_big_m(n, m, cons, obj, T::zero());
    let mut m_coeff = vec![T::zero(); initial.n];
    let mut rhs_m = T::zero();

    for i in 0..m {
        if initial.bv[i] >= n + m {
            m_coeff[initial.bv[i]] = T::one();
            for j in 0..initial.n {
                m_coeff[j] -= initial.cons.data[i][j].clone();
            }
            rhs_m -= initial.cons.aug[i].clone();
        }
    }

    BigMLPData {
        n: initial.n,
        m: initial.m,
        cons: initial.cons,
        obj: initial
            .obj
            .into_iter()
            .zip(m_coeff)
            .map(|(constant, m_coeff)| BigMValue { m_coeff, constant })
            .collect(),
        rhs: BigMValue {
            m_coeff: rhs_m,
            constant: initial.rhs,
        },
        bv: initial.bv,
    }
}

/// Solve with symbolic Big-M tableau snapshots. Each objective cell is stored
/// as `m_coeff * M + constant`, rather than evaluating M numerically.
pub fn solve_lp_big_m_symbolic_steps<T: Scalar>(
    n: usize,
    m: usize,
    cons: AugMat<T>,
    obj: Vec<T>,
) -> (LPResult<T>, Vec<BigMStep<T>>) {
    let eps = T::eps();
    let mut lp = build_symbolic_big_m(n, m, &cons, &obj);
    let (unbounded, steps) = lp.run_with_steps();
    if unbounded {
        return (LPResult::Unbounded, steps);
    }
    if lp
        .bv
        .iter()
        .enumerate()
        .any(|(i, &j)| j >= n + m && lp.cons.aug[i] > eps.clone())
    {
        return (LPResult::Infeasible, steps);
    }

    let mut sln = vec![T::zero(); lp.n];
    for (i, &j) in lp.bv.iter().enumerate() {
        sln[j] = lp.cons.aug[i].clone();
    }
    sln.truncate(n);
    (
        LPResult::Optimal {
            ans: lp.rhs.constant,
            sln,
        },
        steps,
    )
}

pub fn solve_lp_big_m<T: Scalar>(n: usize, m: usize, cons: AugMat<T>, obj: Vec<T>) -> LPResult<T> {
    solve_lp_big_m_symbolic_steps(n, m, cons, obj).0
}

pub fn solve_lp_big_m_steps<T: Scalar>(
    n: usize,
    m: usize,
    cons: AugMat<T>,
    obj: Vec<T>,
) -> (LPResult<T>, Vec<BigMStep<T>>) {
    solve_lp_big_m_symbolic_steps(n, m, cons, obj)
}

// ---------------------------------------------------------------------------
// LPData helpers for phase-2 optimize and truncated extract
// ---------------------------------------------------------------------------

impl<T: Scalar> LPData<T> {
    fn optimize(mut self, orig_n: usize) -> LPResult<T> {
        match self.run() {
            Some(()) => self.extract(orig_n),
            None => LPResult::Unbounded,
        }
    }

    fn optimize_with_steps(mut self, orig_n: usize) -> (LPResult<T>, Vec<SimplexStep<T>>) {
        let (unbounded, steps) = self.run_with_steps();
        let result = if unbounded {
            LPResult::Unbounded
        } else {
            self.extract(orig_n)
        };
        (result, steps)
    }
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

#[cfg(test)]
mod tests {
    use super::*;

    fn assert_lp_result_f64(
        n: usize,
        m: usize,
        cons: AugMat<f64>,
        obj: Vec<f64>,
        expected: LPResult<f64>,
    ) {
        let result = solve_lp_two_phase(n, m, cons, obj);
        let eps = 1e-6f64;
        match (result, expected) {
            (LPResult::Optimal { ans: a1, sln: s1 }, LPResult::Optimal { ans: a2, sln: s2 }) => {
                assert!((a1 - a2).abs() < eps, "ans mismatch: {} vs {}", a1, a2);
                for (x, y) in s1.iter().zip(s2.iter()) {
                    assert!((x - y).abs() < eps, "sln mismatch: {} vs {}", x, y);
                }
            }
            (LPResult::Unbounded, LPResult::Unbounded) => {}
            (LPResult::Infeasible, LPResult::Infeasible) => {}
            (r, e) => panic!("Expected {:?}, got {:?}", e, r),
        }
    }

    #[test]
    fn test_two_phase_f64() {
        let mut cons = AugMat::new(2, 2);
        cons.set_row(0, &[2.0f64, 1.0], 6.0);
        cons.set_row(1, &[-1.0, 2.0], 3.0);
        assert_lp_result_f64(
            2,
            2,
            cons,
            vec![1.0, 1.0],
            LPResult::Optimal {
                ans: 4.2,
                sln: vec![1.8, 2.4],
            },
        );
    }

    #[test]
    fn test_infeasible_f64() {
        let mut cons = AugMat::new(3, 3);
        cons.set_row(0, &[0.0f64, 0.0, 1.0], -4.0);
        cons.set_row(1, &[-2.0, 1.0, 0.0], 4.0);
        cons.set_row(2, &[1.0, 1.0, 0.0], -4.0);
        assert_lp_result_f64(3, 3, cons, vec![1.0, -2.0, 0.0], LPResult::Infeasible);
    }

    #[test]
    fn test_unbounded_f64() {
        let mut cons = AugMat::new(1, 2);
        cons.set_row(0, &[1.0f64, 0.0], 1.0);
        assert_lp_result_f64(2, 1, cons, vec![0.0, 1.0], LPResult::Unbounded);
    }

    #[test]
    fn test_big_m_f64() {
        let mut cons = AugMat::new(2, 2);
        cons.set_row(0, &[2.0f64, 1.0], 6.0);
        cons.set_row(1, &[-1.0, 2.0], 3.0);
        let result = solve_lp_big_m(2, 2, cons, vec![1.0f64, 1.0]);
        let eps = 1e-3f64;
        if let LPResult::Optimal { ans, .. } = result {
            assert!((ans - 4.2).abs() < eps, "ans: {}", ans);
        } else {
            panic!("Expected Optimal");
        }
    }

    #[test]
    fn test_big_m_steps_are_symbolic() {
        let mut cons = AugMat::new(1, 1);
        // x >= 1, represented as -x <= -1, requires an artificial variable.
        cons.set_row(0, &[-1.0f64], -1.0);
        let (_, steps) = solve_lp_big_m_steps(1, 1, cons, vec![1.0]);
        let table = &steps[0].table;
        assert_eq!(table.obj[0].m_coeff, -1.0);
        assert_eq!(table.obj[0].constant, -1.0);
        assert_eq!(table.obj_rhs.m_coeff, -1.0);
        assert_eq!(table.obj_rhs.constant, 0.0);
    }

    #[test]
    fn test_rational() {
        use num_rational::Rational64;
        let r = |n: i64, d: i64| Rational64::new(n, d);
        let mut cons: AugMat<Rational64> = AugMat::new(2, 2);
        cons.set_row(0, &[r(2, 1), r(1, 1)], r(6, 1));
        cons.set_row(1, &[r(-1, 1), r(2, 1)], r(3, 1));
        let result = solve_lp_two_phase(2, 2, cons, vec![r(1, 1), r(1, 1)]);
        if let LPResult::Optimal { ans, sln } = result {
            assert_eq!(ans, r(21, 5));
            assert_eq!(sln[0], r(9, 5));
            assert_eq!(sln[1], r(12, 5));
        } else {
            panic!("Expected Optimal");
        }
    }

    #[test]
    fn test_steps_returned() {
        let mut cons = AugMat::new(2, 2);
        cons.set_row(0, &[2.0f64, 1.0], 6.0);
        cons.set_row(1, &[-1.0, 2.0], 3.0);
        let (result, steps) = solve_lp_two_phase_steps(2, 2, cons, vec![1.0f64, 1.0]);
        assert!(matches!(result, LPResult::Optimal { .. }));
        assert!(!steps.is_empty());
        // Last step has no pivot (terminal snapshot)
        assert!(steps.last().unwrap().pivot.is_none());
    }
}
