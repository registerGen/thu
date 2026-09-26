# Ship

## 1. Cleanup

- LaTeX intermediates in `report/`: `*.aux`, `*.log`, `*.out`, `*.fls`,
  `*.fdb_latexmk`, `*.xdv` — keep the PDF.
- `target/` (about 50 MB per task) — never part of the archive.
- Scratch files created under `/tmp/`.
- `TODO(algo-hw)` markers — resolve or delete every one, and confirm with
  `grep -rn "TODO(algo-hw)" .` that the whole homework is clean: code, report,
  readme, `theoretical.tex`, algorithm files. Markers in the author's own files
  are reported to the user, never patched and never packed.
- Anything outside the homework folder needs the user's approval first.

## 2. Cross-compile for Windows

MSVC target with a statically linked CRT, so the TA installs nothing:

```bash
LIB="/opt/xwin/crt/lib/x86_64;/opt/xwin/sdk/lib/ucrt/x86_64;/opt/xwin/sdk/lib/um/x86_64" \
CARGO_TARGET_X86_64_PC_WINDOWS_MSVC_LINKER=/usr/bin/lld-link-24 \
CARGO_TARGET_X86_64_PC_WINDOWS_MSVC_RUSTFLAGS="-C target-feature=+crt-static" \
  cargo build --release --target x86_64-pc-windows-msvc --bins
```

`lld-link-24` is the linker on this machine; the MSVC CRT/SDK is already
splatted in `/opt/xwin`, so no download is needed. Verify with `file` (PE32+
x86-64 console) and by checking that the import table has no `VCRUNTIME140.dll`
or `MSVCP140.dll`. If the project folder is read-only to the shell, build with
`CARGO_TARGET_DIR=/tmp/<task>-target` and ask before copying the result out.

## 3. Publish

`<project>/bin/<name>.exe`, one file per shipped binary — normally the
benchmark driver, plus the CLI when the task has one.

## 4. README.txt

Wrap it at 80 columns and end it with the `vim:tw=80:` modeline, the same rule
the `.tex` sources follow (see `references/report-and-review.md`).

Windows-first, for a TA who has Windows with Rust, Python 3 and a LaTeX
installation, and who may run the prebuilt binary directly:

1. `TL;DR  <one command>  -- ~N s, checks every result; the write-up is report\report.pdf`, plus one line and the shortest example invocation if the task has a CLI.
2. `The instructions below are for Windows (cmd.exe); the differences under PowerShell are noted where they matter.`
3. `Requirements` — prebuilt binaries: 64-bit Windows and nothing else (static CRT); from source: Rust ≥ 1.85, edition 2024, MSVC toolchain; to rebuild the report: Python 3 and XeLaTeX for `unicode-math`.
4. `What is here` — one line per file that actually exists.
5. The command line interface, if any, with a no-file `gen | run` example.
6. `Run the experiment` — exact command, what it measures, how long it takes, what is checked, machine dependence, and the PowerShell caveat (5.1 writes `>` as UTF-16; use `| Set-Content -Encoding ascii`).
7. `Rebuild the figure and the table`, `Run the correctness tests`, `Rebuild the report`.
8. No cross-compile section — the TA does not build binaries.

Before delivering, audit every instruction against a Windows machine: paths,
redirection, and commands that need Python or LaTeX.

## 5. Pack

One archive per homework, `hwNN/hwNN.zip`, holding every task folder of that
homework — plus `theoretical.tex` and its PDF when the homework has a write-up:

```bash
cd hwNN && zip -r -q hwNN.zip 01-matmul 02-closest-pair \
  -x "*/target/*" "*/.git/*" "*/.agents/*" "*/.codex/*" "*.aux" "*.log" "*.out" \
     "*.fls" "*.fdb_latexmk" "*.xdv" "*.pyc" "*__pycache__/*" "*.DS_Store" "*.zip"
```

Verify by extracting to a scratch directory and, from the extraction alone,
running the tests and rebuilding the report. Report `unzip -t` and the sha256.

## 6. Report back

State what was cleaned, the binary paths and sizes, the archive path and
contents, the verification results, and anything the user still has to decide.
