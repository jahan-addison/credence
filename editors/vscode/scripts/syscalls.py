"""Write the syscall names of credence into the Brocoli grammar.

The names are read from the Linux and Darwin syscall tables of each target
in credence/target/common/syscall.h, so a syscall added to the compiler is
highlighted after this script is run again.
"""

import argparse
import json
import re
import sys
from pathlib import Path
from typing import NoReturn

ROOT = Path(__file__).resolve().parents[3]
SYSCALLS = ROOT / "credence/target/common/syscall.h"
GRAMMAR = Path(__file__).resolve().parents[1] / "syntaxes/brocoli.tmLanguage.json"
ENTRY = re.compile(r'^\s*\{\s*"([a-z_][a-z0-9_]*)",', re.MULTILINE)


class Argument_Parser(argparse.ArgumentParser):
    """An argument parser where an unknown flag exits 1 with the usage."""

    def error(self, message: str) -> NoReturn:
        self.print_usage(sys.stderr)
        self.exit(1, f"{self.prog}: error: {message}\n")


def syscall_names(header: Path) -> list[str]:
    """Read the syscall names of every table in a syscall header.

    Args:
        header: The path of syscall.h.

    Returns:
        The names, sorted and without duplicates.
    """
    return sorted(set(ENTRY.findall(header.read_text())))


def main() -> None:
    """Replace the syscalls pattern of the grammar."""
    parser = Argument_Parser(
        prog="syscalls.py",
        description="Write the syscall names of credence into the Brocoli grammar.",
    )
    parser.parse_args()

    names = syscall_names(SYSCALLS)
    grammar = json.loads(GRAMMAR.read_text())
    grammar["repository"]["syscalls"]["match"] = rf"\b({'|'.join(names)})\b(?=\s*\()"
    GRAMMAR.write_text(json.dumps(grammar, indent=2) + "\n")
    print(f"{len(names)} syscalls written to {GRAMMAR.name}")


if __name__ == "__main__":
    main()
