#!/usr/bin/env python3
"""Detect and optionally smoke-launch PPSSPP for glyph.

This harness intentionally does not download or build PPSSPP;
the emulator must already be installed and discoverable on PATH, or supplied by
--ppsspp.
"""

from __future__ import annotations

import argparse
import os
import signal
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Sequence


PATH_CANDIDATES: tuple[str, ...] = (
    "ppsspp",
    "PPSSPPSDL",
    "PPSSPPQt",
    "ppsspp-emu.ppsspp-sdl",
)
FLATPAK_APP_ID = "org.ppsspp.PPSSPP"
DEFAULT_TIMEOUT_SECONDS = 10.0
OUTPUT_LIMIT = 4000


@dataclass(frozen=True)
class Candidate:
    name: str
    command: tuple[str, ...]
    source: str

    @property
    def display(self) -> str:
        return " ".join(self.command)


def is_windows() -> bool:
    return os.name == "nt"


def executable_file(path: Path) -> bool:
    if not path.is_file():
        return False
    if is_windows():
        return True
    return os.access(path, os.X_OK)


def resolve_override(value: str) -> Candidate | None:
    has_path_part = os.sep in value or (os.altsep is not None and os.altsep in value)
    if has_path_part or Path(value).is_absolute():
        path = Path(value).expanduser()
        if executable_file(path):
            return Candidate(path.name, (str(path),), "override")
        return None

    found = shutil.which(value)
    if found:
        return Candidate(value, (found,), "override")
    return None


def path_candidates(names: Iterable[str] = PATH_CANDIDATES) -> list[Candidate]:
    candidates: list[Candidate] = []
    seen: set[str] = set()

    for name in names:
        found = shutil.which(name)
        if not found:
            continue

        key = os.path.normcase(os.path.realpath(found))
        if key in seen:
            continue

        candidates.append(Candidate(name, (found,), "PATH"))
        seen.add(key)

    return candidates


def flatpak_hint() -> str:
    if shutil.which("flatpak"):
        return (
            "flatpak is installed, but this harness does not run Flatpak PPSSPP yet. "
            f"Future hook: flatpak run {FLATPAK_APP_ID} <EBOOT.PBP>"
        )
    return (
        "Flatpak PPSSPP is a future hook for this harness. "
        f"Expected app id: {FLATPAK_APP_ID}"
    )


def discover(override: str | None) -> tuple[list[Candidate], list[str]]:
    notes: list[str] = []

    if override:
        candidate = resolve_override(override)
        if candidate:
            return [candidate], notes
        notes.append(f"--ppsspp value is not executable or was not found: {override}")
        return [], notes

    candidates = path_candidates()
    if not candidates:
        notes.append(
            "No PPSSPP executable found on PATH. Looked for: "
            + ", ".join(PATH_CANDIDATES)
        )
        notes.append(flatpak_hint())
    return candidates, notes


def validate_eboot(path_value: str | None) -> Path | None:
    if not path_value:
        return None

    path = Path(path_value).expanduser()
    if not path.is_file():
        raise ValueError(f"--eboot does not point to a file: {path_value}")
    return path


def build_smoke_command(candidate: Candidate, eboot: Path | None) -> list[str]:
    command = list(candidate.command)
    if eboot is not None:
        command.append(str(eboot))
    else:
        command.append("--help")
    return command


def trimmed_output(value: str) -> str:
    if len(value) <= OUTPUT_LIMIT:
        return value.rstrip()
    return value[-OUTPUT_LIMIT:].rstrip()


def print_captured(label: str, value: str) -> None:
    text = trimmed_output(value)
    if text:
        print(f"{label}:")
        print(text)


def process_group_kwargs() -> dict[str, object]:
    if os.name == "posix":
        return {"start_new_session": True}
    if is_windows():
        return {"creationflags": subprocess.CREATE_NEW_PROCESS_GROUP}
    return {}


def terminate_process(process: subprocess.Popen[str]) -> None:
    if process.poll() is not None:
        return

    if os.name == "posix":
        try:
            os.killpg(process.pid, signal.SIGTERM)
            return
        except ProcessLookupError:
            return
        except OSError:
            pass

    process.terminate()


