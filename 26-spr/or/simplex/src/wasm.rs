use crate::{
    AugMat, BigMStep, BigMValue, LPResult, SimplexStep, solve_lp_big_m_steps,
    solve_lp_two_phase_steps,
};

use num_rational::BigRational;
use num_traits::Zero;
use serde::Serialize;
use wasm_bindgen::prelude::*;

// ---------------------------------------------------------------------------
// Shared JSON types
// ---------------------------------------------------------------------------

#[derive(Serialize)]
struct TableJson {
    cons: Vec<Vec<String>>,
    aug: Vec<String>,
    obj: Vec<String>,
    obj_rhs: String,
    bv: Vec<usize>,
}

#[derive(Serialize)]
struct StepJson {
    table: TableJson,
    pivot: Option<(usize, usize)>,
}

#[derive(Serialize)]
struct ResultJson {
    result: &'static str,
    #[serde(skip_serializing_if = "Option::is_none")]
    ans: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    sln: Option<Vec<String>>,
    steps: Vec<StepJson>,
}

fn encode<T>(lp_result: LPResult<T>, steps: Vec<SimplexStep<T>>, fmt: fn(&T) -> String) -> String {
    let step_jsons: Vec<StepJson> = steps
        .into_iter()
        .map(|s| StepJson {
            table: TableJson {
                cons: s
                    .table
                    .cons
                    .iter()
                    .map(|r| r.iter().map(fmt).collect())
                    .collect(),
                aug: s.table.aug.iter().map(fmt).collect(),
                obj: s.table.obj.iter().map(fmt).collect(),
                obj_rhs: fmt(&s.table.obj_rhs),
                bv: s.table.bv,
            },
            pivot: s.pivot,
        })
        .collect();
    let out = match lp_result {
        LPResult::Optimal { ans, sln } => ResultJson {
            result: "Optimal",
            ans: Some(fmt(&ans)),
            sln: Some(sln.iter().map(fmt).collect()),
            steps: step_jsons,
        },
        LPResult::Infeasible => ResultJson {
            result: "Infeasible",
            ans: None,
            sln: None,
            steps: step_jsons,
        },
        LPResult::Unbounded => ResultJson {
            result: "Unbounded",
            ans: None,
            sln: None,
            steps: step_jsons,
        },
    };
    serde_json::to_string(&out).unwrap()
}

// ---------------------------------------------------------------------------
// Formatters
// ---------------------------------------------------------------------------

fn fmt_f64(v: &f64) -> String {
    if v.abs() < 1e-10 {
        return "0".into();
    }
    let r = (v * 1_000_000.0).round() / 1_000_000.0;
    r.to_string()
}

fn fmt_rat(v: &BigRational) -> String {
    if v.is_zero() {
        return "0".into();
    }
    if *v.denom() == 1u32.into() {
        v.numer().to_string()
    } else {
        format!("{}/{}", v.numer(), v.denom())
    }
}

fn fmt_big_m<T>(value: &BigMValue<T>, fmt: fn(&T) -> String) -> String
where
    T: Zero + PartialEq + PartialOrd,
{
    let coefficient_is_zero = value.m_coeff.is_zero();
    let constant_is_zero = value.constant.is_zero();
    match (coefficient_is_zero, constant_is_zero) {
        (true, _) => fmt(&value.constant),
        (false, true) => format!("{}M", fmt(&value.m_coeff)),
        (false, false) if value.constant < T::zero() => {
            format!("{}M{}", fmt(&value.m_coeff), fmt(&value.constant))
        }
        (false, false) => format!("{}M+{}", fmt(&value.m_coeff), fmt(&value.constant)),
    }
}

fn encode_big_m<T>(lp_result: LPResult<T>, steps: Vec<BigMStep<T>>, fmt: fn(&T) -> String) -> String
where
    T: Zero + PartialEq + PartialOrd,
{
    let step_jsons: Vec<StepJson> = steps
        .into_iter()
        .map(|s| StepJson {
            table: TableJson {
                cons: s
                    .table
                    .cons
                    .iter()
                    .map(|r| r.iter().map(fmt).collect())
                    .collect(),
                aug: s.table.aug.iter().map(fmt).collect(),
                obj: s.table.obj.iter().map(|v| fmt_big_m(v, fmt)).collect(),
                obj_rhs: fmt_big_m(&s.table.obj_rhs, fmt),
                bv: s.table.bv,
            },
            pivot: s.pivot,
        })
        .collect();
    let out = match lp_result {
        LPResult::Optimal { ans, sln } => ResultJson {
            result: "Optimal",
            ans: Some(fmt(&ans)),
            sln: Some(sln.iter().map(fmt).collect()),
            steps: step_jsons,
        },
        LPResult::Infeasible => ResultJson {
            result: "Infeasible",
            ans: None,
            sln: None,
            steps: step_jsons,
        },
        LPResult::Unbounded => ResultJson {
            result: "Unbounded",
            ans: None,
            sln: None,
            steps: step_jsons,
        },
    };
    serde_json::to_string(&out).unwrap()
}

