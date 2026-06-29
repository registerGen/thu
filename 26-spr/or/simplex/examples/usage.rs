use simplex::{AugMat, LPResult, solve_lp};

fn main() {
    println!("=== LP Solver Public API Usage ===\n");

    // Example 1: Simple optimal solution
    println!("Example 1: Maximize x + y subject to 2x + y ≤ 6 and -x + 2y ≤ 3");
    let mut cons = AugMat::new(2, 2);
    cons.set_row(0, &[2.0, 1.0], 6.0);
    cons.set_row(1, &[-1.0, 2.0], 3.0);
    let obj = vec![1.0, 1.0];

    match solve_lp(2, 2, cons, obj) {
        LPResult::Optimal { ans, sln } => {
            println!("✓ Optimal solution found!");
            println!("  Objective value: {}", ans);
            println!("  Solution: x = {}, y = {}\n", sln[0], sln[1]);
        }
        LPResult::Infeasible => println!("✗ Problem is infeasible\n"),
        LPResult::Unbounded => println!("✗ Problem is unbounded\n"),
    }

    // Example 2: Unbounded problem
    println!("Example 2: Unbounded problem - maximize y subject to x ≥ 1");
    let mut cons = AugMat::new(1, 2);
    cons.set_row(0, &[1.0, 0.0], 1.0);
    let obj = vec![0.0, 1.0];

    match solve_lp(2, 1, cons, obj) {
        LPResult::Optimal { ans, sln } => {
            println!("✓ Optimal: {}, Solution: {:?}\n", ans, sln);
        }
        LPResult::Infeasible => println!("✗ Problem is infeasible\n"),
        LPResult::Unbounded => println!("✓ Correctly detected as unbounded!\n"),
    }

    // Example 3: Infeasible problem
    println!("Example 3: Infeasible problem - x ≥ 1 and x ≤ 0");
    let mut cons = AugMat::new(2, 1);
    cons.set_row(0, &[1.0], 1.0);
    cons.set_row(1, &[-1.0], 0.0);
    let obj = vec![1.0];

    match solve_lp(1, 2, cons, obj) {
        LPResult::Optimal { ans, sln } => {
            println!("✓ Optimal: {}, Solution: {:?}\n", ans, sln);
        }
        LPResult::Infeasible => println!("✓ Correctly detected as infeasible!\n"),
        LPResult::Unbounded => println!("✗ Problem is unbounded\n"),
    }

    println!("=== All examples completed successfully ===");
}