def kill_process(process: subprocess.Popen[str]) -> None:
    if process.poll() is not None:
        return

    if os.name == "posix":
        try:
            os.killpg(process.pid, signal.SIGKILL)
            return
        except ProcessLookupError:
            return
        except OSError:
            pass

    process.kill()


def run_with_timeout(command: Sequence[str], timeout: float) -> int:
    print("Running PPSSPP smoke command:")
    print("  " + " ".join(command))

    try:
        process = subprocess.Popen(
            command,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            **process_group_kwargs(),
        )
    except OSError as exc:
        print(f"Failed to start PPSSPP: {exc}", file=sys.stderr)
        return 1

    try:
        stdout, stderr = process.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        terminate_process(process)
        try:
            stdout, stderr = process.communicate(timeout=2.0)
        except subprocess.TimeoutExpired:
            kill_process(process)
            stdout, stderr = process.communicate()
            print("PPSSPP did not stop after terminate; killed process group.")

        print(
            f"PPSSPP was still running after {timeout:g}s; "
            "terminated it and treated launch as successful."
        )
        print_captured("stdout tail", stdout)
        print_captured("stderr tail", stderr)
        return 0

    print_captured("stdout", stdout)
    print_captured("stderr", stderr)

    if process.returncode == 0:
        print("PPSSPP smoke command exited successfully.")
        return 0

    print(f"PPSSPP smoke command exited with status {process.returncode}.")
    return process.returncode or 1


def print_detect_result(candidates: Sequence[Candidate], notes: Sequence[str]) -> None:
    if candidates:
        print("PPSSPP candidate(s) found:")
        for index, candidate in enumerate(candidates, start=1):
            print(f"  {index}. {candidate.display} ({candidate.source}, name={candidate.name})")
    else:
        print("PPSSPP unavailable for glyph harness.")

    for note in notes:
        print(f"Note: {note}")


def parser() -> argparse.ArgumentParser:
    arg_parser = argparse.ArgumentParser(
        description="Detect and optionally smoke-launch a local PPSSPP install.",
        epilog=(
            "Detect mode never builds or launches PPSSPP and exits 0 even when "
            "the emulator is unavailable. Use --run to execute the smoke command."
        ),
    )
    arg_parser.add_argument(
        "--run",
        action="store_true",
        help="launch the selected PPSSPP candidate; without --eboot this runs --help",
    )
    arg_parser.add_argument(
        "--eboot",
        metavar="PATH",
        help="optional EBOOT.PBP, ISO, or other PSP load target passed to PPSSPP with --run",
    )
    arg_parser.add_argument(
        "--ppsspp",
        metavar="PATH_OR_NAME",
        help="override emulator executable path or command name",
    )
    arg_parser.add_argument(
        "--timeout",
        type=float,
        default=DEFAULT_TIMEOUT_SECONDS,
        metavar="SECONDS",
        help=f"seconds to wait before terminating PPSSPP (default: {DEFAULT_TIMEOUT_SECONDS:g})",
    )
    return arg_parser


def main(argv: Sequence[str] | None = None) -> int:
    arg_parser = parser()
    args = arg_parser.parse_args(argv)
    if args.timeout <= 0:
        arg_parser.error("--timeout must be greater than 0")

    try:
        eboot = validate_eboot(args.eboot)
    except ValueError as exc:
        print(str(exc), file=sys.stderr)
        return 2

    candidates, notes = discover(args.ppsspp)
    print_detect_result(candidates, notes)

    if not args.run:
        if not candidates:
            print("Detect mode complete: PPSSPP is optional, so this is not a failure.")
        return 0

    if not candidates:
        sys.stdout.flush()
        print("Cannot run smoke command because PPSSPP is unavailable.", file=sys.stderr)
        return 2

    command = build_smoke_command(candidates[0], eboot)
    return run_with_timeout(command, args.timeout)


if __name__ == "__main__":
    raise SystemExit(main())
