#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Preserve GSPC font ordinals when a shell glob supplies font10 before font2."""
import argparse
from pathlib import Path
import re
import subprocess

from check_font_refs import require, validate_bundle


def ordered_members(members):
    positions = [i for i, value in enumerate(members) if Path(value).suffix == '.gfb']
    numbered = []
    for index in positions:
        match = re.fullmatch(r'(.+)_font(\d+)\.gfb', Path(members[index]).name)
        require(match is not None, f'Cannot identify font ordinal: {members[index]}')
        numbered.append((match[1], int(match[2]), members[index]))
    numbered.sort()
    require(len({prefix for prefix, _, _ in numbered}) <= 1, 'Bundle helper expects one compiled scene font family')
    require([ordinal for _, ordinal, _ in numbered] == list(range(len(numbered))), 'Missing or duplicate compiler font ordinal')
    result = list(members)
    for index, (_, _, value) in zip(positions, numbered):
        result[index] = value
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('compiler')
    parser.add_argument('-o', '--output', required=True, type=Path)
    parser.add_argument('members', nargs='+')
    args = parser.parse_args()
    subprocess.run([args.compiler, 'bundle', '-o', str(args.output), *ordered_members(args.members)], check=True)
    refs = validate_bundle(args.output.read_bytes(), name=str(args.output))
    print(f'Font order verified: {args.output.name}, {refs} references')


if __name__ == '__main__':
    main()
