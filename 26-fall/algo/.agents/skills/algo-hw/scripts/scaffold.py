#!/usr/bin/env python3
r"""Scaffold a new Algorithm Design and Analysis homework from the templates.

Usage:
  scaffold.py hw03/01-topic --title "Homework 3, Task 1: Matrix multiplication" \
      --algos Strassen,Hybrid --op mat_mul --item Matrix --out Out \
      --sizes 16,32,64,128
  scaffold.py hw04/02-queries --algos DivideConquer --op solve --item "Vec<Point>" \
      --sizes 1000x10,1000x100,1000x1000 --size-arity 2
  scaffold.py hw04/03-matmul --algos Strassen,Hybrid --op mat_mul \
      --params "a: &[Vec<i32>]; b: &[Vec<i32>]" \
      --input-type "(Vec<Vec<i32>>, Vec<Vec<i32>>)" \
      --call-args "&input.0; &input.1" --sizes 16,32,64,128
  scaffold.py hw03 --shape theory --title "Homework 3"
  scaffold.py hw05 --shape both --task 01-matmul --algos Strassen,Hybrid \
      --sizes 16,32,64,128,256

Shapes:
  theory      `<target>.tex` -- a write-up-only homework (hw01 style)
  experiment  `<target>/`    -- one coding task: crate + benchmark + report (hw02 style)
  both        `<target>/theoretical.tex` plus `<target>/<--task>/` -- a write-up
              and coding tasks
Everything is written relative to the current working directory.
"""

import argparse
import re
import shutil
import subprocess
import sys
from pathlib import Path

SKILL = Path(__file__).resolve().parents[1]
ASSETS = SKILL / "assets"


def slug_name(target: str) -> str:
    """`hw03/01-matmul` -> `matmul`."""
    base = Path(target).name
    return re.sub(r"^\d+[-_]", "", base) or base


def default_title(target: str) -> str:
    """`hw03/01-matmul` -> `Homework 3, Task 1: Matmul`."""
    parts = Path(target).parts
    homework = re.sub(r"\D", "", parts[0]) or parts[0]
    if len(parts) == 1:
        return f"Homework {homework}"
    task = re.sub(r"\D.*", "", parts[1]) or "1"
    name = slug_name(target).replace("-", " ")
    return f"Homework {homework}, Task {int(task)}: {name.capitalize()}"


def render(text: str, values: dict) -> str:
    for key, value in values.items():
        text = text.replace("{{%s}}" % key, value)
    leftover = sorted(set(re.findall(r"\{\{([A-Z_]+)\}\}", text)))
    if leftover:
        raise SystemExit("error: unfilled placeholders: " + ", ".join(leftover))
    return text


def parse_sizes(text: str, arity: int) -> list:
    """`100,1000` for arity 1, `1000x10,1000x100` for arity 2."""
    sizes = []
    for chunk in text.split(","):
        chunk = chunk.strip()
        if not chunk:
            continue
        parts = [chunk] if arity == 1 else [p.strip() for p in chunk.split("x")]
        if len(parts) != arity:
            raise SystemExit(
                "error: size %r does not have %d number(s); write them as %s"
                % (chunk, arity, "x".join(["1000"] * arity))
            )
        try:
            sizes.append([int(part) for part in parts])
        except ValueError:
            raise SystemExit("error: size %r is not numeric" % chunk)
    if not sizes:
        raise SystemExit("error: --sizes needs at least one size")
    return sizes


