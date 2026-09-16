#!/usr/bin/env python3
"""Split a Kernel-V serial log into one file per test.

The kernel test runner brackets every test with machine-parseable markers
emitted by ktest_marker() in kernel/tests/test_runner.c:

    ... [KVTEST] @@KV_TEST BEGIN name=spinlock cat=unit sc=42
    ...            (all lines produced while the test runs)
    ... [KVTEST] @@KV_TEST END   name=spinlock cat=unit sc=42

Because tests run sequentially in a single QEMU session, the serial stream is
time-ordered. This tool uses a WINDOW model: every line between a BEGIN and its
matching END is attributed to that test - including context switches, IRQ
handlers and child-process output that interleave during the test. That is the
whole point: you get one file with everything that happened during the test.

Output layout (under --out-dir, default logs/run-<timestamp>/):

    full.log                 verbatim copy of the input
    index.md                 summary table (result, line count, link)
    00_preamble.log          lines before the first test (boot, init)
    <category>/<name>.log    one file per test
    failed/<name>.log        copies of tests that look like failures

A test is flagged FAIL if its window contains an EMERG/ERROR level line or a
"[FAIL]" assertion token; otherwise PASS.
"""

from __future__ import annotations

import argparse
import datetime as _dt
import re
import shutil
import sys
from pathlib import Path

MARKER_RE = re.compile(
    r"@@KV_TEST\s+(?P<phase>BEGIN|END)\s+name=(?P<name>\S+)"
    r"(?:\s+cat=(?P<cat>\S+))?(?:\s+sc=(?P<sc>\S+))?"
)
# A test only fails on an explicit assertion token. Kernel [ERROR]/[EMERG]
# level lines are NOT failures here: negative-path tests deliberately trigger
# them (e.g. "buffer is empty", "unregistered handler").
FAIL_RE = re.compile(r"\[FAIL\]")


class Test:
    __slots__ = ("name", "category", "sc", "lines", "start_line", "end_line",
                 "closed", "rel")

    def __init__(self, name: str, category: str, sc: str, start_line: int):
        self.name = name
        self.category = category or "uncategorized"
        self.sc = sc or ""
        self.lines: list[str] = []
        self.start_line = start_line
        self.end_line = start_line
        self.closed = False  # True once the END marker was seen
        self.rel = Path()

    @property
    def has_fail(self) -> bool:
        return any(FAIL_RE.search(ln) for ln in self.lines)

    @property
    def incomplete(self) -> bool:
        return not self.closed

    @property
    def ok(self) -> bool:
        return self.closed and not self.has_fail

    @property
    def result(self) -> str:
        if self.has_fail:
            return "FAIL"
        if not self.closed:
            return "INCOMPLETE"
        return "PASS"


def parse(lines: list[str]) -> tuple[list[str], list[Test], list[str]]:
    """Return (preamble, tests, epilogue) with EVERY line accounted for.

    Model (no gaps by construction):
    - preamble  : lines before the first BEGIN (boot / init).
    - test file : lines inside its [BEGIN, END] window.
    - between   : lines after a test's END but before the NEXT BEGIN (small
                  async timer/scheduler fallout) are attributed to that
                  preceding test as trailing output.
    - epilogue  : lines after the FINAL test closes with no further BEGIN
                  (the post-suite idle loop) go to 99_epilogue.log instead of
                  bloating the last test.
    """
    preamble: list[str] = []
    tests: list[Test] = []
    current: Test | None = None  # inside an open BEGIN..END window
    last: Test | None = None     # most recent closed test
    pending: list[str] = []      # trailing lines awaiting next BEGIN or EOF

    def flush_pending_to_last() -> None:
        if pending and last is not None:
            last.lines.extend(pending)
            last.end_line = last.start_line + len(last.lines)
        pending.clear()

    for i, raw in enumerate(lines):
        m = MARKER_RE.search(raw)
        if m and m.group("phase") == "BEGIN":
            flush_pending_to_last()  # between-test trailing -> preceding test
            current = Test(m.group("name"), m.group("cat"), m.group("sc"), i + 1)
            current.lines.append(raw)
            current.end_line = i + 1
            tests.append(current)
            continue
        if m and m.group("phase") == "END":
            if current is not None and current.name == m.group("name"):
                current.lines.append(raw)
                current.end_line = i + 1
                current.closed = True
                last = current
                current = None
            elif current is not None:
                current.lines.append(raw)
                current.end_line = i + 1
            elif last is not None:
                pending.append(raw)
            else:
                preamble.append(raw)
            continue

        if current is not None:
            current.lines.append(raw)
            current.end_line = i + 1
        elif last is not None:
            pending.append(raw)  # provisional trailing (last test or epilogue)
        else:
            preamble.append(raw)

    # Any lines still pending after EOF are post-suite idle -> epilogue.
    epilogue = list(pending)
    return preamble, tests, epilogue


    return preamble, tests


