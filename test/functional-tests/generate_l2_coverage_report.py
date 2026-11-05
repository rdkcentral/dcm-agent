#!/usr/bin/env python3
"""
Generate a markdown L2 coverage report from an lcov tracefile.

Usage
-----
  python3 generate_l2_coverage_report.py \
      --tracefile    /tmp/l2_coverage/coverage.info \
      --features-dir test/functional-tests/features \
      --tests-dir    test/functional-tests/tests \
      --output       /tmp/L2_Coverage.md
"""

import argparse
import datetime
import os
import re
import sys

# (module label, path fragment); first match wins, everything else is DCM core
MODULES = [
    ('uploadstblogs', '/uploadstblogs/'),
    ('usbLogUpload', '/usbLogUpload/'),
    ('backup_logs', '/backup_logs/'),
]
CORE = 'DCM core'


def parse_tracefile(path):
    """Return {source_file: {lh, lf, fnh, fnf, brh, brf}} from an lcov .info file."""
    files = {}
    cur = None
    keys = {'LH': 'lh', 'LF': 'lf', 'FNH': 'fnh', 'FNF': 'fnf', 'BRH': 'brh', 'BRF': 'brf'}
    with open(path, errors='replace') as fh:
        for raw in fh:
            line = raw.strip()
            if line.startswith('SF:'):
                cur = dict.fromkeys(keys.values(), 0)
                cur['path'] = line[3:]
            elif line == 'end_of_record' and cur:
                files[cur['path']] = cur
                cur = None
            elif cur and ':' in line:
                key, _, val = line.partition(':')
                if key in keys and val.isdigit():
                    cur[keys[key]] = int(val)
    return files


def module_of(path):
    norm = path.replace('\\', '/')
    for label, fragment in MODULES:
        if fragment in norm:
            return label
    return CORE


def pct(hit, found):
    return 'N/A' if found == 0 else f'{hit / found * 100:.1f}%'


def totals(entries):
    return {k: sum(e[k] for e in entries) for k in ('lh', 'lf', 'fnh', 'fnf', 'brh', 'brf')}


def row(label, t):
    return (f"| {label} | {pct(t['lh'], t['lf'])} ({t['lh']}/{t['lf']}) "
            f"| {pct(t['fnh'], t['fnf'])} ({t['fnh']}/{t['fnf']}) "
            f"| {pct(t['brh'], t['brf'])} ({t['brh']}/{t['brf']}) |")


def count_matches(directory, name_re, line_re):
    total_files = total = 0
    if not directory or not os.path.isdir(directory):
        return 0, 0
    for fn in sorted(os.listdir(directory)):
        if re.match(name_re, fn):
            total_files += 1
            with open(os.path.join(directory, fn), errors='replace') as fh:
                total += sum(1 for line in fh if re.match(line_re, line))
    return total_files, total


def generate(files, features_dir, tests_dir, today):
    by_module = {}
    for path, entry in files.items():
        by_module.setdefault(module_of(path), []).append(entry)

    feat_files, scenarios = count_matches(features_dir, r'.*\.feature$',
                                          r'\s*Scenario(\s+Outline)?:')
    test_files, test_funcs = count_matches(tests_dir, r'test_.*\.py$', r'\s*(async\s+)?def test_')
    overall = totals(list(files.values()))

    out = [
        '# DCM Agent L2 Coverage Report', '',
        f'**Generated:** {today}  ',
        '**Components:** `dcm-agent`, `uploadstblogs`, `usbLogUpload`, `backup_logs`  ',
        '**Test suite:** `test/functional-tests/`  ',
        '**Coverage tool:** lcov (`--coverage` instrumentation, see `test/l2_coverage.sh`)', '',
        '## 1. Executive Summary', '',
        '| Metric | Value |', '|---|:---:|',
        f'| Feature files / scenarios | {feat_files} / {scenarios} |',
        f'| Test files / test functions | {test_files} / {test_funcs} |',
        f"| **Line coverage** | **{pct(overall['lh'], overall['lf'])}** ({overall['lh']} of {overall['lf']}) |",
        f"| **Function coverage** | **{pct(overall['fnh'], overall['fnf'])}** ({overall['fnh']} of {overall['fnf']}) |",
        f"| **Branch coverage** | **{pct(overall['brh'], overall['brf'])}** ({overall['brh']} of {overall['brf']}) |",
        '', '## 2. Coverage by Module', '',
        '| Module | Lines | Functions | Branches |', '|---|:---:|:---:|:---:|',
    ]
    for label in [CORE] + [m[0] for m in MODULES]:
        if label in by_module:
            out.append(row(label, totals(by_module[label])))
    out.append(row('**Total**', overall))

    out += ['', '## 3. Coverage by File', '',
            '| Module | File | Lines | Functions | Branches |', '|---|---|:---:|:---:|:---:|']
    for path, entry in sorted(files.items(), key=lambda kv: (module_of(kv[0]), kv[0])):
        t = totals([entry])
        out.append(f"| {module_of(path)} | `{os.path.basename(path)}` | "
                   f"{pct(t['lh'], t['lf'])} ({t['lh']}/{t['lf']}) | "
                   f"{pct(t['fnh'], t['fnf'])} ({t['fnh']}/{t['fnf']}) | "
                   f"{pct(t['brh'], t['brf'])} ({t['brh']}/{t['brf']}) |")

    out += ['', '## 4. Lowest Line Coverage (top 10, files with > 0 lines)', '',
            '| File | Lines |', '|---|:---:|']
    ranked = sorted((e for e in files.values() if e['lf'] > 0),
                    key=lambda e: e['lh'] / e['lf'])[:10]
    for e in ranked:
        out.append(f"| `{os.path.basename(e['path'])}` | {pct(e['lh'], e['lf'])} ({e['lh']}/{e['lf']}) |")
    return '\n'.join(out) + '\n'


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--tracefile', required=True, help='lcov .info file produced by test/l2_coverage.sh')
    ap.add_argument('--features-dir', help='directory containing .feature files')
    ap.add_argument('--tests-dir', help='directory containing test_*.py files')
    ap.add_argument('--output', required=True, help='markdown file to write')
    args = ap.parse_args()

    files = parse_tracefile(args.tracefile)
    if not files:
        sys.exit(f'No coverage records found in {args.tracefile}')

    report = generate(files, args.features_dir, args.tests_dir, datetime.date.today().isoformat())
    with open(args.output, 'w', encoding='utf-8') as fh:
        fh.write(report)
    print(f'Wrote {args.output}')


if __name__ == '__main__':
    main()
