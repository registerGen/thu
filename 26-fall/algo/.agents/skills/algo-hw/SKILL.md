---
name: algo-hw
description: Run an Algorithm Design and Analysis course homework end to end - scaffold the crate or LaTeX skeleton in this course's layout, add the random generator, CLI, 2-3 correctness tests and benchmark around the algorithm code the user already wrote, turn the benchmark CSV into table and figure snippets, write/review/improve the report, audit that every number traces to the CSV, the code or a calculation, cross-compile the Windows binary, write the TA readme, then clean up and pack hwNN.zip. Use for this course's homework, theory-only, experiment-only or both.
---

# Algorithm Design and Analysis homework

Coursework lives in `~/thu/26-fall/algo/`: theory write-ups as `hwNN.tex`,
coding tasks as `hwNN/MM-topic/`.

## Shape first

Detect the shape from the request and the files, and state it in one line
before working:

- **theory-only** — problem solutions in LaTeX, no crate (the `hw01.tex`
  style).
- **experiment-only** — a coding task with its own report, the `hw02` style:
  crate, benchmark, report. The report's Algorithm Analysis section carries
  whatever theory the task needs.
- **both** — a problem write-up *and* one or more coding tasks, i.e. `hw01`
  plus `hw02`: `hwNN/theoretical.tex` sitting next to the `hwNN/MM-topic/`
  folders.

## Modes

A numbered task list in the user's prompt maps onto these modes in order.

| Mode | Typical trigger | Produces | Gate |
| --- | --- | --- | --- |
| scaffold | "start hw03 task 1" | skeletons from `assets/` via `scripts/scaffold.py` | title, algorithm names, metric and size ladder confirmed |
| support | "I've written the algorithm impls" | generator, CLI, tests in `lib.rs` | tests green; the user's algorithm files untouched |
| measure | "write the bench" | `src/bin/bench.rs`, `report/<data>.csv`, generated snippets | reference agrees wherever it ran; snippets equal the CSV |
| report | "write the report" | `report/report.tex` | every number traced; clean build |
| review | "review my report" | edits + change manifest | report still builds; nothing landed silently |
| audit | **never asked for — it is your duty**: after the benchmark, after every report edit, and before packing | regex sweep, recomputed values, fixes | zero unmatched numbers |
| ship | "ready to submit" | `bin/*.exe`, `README.txt`, `hwNN.zip` | tests pass, report rebuilds from the zip, `unzip -t` clean |

Procedures, commands and checklists: [references/workflow.md](references/workflow.md).

## Invariants

- **Never edit the algorithm files the user wrote** — `src/naive.rs`,
  `src/divide_conquer.rs`, `src/strassen.rs` and friends. New behaviour goes
  into new files. The one exception is a real correctness bug: show the
  evidence, then fix it after the user agrees.
- **The theory write-up is report-only: never edit it.** `theoretical.tex` is
  the author's own text. Read it, check the mathematics, and put what is wrong
  in the review's change manifest — do not patch the file, however small the
  fix, and do not reformat it either. The report's prose, Algorithm Analysis
  section included, is edited as usual; only the theory write-up is off limits.
- **No external crates.** `[dependencies]` stays empty; random data comes from
  the hand-rolled PRNG in `src/generator.rs`.
- **Numbers are evidence.** Every figure, table cell and number in the report
  must be re-derivable from the benchmark CSV, the source, or a calculation
  stated next to it.
- **`check` means the reference agrees.** The benchmark CSV carries the input
  size, the algorithm, the time and the check. The size is often a single
  number, but it does not have to be: when a task scales two things at once
  (two sequence lengths, vertices and edges, a matrix pair, ...) keep one
  column per dimension — `n,m,algorithm,time_ms,check` — and say in the report
  which dimension is being varied and which is held fixed.
- **Report what you changed.** Review and audit edit and then list every edit
  with the recomputation behind it; nothing lands silently.
- **TODOs are a checklist, not decoration.** The scaffold's `TODO(algo-hw)`
  markers are yours to clear. "I've written the algorithm impls" is the signal:
  fill in `generator.rs`, the CLI, the tests and the adapters, and delete every
  marker those changes resolve, without being asked. **Shipping allows no
  markers at all** — not in the code, the report or the readme, not in the
  algorithm files, not in `theoretical.tex`: `grep -rn "TODO(algo-hw)" .` must
  come back empty. The theory write-up and the algorithm files are not yours to
  edit, so a marker still sitting there is reported to the user (path and line)
  and shipping waits for it to go — never patched by you, never ignored.
- **Text is wrapped at textwidth 80.** `theoretical.tex`, `report/report.tex`,
  `README.txt` and every template under `assets/` carry the `vim:tw=80:`
  modeline -- after a `%` at the top of the `.tex` files, on the last line of
  the README -- and keep every line within 80 columns, so a plain terminal
  shows the whole line and `gq` cannot improve it. A single token longer than
  the limit (in practice a URL, or one long shell command) stays unbroken,
  exactly as `gq` leaves it. Check with `awk 'length > 80' *.tex README.txt`.
- **Disclose AI in the report only.** A theory write-up (`theoretical.tex`)
  carries no AI disclosure and no link to this skill; only the report does, and
  the theory template leaves no reminder for either. The report's disclosure
  names the AI-written artifacts -- readme, CLI when the task has one,
  generator, tests, benchmark, plot/table scripts -- and says the report was
  reviewed and improved; reviewing the algorithm code is the author's own affair
  and is not stated. It ends by linking this skill —
  `https://github.com/registerGen/thu/tree/main/26-fall/algo/.agents/skills/algo-hw`
  — and noting that it is the homework author's own, so the workflow behind the
  work is inspectable.
- **Audit without being asked.** Running `scripts/audit.py` over the report and
  reconciling every number against the CSV, the code or a stated calculation is
  part of measuring, reviewing and shipping — never a favour the user has to
  request.
- **Ask before** deleting anything outside the homework folder, writing
  outside the workspace, replacing a prebuilt binary, or claiming a data
  source that was not actually used.

## Defaults (adapt them, then say what changed)

A size ladder that fits the task (powers of two, powers of ten, or one step per
point of whichever dimension the experiment varies, with the others held
fixed), and a cap on the algorithms that are too slow or too memory hungry at
large sizes — drop their largest sizes, whatever their complexity — plus a
0.5 s repetition budget, 3 significant figures in tables, 2-page report
target, English prose, course/name header, figure caption below the plot and
table caption above it, one `hwNN.zip` holding every task folder of the
homework.

## Where to look

- Pipeline, commands, checkpoints: [references/workflow.md](references/workflow.md)
- Crate layout, trait, tests, generator, CLI, bench: [references/rust-conventions.md](references/rust-conventions.md)
- Report skeleton, generators, review rubric, audits: [references/report-and-review.md](references/report-and-review.md)
- Cross-compile, README, cleanup, packaging: [references/ship.md](references/ship.md)
- Templates to copy: `assets/crate/`, `assets/report/`, `assets/theory/`
- Scripts: `scripts/scaffold.py`, `scripts/audit.py`

## Discovery

The skill lives in the course folder, so Codex finds it when the working
directory is inside `~/thu/26-fall/algo/`. Launching Codex elsewhere needs
`ln -s ~/thu/26-fall/algo/.agents/skills/algo-hw ~/.agents/skills/algo-hw`.