// ---------------------------------------------------------------------------
// Input parsers
// ---------------------------------------------------------------------------

fn parse_f64(s: &str) -> Result<(usize, usize, AugMat<f64>, Vec<f64>), String> {
    let mut it = s.split_whitespace();
    let n: usize = it
        .next()
        .ok_or_else(|| "missing n".to_string())?
        .parse()
        .map_err(|_| "bad n")?;
    let m: usize = it
        .next()
        .ok_or_else(|| "missing m".to_string())?
        .parse()
        .map_err(|_| "bad m")?;
    let mut tok = || {
        it.next()
            .ok_or_else(|| "unexpected end of input".to_string())
            .and_then(|t| t.parse::<f64>().map_err(|e| e.to_string()))
    };
    let obj: Vec<f64> = (0..n).map(|_| tok()).collect::<Result<_, _>>()?;
    let mut cons: AugMat<f64> = AugMat::new(m, n);
    for i in 0..m {
        let row: Vec<f64> = (0..n).map(|_| tok()).collect::<Result<_, _>>()?;
        cons.set_row(i, &row, tok()?);
    }
    if it.next().is_some() {
        return Err("unexpected trailing input".into());
    }
    Ok((n, m, cons, obj))
}

fn parse_rat_token(t: &str) -> Result<BigRational, String> {
    if let Some((n, d)) = t.split_once('/') {
        let num: num_bigint::BigInt = n.trim().parse().map_err(|_| format!("bad number: {t}"))?;
        let den: num_bigint::BigInt = d.trim().parse().map_err(|_| format!("bad number: {t}"))?;
        if den.is_zero() {
            return Err("zero denominator".into());
        }
        Ok(BigRational::new(num, den))
    } else {
        Ok(BigRational::from(
            t.parse::<num_bigint::BigInt>()
                .map_err(|_| format!("bad number: {t}"))?,
        ))
    }
}

fn parse_rational(
    s: &str,
) -> Result<(usize, usize, AugMat<BigRational>, Vec<BigRational>), String> {
    let mut it = s.split_whitespace();
    let n: usize = it
        .next()
        .ok_or_else(|| "missing n".to_string())?
        .parse()
        .map_err(|_| "bad n")?;
    let m: usize = it
        .next()
        .ok_or_else(|| "missing m".to_string())?
        .parse()
        .map_err(|_| "bad m")?;
    let mut tok = || {
        it.next()
            .ok_or_else(|| "unexpected end of input".to_string())
            .and_then(|t| parse_rat_token(t))
    };
    let obj: Vec<BigRational> = (0..n).map(|_| tok()).collect::<Result<_, _>>()?;
    let mut cons: AugMat<BigRational> = AugMat::new(m, n);
    for i in 0..m {
        let row: Vec<BigRational> = (0..n).map(|_| tok()).collect::<Result<_, _>>()?;
        cons.set_row(i, &row, tok()?);
    }
    if it.next().is_some() {
        return Err("unexpected trailing input".into());
    }
    Ok((n, m, cons, obj))
}

// ---------------------------------------------------------------------------
// Exported WASM functions
// ---------------------------------------------------------------------------

#[wasm_bindgen]
pub fn solve_two_phase(input: &str) -> String {
    match parse_f64(input) {
        Err(e) => format!("{{\"error\":\"{e}\"}}"),
        Ok((n, m, cons, obj)) => {
            let (r, s) = solve_lp_two_phase_steps(n, m, cons, obj);
            encode(r, s, fmt_f64)
        }
    }
}

#[wasm_bindgen]
pub fn solve_big_m(input: &str) -> String {
    match parse_f64(input) {
        Err(e) => format!("{{\"error\":\"{e}\"}}"),
        Ok((n, m, cons, obj)) => {
            let (r, s) = solve_lp_big_m_steps(n, m, cons, obj);
            encode_big_m(r, s, fmt_f64)
        }
    }
}

#[wasm_bindgen]
pub fn solve_two_phase_rational(input: &str) -> String {
    match parse_rational(input) {
        Err(e) => format!("{{\"error\":\"{e}\"}}"),
        Ok((n, m, cons, obj)) => {
            let (r, s) = solve_lp_two_phase_steps(n, m, cons, obj);
            encode(r, s, fmt_rat)
        }
    }
}

#[wasm_bindgen]
pub fn solve_big_m_rational(input: &str) -> String {
    match parse_rational(input) {
        Err(e) => format!("{{\"error\":\"{e}\"}}"),
        Ok((n, m, cons, obj)) => {
            let (r, s) = solve_lp_big_m_steps(n, m, cons, obj);
            encode_big_m(r, s, fmt_rat)
        }
    }
}
