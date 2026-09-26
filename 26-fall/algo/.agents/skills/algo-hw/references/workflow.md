# Workflow

## 0. Orient (always first)

Read `src/solution.rs` and every algorithm module the user wrote; note the
metric, the input type, the sizes they asked for, and any interface the task
needs (judge input, CLI, library). State the shape and the mode plan in one or
two lines, then work.

## 1. scaffold

```bash
python3 <skill>/scripts/scaffold.py hw03/01-topic --shape experiment --name topic \
  --title "Homework 3, Task 1: Matrix multiplication" \
  --algos Strassen,Hybrid --sizes 16,32,64,128,256,512,1024,2048,4096,8192
python3 <skill>/scripts/scaffold.py hw03 --shape theory --title "Homework 3"
python3 <skill>/scripts/scaffold.py hw03 --shape both --task 01-topic \
  --algos Strassen,Hybrid --sizes 16,32,64,128,256,512,1024
```

`experiment` scaffolds one task folder, `theory` just `<target>.tex`, and
`both` — a homework with a problem write-up *and* coding tasks — writes
`<target>/theoretical.tex` and `<target>/<--task>/` in one go, so the folder
holds the write-up next to its tasks.

Confirm with the user: title string, algorithm names, metric, size ladder and
how many sizes each algorithm runs. Fill the placeholders the scaffold leaves
(trait signature, algorithm modules, metric name).

Gate: `cargo test` runs, the report and its placeholder table/figure build.

## 2. support (tests, generator, CLI)

Write into new files only:

- `src/generator.rs` — seeded xorshift64\* PRNG, `generate(n)` /
  `generate_with_seed(n, seed)`, and the construction of whatever the task
  randomizes. No `rand`.
- `src/lib.rs` — `#[cfg(test)] mod tests` with 2-3 tests: a hand-computed case
  with a known answer, a boundary or degenerate case (n = 1, duplicates,
  collinear, all-equal), and randomized agreement of every algorithm against
  the naive reference. Deterministic seeds; a doc comment per test saying what
  it exercises.
- `src/main.rs` — only if the task has an I/O interface: `gen --size N
  [--seed S]` and `run (--naive | --<algorithm>)`, data as `n` then `n` lines,
  answer to stdout, hand-written argument parsing (no `clap`), and a
  `Scanner<R: BufRead>` tokenizer when inputs can be large.

"I've written the algorithm impls" is also the signal to clear the scaffold's
`TODO(algo-hw)` markers in the files you own: build the random input, parse and
print the task's data format, write the tests and the adapters, then delete each
marker those changes resolve — unprompted, because a marker that outlives the
code it was waiting for is a stale note in the user's repo.

Gate: `cargo test --release` green, hashes of the user's algorithm files
unchanged from before the edit, and no marker left in your own files.
`theoretical.tex` and the algorithm files are not yours to edit, so if a marker
is still in them, report the file and line to the user and wait.

## 3. measure (benchmark, CSV, snippets)

`src/bin/bench.rs` follows the house shape (`references/rust-conventions.md`):
size ladder, a per-algorithm count of how many sizes it runs (a slow or hungry
algorithm drops the largest ones, whatever its complexity), 0.5 s repetition
budget with the mean reported, naive first as the reference,
`<size>,algorithm,time_ms,check` on stdout, progress and mismatches on stderr,
and a hard limit so one hopeless algorithm cannot stall the run. When the input
size is not a single number, give each dimension its own CSV column and vary
one dimension at a time — the table shows them all, and the log-log figure uses
the varying one.

Start the run in the background — the largest sizes take minutes — and use the
wait to copy `assets/report/table.py` and `plot.py` into `report/` and to
verify them against a partial CSV.

Gate: each row's `check` is `ok` wherever a reference exists (`na` otherwise);
the generated `table.tex` / `plot.tex` compile into the report and their
values equal the CSV.

## 4. report

Fill `report/report.tex` from `assets/report/report.tex`, keeping the section
order and the quantitative abstract/conclusion. Regenerate the snippets, then
build with `latexmk -xelatex`.

Gate: 0 errors, 0 warnings, verified page count, every claim recomputed (see
`references/report-and-review.md`).

## 5. review

Read the report next to the CSV and produce the five buckets from
`references/report-and-review.md`, then apply the mechanical *and* analytical
improvements and publish a change manifest: before → after per edit, the
recomputation behind each number, and anything left alone with the reason.

Reviewing a theory write-up works the same way except that `theoretical.tex` is
never edited: the findings go into the manifest and the author applies them.

## 6. audit — always, never on request

This is your duty rather than a mode the user asks for: run it after the
benchmark, after every report edit, and again before packing.

```bash
python3 <skill>/scripts/audit.py numbers report/report.tex
python3 <skill>/scripts/audit.py stats   report/bench.csv
```

The first lists every number with its context; the second prints the ratios,
fitted exponents and per-size winners the report is allowed to quote.
Recompute each report number from the CSV/code/calculation, fix or drop what
does not match, and say which numbers you changed. Confirm that every figure
coordinate and table cell equals its CSV value after the documented rounding.

## 7. ship

Follow `references/ship.md`: cleanup, cross-compile, publish to `bin/`, write
the TA readme, pack `hwNN.zip`, verify the archive by extracting it. Ship only
when `grep -rn "TODO(algo-hw)" .` finds nothing anywhere — every placeholder in
the code, the report, the readme, `theoretical.tex` and the algorithm files has
been replaced by the real thing. Markers in the author's own files are the
user's to remove: say which ones remain and where, and do not pack.
