{{TITLE}}

TL;DR  {{TLDR}}

The instructions below are for Windows (cmd.exe); the differences under
PowerShell are noted where they matter.

Requirements
  To run the prebuilt binaries: 64-bit Windows and nothing else -- the C
  runtime is linked statically, so no Visual C++ redistributable is needed.
  To build from source: Rust 1.85 or newer (the crate uses edition 2024) with
  the MSVC toolchain, i.e. the default x86_64-pc-windows-msvc target.
  To rebuild the report: Python 3 and a LaTeX installation that provides
  pgfplots, booktabs and siunitx (TeX Live or MiKTeX). report.tex loads
  unicode-math, so it needs XeLaTeX or LuaLaTeX.

What is here
{{FILES}}

Run the experiment
  Open a terminal in this folder (the one holding Cargo.toml) and either run
  the prebuilt binary

    bin\bench.exe > report\bench.csv

  or build it from source and run it in one step

    cargo run --release --bin bench > report\bench.csv

{{RUN_NOTES}}

  In PowerShell, run a program from the current folder with .\ :

    .\bin\bench.exe > report\bench.csv

  This works in PowerShell 7 or newer. Windows PowerShell 5.1 writes ">" as
  UTF-16, which the Python scripts below cannot read; use

    .\bin\bench.exe | Set-Content -Encoding ascii report\bench.csv

Rebuild the figure and the table
  cd report
  python3 plot.py          # figure -> plot.tex   (use python if you do not
  python3 table.py         # table  -> table.tex   have a python3 command)

Run the correctness tests
  cargo test --release

Rebuild the report
  cd report
  latexmk -xelatex report.tex
  report.tex loads unicode-math, so it needs XeLaTeX or LuaLaTeX -- hence the
  -xelatex. With MiKTeX, latexmk also needs Perl; otherwise simply run
  "xelatex report.tex" twice so that the cross-references settle.

vim:tw=80:
