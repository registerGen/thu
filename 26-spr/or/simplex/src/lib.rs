const EPS: f64 = 1e-9;

#[derive(Debug, Clone)]
pub struct AugMat {
    m: usize,
    n: usize,
    data: Vec<Vec<f64>>,
    aug: Vec<f64>,
}

impl AugMat {
    pub fn new(m: usize, n: usize) -> Self {
        AugMat {
            m,
            n,
            data: vec![vec![0.0; n]; m],
            aug: vec![0.0; m],
        }
    }

    pub fn set_row(&mut self, i: usize, coeffs: &[f64], rhs: f64) {
        assert!(i < self.m);
        assert_eq!(coeffs.len(), self.n);
        self.data[i].copy_from_slice(coeffs);
        self.aug[i] = rhs;
    }

    pub fn get_row(&self, i: usize) -> (&[f64], f64) {
        assert!(i < self.m);
        (&self.data[i], self.aug[i])
    }

    fn two_rows(&mut self, i: usize, j: usize) -> (&mut [f64], &[f64]) {
        assert!(i != j);
        if i < j {
            let (a, b) = self.data.split_at_mut(j);
            (&mut a[i], &b[0])
        } else {
            let (a, b) = self.data.split_at_mut(i);
            (&mut b[0], &a[j])
        }
    }

    fn scale(&mut self, i: usize, scalar: f64) {
        self.data[i].iter_mut().for_each(|x| *x *= scalar);
        self.aug[i] *= scalar;
    }

    fn add_to(&mut self, i: usize, j: usize, scalar: f64) {
        let (row_i, row_j) = self.two_rows(i, j);
        row_i
            .iter_mut()
            .zip(row_j.iter())
            .for_each(|(x, y)| *x += y * scalar);
        self.aug[i] += self.aug[j] * scalar;
    }

    fn add_to_vec(&self, vec: &mut [f64], rhs: &mut f64, j: usize, scalar: f64) {
        vec.iter_mut()
            .zip(self.data[j].iter())
            .for_each(|(x, y)| *x += y * scalar);
        *rhs += self.aug[j] * scalar;
    }
}

#[derive(Debug, Clone)]
pub enum LPResult {
    Optimal { ans: f64, sln: Vec<f64> },
    Infeasible,
    Unbounded,
}

#[derive(Debug, Clone)]
struct LPData {
    /// Number of variables, including slacks.
    n: usize,
    /// Number of constraints.
    m: usize,
    /// Constraints coefficients and right-hand sides.
    cons: AugMat,
    /// Negated objective coefficients.
    obj: Vec<f64>,
    /// Right-hand side of the objective function.
    rhs: f64,
    /// bv[i] is the index of the basic variable for the i-th constraint.
    bv: Vec<usize>,
}