def write(path: Path, text: str, created: list, force: bool) -> None:
    if path.exists() and not force:
        created.append((path, "kept (already exists)"))
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text)
    created.append((path, "written"))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("target", help="e.g. hw03/01-topic, or hw03 for theory")
    parser.add_argument("--shape", choices=("both", "experiment", "theory"),
                        default="experiment",
                        help="theory: write-up only; experiment: one task folder; "
                             "both: write-up plus <target>/<--task>/")
    parser.add_argument("--task", help="task folder name when --shape both, "
                                       "e.g. 01-matmul")
    parser.add_argument("--name", help="crate name (default: derived from target)")
    parser.add_argument("--title", help="title used in the report and readme")
    parser.add_argument("--algos", default="Naive",
                        help="comma separated algorithm names, reference first")
    parser.add_argument("--op", default="solve", help="trait method name")
    parser.add_argument("--item", default="i64",
                        help="item type used in the default --input-type")
    parser.add_argument("--out", default="f64", help="operation's return type")
    parser.add_argument("--params", default=None,
                        help="the operation's parameter list, `;` between "
                             "parameters, e.g. \"a: &[Vec<i32>]; b: &[Vec<i32>]\" "
                             "(default: \"input: &[<--item>]\")")
    parser.add_argument("--input-type", default=None,
                        help="what generator::generate returns "
                             "(default: Vec<<--item>>)")
    parser.add_argument("--call-args", default=None,
                        help="what the adapter passes to the operation, `;` "
                             "between arguments (default: the single `input`; "
                             "required when --params has several parameters)")
    parser.add_argument("--sizes",
                        help="benchmark sizes: `100,1000,10000` for one number per "
                             "size, or `1000x10,1000x100` when --size-arity is 2")
    parser.add_argument("--size-arity", type=int, default=1, choices=(1, 2, 3),
                        help="how many numbers make up one input size")
    parser.add_argument("--author", default="Changshui Xiang")
    parser.add_argument("--course", default="Algorithm Design and Analysis")
    parser.add_argument("--machine", default="a PC with Intel Core i9-12900H running WSL2",
                        help="machine description quoted in the report")
    parser.add_argument("--force", action="store_true",
                        help="overwrite files that already exist")
    parser.add_argument("--no-verify", action="store_true",
                        help="skip the cargo test check")
    args = parser.parse_args()

    if args.shape == "both" and not args.task:
        raise SystemExit("error: --shape both needs --task <folder>, e.g. --task 01-matmul")
    if args.shape == "experiment" and args.task:
        raise SystemExit("error: --task only applies to --shape both")
    crate_target = (Path(args.target) / args.task if args.shape == "both"
                    else Path(args.target))

    name = args.name or slug_name(str(crate_target))
    title = args.title or default_title(str(crate_target))
    algos = [a.strip() for a in args.algos.split(",") if a.strip()]
    if args.shape != "theory" and not args.sizes:
        raise SystemExit("error: --sizes is required, e.g. `--sizes 100,1000,10000`")
    sizes = parse_sizes(args.sizes, args.size_arity) if args.sizes else []
    arity = args.size_arity
    size_names = ["n", "m", "k"][:arity]
    if arity == 1:
        size_type, size_literal = "usize", ", ".join(str(s[0]) for s in sizes)
        size_format = "size.to_string()"
        size_line = "size.to_string()"
        size_parse = 'text.parse().map_err(|_| format!("invalid size `{text}`"))'
    else:
        size_type = "(" + ", ".join(["usize"] * arity) + ")"
        size_literal = ", ".join("(" + ", ".join(str(v) for v in s) + ")" for s in sizes)
        fields = ", ".join(f"size.{i}" for i in range(arity))
        holes = ",".join("{}" for _ in range(arity))
        size_format = f'format!("{holes}", {fields})'
        size_line = f'format!("{" ".join(["{}"] * arity)}", {fields})'
        reads = "\n".join(
            f'    let v{i} = parts[{i}].parse().map_err(|_| format!("invalid size `{{text}}`"))?;'
            for i in range(arity)
        )
        returns = ", ".join(f"v{i}" for i in range(arity))
        examples = "x".join(["1000", "100", "10"][:arity])
        size_parse = (
            f'let parts: Vec<&str> = text.split(\'x\').map(str::trim).collect();\n'
            f'    if parts.len() != {arity} {{\n'
            f'        return Err(format!("invalid size `{{text}}`, want e.g. {examples}"));\n'
            f'    }}\n{reads}\n    Ok(({returns}))'
        )
    size_help = "x".join(["1000", "100", "10"][:arity])

    params = args.params or f"input: &[{args.item}]"
    param_list = [part.strip() for part in params.split(";") if part.strip()]
    param_types = [p.split(":", 1)[1].strip() if ":" in p else p for p in param_list]
    if len(param_list) > 1 and not args.call_args:
        raise SystemExit(
            "error: --params has %d parameters, so --call-args is required, e.g. "
            '--call-args "&input.0; &input.1"' % len(param_list)
        )
    call_args = "; ".join(part.strip() for part in (args.call_args or "input").split(";")
                          if part.strip()).replace("; ", ", ")
    names = [p.split(":", 1)[0].strip() if ":" in p else None for p in param_list]
    if not all(names):
        param_discard = ""
    elif len(names) == 1:
        param_discard = f"let _ = {names[0]};"
    else:
        param_discard = "let _ = (%s);" % ", ".join(names)
    homework = "Homework " + (re.sub(r"\D", "", Path(args.target).parts[0]) or "N")

    try:
        rustc = subprocess.run(["rustc", "--version"], capture_output=True,
                               text=True, check=True).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        rustc = "rustc <version>"

    values = {
        "NAME": name,
        "CRATE": name.replace("-", "_"),
        "TITLE": title,
        "HOMEWORK": homework,
        "COURSE": args.course,
        "AUTHOR": args.author,
        "MACHINE": args.machine,
        "RUSTC": rustc,
        "ALGORITHM": algos[0],
        "ALGOS": ", ".join(algos),
        "OP": args.op,
        "ITEM": args.item,
        "OUT": args.out,
        "PARAMS": ", ".join(param_list),
        "PARAM_TYPES": ", ".join(param_types),
        "INPUT_TYPE": args.input_type or f"Vec<{args.item}>",
        "CALL_ARGS": call_args,
        "PARAM_DISCARD": param_discard,
        "SIZE_TYPE": size_type,
        "SIZE_HEADER": ",".join(size_names),
        "SIZE_FORMAT": size_format,
        "SIZE_LINE": size_line,
        "SIZE_PARSE": size_parse,
        "SIZE_HELP": size_help,
        "SIZES": size_literal,
        "SIZES_LEN": str(len(sizes)),
        "ABSTRACT": "TODO(algo-hw): the measured exponent, speed-up and crossover.",
        "AI_SCOPE": "the readme, the command line interface, the generator, the tests and the benchmark",
        # Pre-wrapped, because README.txt is held to textwidth 80 too.
        "TLDR": ("TODO: time the benchmark, then put the headline command here,\n"
                 "       e.g. bin\\bench.exe > report\\bench.csv -- ~20 s, checks\n"
                 "       every result; the write-up is report\\report.pdf"),
        "RUN_NOTES": ("    - TODO: how long it takes, what it checks, and that the times\n"
                      "      depend on the machine."),
    }

    created = []
    target = Path(args.target)

    # Files cargo init just wrote are ours to replace.
    initialized: set = set()

    if args.shape in ("theory", "both"):
        template = (ASSETS / "theory" / "homework.tex").read_text()
        # A homework with both parts keeps the write-up inside its own folder,
        # next to the task folders; a theory-only homework is `hwNN.tex`.
        theory_path = (Path(str(target) + ".tex") if args.shape == "theory"
                       else target / "theoretical.tex")
        write(theory_path, render(template, values), created, args.force)

    if args.shape != "theory":
        crate_target.mkdir(parents=True, exist_ok=True)
        if not (crate_target / "Cargo.toml").exists():
            subprocess.run(["cargo", "init", "--lib", "--vcs=none", "--name", name],
                           cwd=crate_target, check=True)
            created.append((crate_target / "Cargo.toml", "initialized by cargo"))
            initialized = {crate_target / "Cargo.toml", crate_target / "src" / "lib.rs"}

        for source in sorted((ASSETS / "crate").rglob("*")):
            if source.is_file():
                relative = source.relative_to(ASSETS / "crate")
                destination = crate_target / relative
                write(destination, render(source.read_text(), values), created,
                      args.force or destination in initialized)

        # The README is written last: it lists the files that actually exist.
        root_readme = None
        for source in sorted((ASSETS / "report").glob("*")):
            if source.name == "README.txt":
                root_readme = source
                continue
            write(crate_target / "report" / source.name,
                  render(source.read_text(), values), created, args.force)

        if root_readme is not None:
            values["FILES"] = "\n".join(
                "  " + str(path.relative_to(crate_target))
                for path, _ in created
                if path.is_file() and path.is_relative_to(crate_target)
            )
            write(crate_target / "README.txt",
                  render(root_readme.read_text(), values), created, args.force)

        subprocess.run(["cargo", "fmt"], cwd=crate_target, check=False)

    print(f"scaffolded {args.shape} homework at {target.resolve()}")
    for path, note in created:
        print(f"  {note:<22} {path}")
    if args.shape == "theory":
        print("\nnext: write the solutions, then build with "
              "`latexmk -xelatex` (references/workflow.md).")
    else:
        print("\nnext: fill the TODO(algo-hw) markers, then `cargo test --release`, "
              "then the measure/report modes (references/workflow.md).")

    if args.shape != "theory" and not args.no_verify:
        result = subprocess.run(["cargo", "test", "--release"], cwd=crate_target)
        if result.returncode != 0:
            print("error: the scaffolded crate does not build", file=sys.stderr)
            return result.returncode
        print("cargo test --release: ok")
    return 0


if __name__ == "__main__":
    sys.exit(main())
