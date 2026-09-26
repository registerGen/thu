#!/usr/bin/env python3
r"""Audit the numbers of a homework report against its measurements.

  audit.py numbers <file.tex> [...]     every number in the text, with context
  audit.py stats   <data.csv> [--size-columns n]   what the measurements support

`numbers` is for the report pass: it lists each distinct number with the text
around it, so every one can be traced to the CSV, the code or a calculation.
`stats` prints the growth factors, fitted exponents, per-size winners and
crossovers that a report is allowed to quote. When the input size is several
numbers (`n,m,...`), pass them with `--size-columns`; growth factors are only
printed for a single numeric size column, since ratios need one growing axis.
"""

import argparse
import csv
import math
import re
import sys
from collections import defaultdict


def cmd_numbers(args) -> int:
    seen = {}
    for path in args.files:
        text = open(path).read()
        for match in re.finditer(r"\d+(?:\.\d+)?", text):
            context = " ".join(text[max(0, match.start() - args.context):match.end() + args.context].split())
            seen.setdefault(match.group(0), context)
    print(f"{len(seen)} distinct numbers")
    for number, context in sorted(seen.items(), key=lambda item: float(item[0])):
        print(f"  {number:<12} ... {context}")
    return 0


def fmt(value: float) -> str:
    return f"{value:.4g}"


def natural_key(size) -> list:
    return [int(part) if part.isdigit() else part.lower()
            for part in re.split(r"(\d+)", " ".join(size))]


def is_number(text: str) -> bool:
    try:
        float(text)
        return True
    except ValueError:
        return False


def cmd_stats(args) -> int:
    size_columns = [c.strip() for c in args.size_columns.split(",") if c.strip()]
    times = defaultdict(dict)
    order = []
    with open(args.csv_path, newline="") as handle:
        for row in csv.DictReader(handle):
            algorithm = row[args.algorithm_column]
            if algorithm not in order:
                order.append(algorithm)
            size = tuple(row[column].strip() for column in size_columns)
            times[algorithm][size] = float(row[args.time_column])
    if not order:
        raise SystemExit("error: no rows in %s" % args.csv_path)

    sizes = sorted({size for series in times.values() for size in series},
                   key=natural_key)
    print("sizes: " + ", ".join("x".join(size) for size in sizes))
    print("algorithms: " + ", ".join(order))
    print()

    for algorithm in order:
        series = sorted(times[algorithm].items(), key=lambda item: natural_key(item[0]))
        numeric = all(len(size) == 1 and is_number(size[0]) for size, _ in series)
        if len(series) < 2 or not numeric:
            print(f"{algorithm}: {len(series)} size(s); growth factors need a single "
                  "numeric size column (--size-columns)")
            print()
            continue
        series = [(float(size[0]), time) for size, time in series]
        factors, exponents = [], []
        for (n1, t1), (n2, t2) in zip(series, series[1:]):
            factor = t2 / t1
            factors.append(factor)
            exponents.append(math.log(factor) / math.log(n2 / n1))
        step = "doubling" if series[1][0] == 2 * series[0][0] else "step"
        print(f"{algorithm}: n = {series[0][0]} .. {series[-1][0]}")
        print("  growth per %s: %s" % (step, " ".join(fmt(f) for f in factors)))
        print("  min %s, max %s, mean %s"
              % (fmt(min(factors)), fmt(max(factors)),
                 fmt(sum(factors) / len(factors))))
        (n1, t1), (n2, t2) = series[0], series[-1]
        print("  fitted exponent %d -> %d: %s"
              % (n1, n2, fmt(math.log(t2 / t1) / math.log(n2 / n1))))
        print()

    reference = order[0]
    print(f"fastest algorithm per size (reference: {reference}):")
    for size in sizes:
        label = "x".join(size)
        present = [(times[a][size], a) for a in order if size in times[a]]
        fastest, name = min(present)
        base = times[reference].get(size)
        if base is None:
            print(f"  {label}: {name} (no {reference} reference)")
        elif name == reference:
            print(f"  {label}: {reference} is fastest")
        else:
            print(f"  {label}: {name} is {fmt(base / fastest)}x faster than {reference}")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    sub = parser.add_subparsers(dest="command", required=True)

    numbers = sub.add_parser("numbers", help="numbers in a .tex with context")
    numbers.add_argument("files", nargs="+")
    numbers.add_argument("--context", type=int, default=45,
                         help="characters of context around each number")
    numbers.set_defaults(func=cmd_numbers)

    stats = sub.add_parser("stats", help="growth factors and winners from a CSV")
    stats.add_argument("csv_path")
    stats.add_argument("--time-column", default="time_ms")
    stats.add_argument("--algorithm-column", default="algorithm")
    stats.add_argument("--size-columns", default="n",
                       help="comma separated size columns, e.g. n or n,m")
    stats.set_defaults(func=cmd_stats)

    args = parser.parse_args()
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