def _slug(text: str) -> str:
    return re.sub(r"[^A-Za-z0-9._-]", "_", text)


def write_outputs(out_dir: Path, raw: list[str], preamble: list[str],
                  tests: list[Test], epilogue: list[str]) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)

    (out_dir / "full.log").write_text("".join(raw), encoding="utf-8")
    if preamble:
        (out_dir / "00_preamble.log").write_text("".join(preamble),
                                                 encoding="utf-8")
    if epilogue:
        (out_dir / "99_epilogue.log").write_text("".join(epilogue),
                                                 encoding="utf-8")

    failed_dir = out_dir / "failed"
    for t in tests:
        rel = Path(_slug(t.category)) / f"{_slug(t.name)}.log"
        dest = out_dir / rel
        dest.parent.mkdir(parents=True, exist_ok=True)
        dest.write_text("".join(t.lines), encoding="utf-8")
        t.rel = rel
        if not t.ok:
            failed_dir.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(dest, failed_dir / f"{_slug(t.name)}.log")

    write_index(out_dir, tests)


def write_index(out_dir: Path, tests: list[Test]) -> None:
    passed = sum(1 for t in tests if t.ok)
    failed = sum(1 for t in tests if t.has_fail)
    incomplete = sum(1 for t in tests if t.incomplete and not t.has_fail)

    rows = [
        "# Kernel-V test log index",
        "",
        f"- Generated: {_dt.datetime.now().isoformat(timespec='seconds')}",
        f"- Tests: {len(tests)}  |  PASS: {passed}  |  FAIL: {failed}"
        f"  |  INCOMPLETE: {incomplete}",
        "",
        "| Result | Category | Test | Lines | sc | Log |",
        "| ------ | -------- | ---- | ----- | -- | --- |",
    ]
    order = {"FAIL": 0, "INCOMPLETE": 1, "PASS": 2}
    marks = {"FAIL": "❌ FAIL", "INCOMPLETE": "⚠️ INCOMPLETE", "PASS": "✅ PASS"}
    for t in sorted(tests, key=lambda x: (order[x.result], x.category, x.name)):
        rel = t.rel
        rows.append(
            f"| {marks[t.result]} | {t.category} | {t.name} | {len(t.lines)} | "
            f"{t.sc} | [{rel}]({rel.as_posix()}) |"
        )
    rows.append("")
    (out_dir / "index.md").write_text("\n".join(rows), encoding="utf-8")


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("log", nargs="?", default="serial.log",
                    help="serial log to split (default: serial.log)")
    ap.add_argument("--out-dir", default=None,
                    help="output directory (default: logs/run-<timestamp>)")
    ap.add_argument("--latest", action="store_true",
                    help="also update logs/latest -> this run")
    args = ap.parse_args(argv)

    src = Path(args.log)
    if not src.is_file():
        print(f"split_tests: no such log: {src}", file=sys.stderr)
        return 1

    raw = src.read_text(encoding="utf-8", errors="replace").splitlines(keepends=True)
    preamble, tests, epilogue = parse(raw)

    if args.out_dir:
        out_dir = Path(args.out_dir)
    else:
        stamp = _dt.datetime.now().strftime("%Y%m%dT%H%M%S")
        out_dir = Path("logs") / f"run-{stamp}"

    write_outputs(out_dir, raw, preamble, tests, epilogue)

    if args.latest or not args.out_dir:
        link = Path("logs") / "latest"
        try:
            if link.is_symlink() or link.exists():
                link.unlink()
            link.symlink_to(out_dir.name)
        except OSError:
            pass  # non-fatal (e.g. filesystem without symlinks)

    passed = sum(1 for t in tests if t.ok)
    failed = sum(1 for t in tests if t.has_fail)
    incomplete = sum(1 for t in tests if t.incomplete and not t.has_fail)
    if not tests:
        print(f"split_tests: no @@KV_TEST markers found in {src} "
              f"(is this a test build?)", file=sys.stderr)
    print(f"split_tests: {len(tests)} tests -> {out_dir}  "
          f"(PASS {passed}, FAIL {failed}, INCOMPLETE {incomplete})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