impl LPData {
    fn new(n: usize, m: usize, cons: AugMat, obj: Vec<f64>, rhs: f64, bv: Vec<usize>) -> Self {
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

    fn pivot_col(&self) -> Option<usize> {
        Some(
            self.obj
                .iter()
                .enumerate()
                .min_by(|x, y| x.1.partial_cmp(y.1).unwrap())
                .filter(|x| *x.1 < -EPS)?
                .0,
        )
    }

    fn pivot_row(&self, col: usize) -> Option<usize> {
        Some(
            (0..self.m)
                .filter_map(|i| {
                    if self.cons.data[i][col] > EPS {
                        Some((i, self.cons.aug[i] / self.cons.data[i][col]))
                    } else {
                        None
                    }
                })
                .min_by(|x, y| x.1.partial_cmp(&y.1).unwrap())?
                .0,
        )
    }

    fn change_basis(&mut self, row: usize, col: usize) {
        let pivot = self.cons.data[row][col];
        let obj_col = self.obj[col];

        self.bv[row] = col;

        self.cons.scale(row, 1.0 / pivot);

        let scalars = (0..self.m)
            .map(|i| -self.cons.data[i][col])
            .collect::<Vec<_>>();
        scalars.into_iter().enumerate().for_each(|(i, x)| {
            if i != row {
                self.cons.add_to(i, row, x)
            }
        });

        self.cons
            .add_to_vec(&mut self.obj, &mut self.rhs, row, -obj_col);
    }

    fn run(&mut self) -> Option<()> {
        while let Some(col) = self.pivot_col() {
            self.change_basis(self.pivot_row(col)?, col);
        }
        Some(())
    }

    fn extract(&self) -> LPResult {
        let mut sln = vec![0.0; self.n];
        self.bv.iter().enumerate().for_each(|(i, &j)| {
            sln[j] = self.cons.aug[i];
        });
        LPResult::Optimal { ans: self.rhs, sln }
    }

    fn optimize(mut self) -> LPResult {
        match self.run() {
            Some(()) => self.extract(),
            None => LPResult::Unbounded,
        }
    }
}

/// Solves the linear program:
///   maximize obj^T x
///   subject to cons.data[i]^T x <= cons.aug[i] for all i.
/// where n is the number of variables, m is the number of constraints.
pub fn solve_lp(n: usize, m: usize, cons: AugMat, obj: Vec<f64>) -> LPResult {
    let arts = cons.aug.iter().filter(|&&x| x < -EPS).count();
    let mut cons1 = AugMat::new(m, n + m + arts);
    let mut bv1 = vec![0usize; m];
    let mut art_idx = n + m;

    for (i, bv) in bv1.iter_mut().enumerate() {
        if cons.aug[i] < -EPS {
            (0..n).for_each(|j| cons1.data[i][j] = -cons.data[i][j]);
            cons1.aug[i] = -cons.aug[i];
            cons1.data[i][n + i] = -1.0;
            cons1.data[i][art_idx] = 1.0;
            *bv = art_idx;
            art_idx += 1;
        } else {
            (0..n).for_each(|j| cons1.data[i][j] = cons.data[i][j]);
            cons1.aug[i] = cons.aug[i];
            cons1.data[i][n + i] = 1.0;
            *bv = n + i;
        }
    }

    let mut obj1 = std::iter::repeat_n(0.0, n + m)
        .chain(std::iter::repeat_n(1.0, arts))
        .collect::<Vec<_>>();
    let mut rhs1 = 0.0;

    for i in 0..m {
        if cons.aug[i] < -EPS {
            cons1.add_to_vec(&mut obj1, &mut rhs1, i, -1.0);
        }
    }

    let mut lp1 = LPData::new(n + m + arts, m, cons1, obj1, rhs1, bv1);
    lp1.run().expect("Phase 1 should not be unbounded");
    if lp1.rhs < -EPS {
        return LPResult::Infeasible;
    }

    for i in 0..m {
        if lp1.bv[i] >= n + m {
            if let Some(col) = (0..n + m).find(|&j| lp1.cons.data[i][j].abs() > EPS) {
                lp1.change_basis(i, col);
            } else {
                // This constraint is redundant, we can ignore it.
            }
        }
    }

    let mut cons2 = AugMat::new(m, n + m);
    for i in 0..m {
        cons2.data[i].copy_from_slice(&lp1.cons.data[i][..n + m]);
        cons2.aug[i] = lp1.cons.aug[i];
    }

    let mut obj2 = obj
        .iter()
        .map(|x| -*x)
        .chain(std::iter::repeat_n(0.0, m))
        .collect::<Vec<_>>();
    let mut rhs2 = 0.0;

    for i in 0..m {
        let col = lp1.bv[i];
        if col < n + m {
            let x = -obj2[col];
            lp1.cons.add_to_vec(&mut obj2, &mut rhs2, i, x);
        }
    }

    let lp2 = LPData::new(n + m, m, cons2, obj2, rhs2, lp1.bv);

    let mut result = lp2.optimize();
    if let LPResult::Optimal { sln, .. } = &mut result {
        sln.truncate(n);
    }
    result
}

#[cfg(test)]
mod tests {
    use super::*;

    fn assert_lp_result(n: usize, m: usize, cons: AugMat, obj: Vec<f64>, expected: LPResult) {
        let result = solve_lp(n, m, cons, obj);
        match (result.clone(), expected.clone()) {
            (
                LPResult::Optimal {
                    ans: ans1,
                    sln: sln1,
                },
                LPResult::Optimal {
                    ans: ans2,
                    sln: sln2,
                },
            ) => {
                assert!((ans1 - ans2).abs() < EPS);
                assert_eq!(sln1.len(), sln2.len());
                for (x1, x2) in sln1.iter().zip(sln2.iter()) {
                    assert!((x1 - x2).abs() < EPS);
                }
            }
            (LPResult::Unbounded, LPResult::Unbounded) => {}
            (LPResult::Infeasible, LPResult::Infeasible) => {}
            _ => panic!("Expected {:?}, got {:?}", expected, result),
        }
    }

    #[test]
    fn test_lp() {
        assert_lp_result(
            2,
            2,
            AugMat {
                m: 2,
                n: 2,
                data: vec![vec![2.0, 1.0], vec![-1.0, 2.0]],
                aug: vec![6.0, 3.0],
            },
            vec![1.0, 1.0],
            LPResult::Optimal {
                ans: 4.2,
                sln: vec![1.8, 2.4],
            },
        );

        assert_lp_result(
            2,
            2,
            AugMat {
                m: 2,
                n: 2,
                data: vec![vec![1.0, 1.0], vec![-1.0, -2.0]],
                aug: vec![4.0, -2.0],
            },
            vec![1.0, -1.0],
            LPResult::Optimal {
                ans: 4.0,
                sln: vec![4.0, 0.0],
            },
        );

        assert_lp_result(
            3,
            3,
            AugMat {
                m: 3,
                n: 3,
                data: vec![
                    vec![0.0, 0.0, 1.0],
                    vec![-2.0, 1.0, 0.0],
                    vec![1.0, 1.0, 0.0],
                ],
                aug: vec![-4.0, 4.0, -4.0],
            },
            vec![1.0, -2.0, 0.0],
            LPResult::Infeasible,
        );

        assert_lp_result(
            2,
            1,
            AugMat {
                m: 1,
                n: 2,
                data: vec![vec![1.0, 0.0]],
                aug: vec![1.0],
            },
            vec![0.0, 1.0],
            LPResult::Unbounded,
        );
    }
}
