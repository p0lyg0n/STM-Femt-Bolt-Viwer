#!/usr/bin/env -S uv run --script
# /// script
# requires-python = ">=3.11"
# ///

"""Runs C++ style tooling on tests and newly-added files."""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

CPP_SUFFIXES = {".h", ".hpp", ".hh", ".hxx", ".cc", ".cpp", ".cxx"}
HEADER_SUFFIXES = {".h", ".hpp", ".hh", ".hxx"}
SOURCE_SUFFIXES = {".cc", ".cpp", ".cxx"}
CONTROL_KEYWORDS = {"if", "for", "while", "switch", "catch", "return"}
CLASS_KINDS = {"class", "struct"}


@dataclass(frozen=True)
class Candidate:
    """Represents a documented declaration candidate."""

    line_number: int
    summary: str
    symbol_name: str | None = None


def run_command(args: list[str], cwd: Path, check: bool = True) -> subprocess.CompletedProcess[str]:
    """Runs a subprocess and returns its completed result."""

    return subprocess.run(
        args,
        cwd=cwd,
        check=check,
        text=True,
        capture_output=True,
    )


def git_output(repo_root: Path, *args: str, check: bool = True) -> str:
    """Runs a git command rooted at the repository."""

    completed = run_command(["git", *args], cwd=repo_root, check=check)
    return completed.stdout


def normalize_cpp_paths(paths: Iterable[str]) -> list[Path]:
    """Filters a path list down to relative C/C++ files."""

    unique_paths = set()
    for raw_path in paths:
        path = raw_path.strip()
        if not path:
            continue
        candidate = Path(path)
        if candidate.suffix.lower() not in CPP_SUFFIXES:
            continue
        unique_paths.add(Path(path.replace("\\", "/")))
    return sorted(unique_paths)


def tracked_test_files(repo_root: Path) -> list[Path]:
    """Returns tracked test files under tests/."""

    output = git_output(repo_root, "ls-files", "--", "tests")
    return normalize_cpp_paths(output.splitlines())


def untracked_cpp_files(repo_root: Path) -> list[Path]:
    """Returns untracked C/C++ files."""

    output = git_output(repo_root, "ls-files", "--others", "--exclude-standard")
    return normalize_cpp_paths(output.splitlines())


def ensure_origin_ref(base_ref: str) -> str:
    """Normalizes a remote branch reference for origin."""

    if base_ref.startswith("origin/"):
        return base_ref
    return f"origin/{base_ref}"


def diff_added_cpp_files(repo_root: Path, revision_range: str) -> list[Path]:
    """Returns Added files from a diff range."""

    output = git_output(repo_root, "diff", "--name-only", "--diff-filter=A", revision_range, "--")
    return normalize_cpp_paths(output.splitlines())


def local_new_cpp_files(repo_root: Path, base_ref: str) -> list[Path]:
    """Returns new C/C++ files for the local workflow."""

    added_paths = diff_added_cpp_files(repo_root, f"{ensure_origin_ref(base_ref)}...HEAD")
    untracked_paths = untracked_cpp_files(repo_root)
    return sorted(set(added_paths) | set(untracked_paths))


def ci_new_cpp_files(repo_root: Path) -> list[Path]:
    """Returns new C/C++ files for the active CI event."""

    event_name = os.environ.get("GITHUB_EVENT_NAME", "")
    if event_name == "pull_request":
        base_ref = os.environ.get("GITHUB_BASE_REF") or os.environ.get("STM_STYLE_BASE_REF")
        if not base_ref:
            raise SystemExit("GITHUB_BASE_REF is required for pull_request style checks.")
        return diff_added_cpp_files(repo_root, f"{ensure_origin_ref(base_ref)}...HEAD")

    if event_name == "push":
        before = os.environ.get("STM_STYLE_BEFORE")
        sha = os.environ.get("GITHUB_SHA") or os.environ.get("STM_STYLE_SHA")
        if not before or not sha:
            raise SystemExit("STM_STYLE_BEFORE and GITHUB_SHA are required for push style checks.")
        return diff_added_cpp_files(repo_root, f"{before}..{sha}")

    raise SystemExit(f"Unsupported CI event for style selection: {event_name}")


def style_targets(repo_root: Path, mode: str, base_ref: str) -> list[Path]:
    """Returns the combined style target set."""

    tests_paths = tracked_test_files(repo_root)
    if mode == "local":
        new_paths = local_new_cpp_files(repo_root, base_ref)
    else:
        new_paths = ci_new_cpp_files(repo_root)
    return sorted(set(tests_paths) | set(new_paths))


def docs_targets(repo_root: Path, mode: str, base_ref: str) -> list[Path]:
    """Returns only the new-file targets for docs enforcement."""

    if mode == "local":
        return local_new_cpp_files(repo_root, base_ref)
    return ci_new_cpp_files(repo_root)


def first_non_empty_index(lines: list[str]) -> int | None:
    """Returns the first non-empty line index."""

    for index, line in enumerate(lines):
        if line.strip():
            return index
    return None


def has_file_overview(lines: list[str]) -> bool:
    """Checks whether the file starts with a comment overview."""

    first_index = first_non_empty_index(lines)
    if first_index is None:
        return False
    first_line = lines[first_index].lstrip()
    return first_line.startswith("//") or first_line.startswith("/*")


def strip_comments(lines: list[str]) -> list[str]:
    """Removes comments while preserving line count."""

    output: list[str] = []
    in_block_comment = False
    for line in lines:
        index = 0
        cleaned = []
        while index < len(line):
            if in_block_comment:
                end = line.find("*/", index)
                if end == -1:
                    index = len(line)
                    continue
                in_block_comment = False
                index = end + 2
                continue
            if line.startswith("//", index):
                break
            if line.startswith("/*", index):
                in_block_comment = True
                index += 2
                continue
            cleaned.append(line[index])
            index += 1
        output.append("".join(cleaned))
    return output


def is_comment_line(line: str) -> bool:
    """Returns whether a line participates in a comment block."""

    stripped = line.strip()
    return stripped.startswith("//") or stripped.startswith("/*") or stripped.startswith("*") or stripped.endswith("*/")


def has_attached_comment(lines: list[str], zero_based_line: int) -> bool:
    """Checks whether a declaration has a directly attached comment block."""

    index = zero_based_line - 1
    saw_comment = False
    while index >= 0:
        stripped = lines[index].strip()
        if not stripped:
            return saw_comment
        if is_comment_line(lines[index]):
            saw_comment = True
            index -= 1
            continue
        return False
    return saw_comment


def find_matching_header(source_path: Path) -> Path | None:
    """Returns a sibling header path if one exists."""

    for suffix in HEADER_SUFFIXES:
        candidate = source_path.with_suffix(suffix)
        if candidate.exists():
            return candidate
    return None


def parse_function_name(signature: str) -> str | None:
    """Extracts a function name from a declaration or definition."""

    match = re.search(r"([~A-Za-z_][\w:]*)\s*\(", signature)
    if not match:
        return None
    name = match.group(1)
    if name in CONTROL_KEYWORDS:
        return None
    return name.split("::")[-1]


def is_function_signature(signature: str) -> bool:
    """Checks if a statement looks like a function signature."""

    compact = " ".join(signature.split())
    if "(" not in compact or ")" not in compact:
        return False
    if compact.startswith(("using ", "typedef ", "return ", "namespace ")):
        return False
    name = parse_function_name(compact)
    if not name:
        return False
    if re.fullmatch(r"[A-Z][A-Z0-9_]*", name) and compact.startswith(f"{name}("):
        return False
    return True


def collect_header_candidates(header_path: Path) -> tuple[list[Candidate], set[str]]:
    """Collects documented declaration candidates from a header."""

    lines = header_path.read_text(encoding="utf-8").splitlines()
    clean_lines = strip_comments(lines)
    candidates: list[Candidate] = []
    declared_functions: set[str] = set()

    scope_stack: list[dict[str, object]] = []
    pending_scope: dict[str, object] | None = None
    pending_function: dict[str, object] | None = None

    def current_class_scope() -> dict[str, object] | None:
        for scope in reversed(scope_stack):
            if scope["kind"] in CLASS_KINDS:
                return scope
        return None

    def inside_anonymous_namespace() -> bool:
        return any(scope["kind"] == "namespace" and scope.get("anonymous", False) for scope in scope_stack)

    def push_scope(kind: str, name: str | None = None, anonymous: bool = False) -> None:
        if kind == "class":
            scope_stack.append({"kind": kind, "access": "private", "name": name})
        elif kind == "struct":
            scope_stack.append({"kind": kind, "access": "public", "name": name})
        elif kind == "namespace":
            scope_stack.append({"kind": kind, "anonymous": anonymous, "name": name})
        else:
            scope_stack.append({"kind": kind, "name": name})

    def pop_scope() -> None:
        if scope_stack:
            scope_stack.pop()

    for index, clean_line in enumerate(clean_lines):
        stripped = clean_line.strip()
        if not stripped:
            continue

        if stripped in {"public:", "private:", "protected:"}:
            scope = current_class_scope()
            if scope is not None:
                scope["access"] = stripped[:-1]
            continue

        if pending_scope and "{" in stripped:
            push_scope(
                pending_scope["kind"],
                pending_scope.get("name"),
                bool(pending_scope.get("anonymous", False)),
            )
            pending_scope = None

        namespace_match = re.match(r"^namespace(?:\s+([A-Za-z_]\w*))?\s*(?:\{|$)", stripped)
        compound_match = re.match(r"^(class|struct|enum(?:\s+class)?)\s+([A-Za-z_]\w*)\b", stripped)
        if namespace_match:
            name = namespace_match.group(1)
            anonymous_namespace = name is None
            if "{" in stripped:
                push_scope("namespace", name, anonymous_namespace)
            else:
                pending_scope = {"kind": "namespace", "name": name, "anonymous": anonymous_namespace}
            continue
        if compound_match:
            keyword = compound_match.group(1)
            name = compound_match.group(2)
            in_public_class = current_class_scope() is None or current_class_scope().get("access") == "public"
            if keyword.startswith("enum") or in_public_class:
                candidates.append(Candidate(index + 1, stripped, name))
            kind = keyword.split()[0]
            if "{" in stripped:
                push_scope(kind, name)
            else:
                pending_scope = {"kind": kind, "name": name}
            continue

        if pending_function is None and "(" in stripped and not stripped.startswith("#"):
            pending_function = {"start": index, "parts": [stripped]}
        elif pending_function is not None:
            pending_function["parts"].append(stripped)

        if pending_function is not None:
            signature = " ".join(pending_function["parts"])
            if ";" in stripped or "{" in stripped:
                if is_function_signature(signature):
                    scope = current_class_scope()
                    name = parse_function_name(signature)
                    if scope is None and not inside_anonymous_namespace():
                        if name:
                            declared_functions.add(name)
                            candidates.append(Candidate(pending_function["start"] + 1, signature, name))
                    elif scope is not None and scope.get("access") == "public":
                        if name:
                            candidates.append(Candidate(pending_function["start"] + 1, signature, name))
                pending_function = None

        closing_braces = stripped.count("}")
        for _ in range(closing_braces):
            pop_scope()

        generic_open_count = stripped.count("{")
        if compound_match or pending_scope:
            generic_open_count -= stripped.count("{")
        for _ in range(max(0, generic_open_count)):
            push_scope("block")

    return candidates, declared_functions


def collect_source_candidates(source_path: Path, header_function_names: set[str]) -> list[Candidate]:
    """Collects public free-function definitions from a source file."""

    lines = source_path.read_text(encoding="utf-8").splitlines()
    clean_lines = strip_comments(lines)
    candidates: list[Candidate] = []
    scope_stack: list[dict[str, object]] = []
    pending_scope: dict[str, object] | None = None
    pending_function: dict[str, object] | None = None

    def inside_anonymous_namespace() -> bool:
        return any(scope["kind"] == "namespace" and scope.get("anonymous", False) for scope in scope_stack)

    def inside_class_scope() -> bool:
        return any(scope["kind"] in CLASS_KINDS for scope in scope_stack)

    def push_scope(kind: str, anonymous: bool = False) -> None:
        scope_stack.append({"kind": kind, "anonymous": anonymous})

    def pop_scope() -> None:
        if scope_stack:
            scope_stack.pop()

    for index, clean_line in enumerate(clean_lines):
        stripped = clean_line.strip()
        if not stripped:
            continue

        if pending_scope and "{" in stripped:
            push_scope(pending_scope["kind"], bool(pending_scope.get("anonymous", False)))
            pending_scope = None

        namespace_match = re.match(r"^namespace(?:\s+([A-Za-z_]\w*))?\s*(?:\{|$)", stripped)
        compound_match = re.match(r"^(class|struct)\s+([A-Za-z_]\w*)\b", stripped)
        if namespace_match:
            anonymous_namespace = namespace_match.group(1) is None
            if "{" in stripped:
                push_scope("namespace", anonymous_namespace)
            else:
                pending_scope = {"kind": "namespace", "anonymous": anonymous_namespace}
            continue
        if compound_match:
            kind = compound_match.group(1)
            if "{" in stripped:
                push_scope(kind)
            else:
                pending_scope = {"kind": kind, "anonymous": False}
            continue

        if pending_function is None and "(" in stripped and not stripped.startswith("#"):
            pending_function = {"start": index, "parts": [stripped]}
        elif pending_function is not None:
            pending_function["parts"].append(stripped)

        if pending_function is not None:
            signature = " ".join(pending_function["parts"])
            if "{" in stripped:
                compact = " ".join(signature.split())
                name = parse_function_name(compact)
                if (
                    is_function_signature(compact)
                    and name
                    and name not in header_function_names
                    and "::" not in compact
                    and not inside_anonymous_namespace()
                    and not inside_class_scope()
                    and not compact.startswith("static ")
                ):
                    candidates.append(Candidate(pending_function["start"] + 1, compact, name))
                pending_function = None
            elif ";" in stripped:
                pending_function = None

        closing_braces = stripped.count("}")
        for _ in range(closing_braces):
            pop_scope()

        generic_open_count = stripped.count("{")
        if compound_match or pending_scope:
            generic_open_count -= stripped.count("{")
        for _ in range(max(0, generic_open_count)):
            push_scope("block")

    return candidates


def check_docs(repo_root: Path, mode: str, base_ref: str) -> int:
    """Runs the docs checker across new files."""

    targets = docs_targets(repo_root, mode, base_ref)
    if not targets:
        print("No new C++ files found for docs check.")
        return 0

    failures: list[str] = []
    header_function_names: dict[Path, set[str]] = {}

    for relative_path in targets:
        absolute_path = repo_root / relative_path
        lines = absolute_path.read_text(encoding="utf-8").splitlines()
        if not has_file_overview(lines):
            failures.append(f"{relative_path}:1: missing file overview comment")
        if relative_path.suffix.lower() in HEADER_SUFFIXES:
            candidates, declared_names = collect_header_candidates(absolute_path)
            header_function_names[absolute_path] = declared_names
            for candidate in candidates:
                if not has_attached_comment(lines, candidate.line_number - 1):
                    failures.append(
                        f"{relative_path}:{candidate.line_number}: missing Google-style comment for public API '{candidate.summary}'"
                    )

    for relative_path in targets:
        if relative_path.suffix.lower() not in SOURCE_SUFFIXES:
            continue
        absolute_path = repo_root / relative_path
        header_names: set[str] = set()
        matching_header = find_matching_header(absolute_path)
        if matching_header is not None:
            header_names = header_function_names.get(matching_header, set())
            if not header_names and matching_header.exists():
                _, header_names = collect_header_candidates(matching_header)

        lines = absolute_path.read_text(encoding="utf-8").splitlines()
        for candidate in collect_source_candidates(absolute_path, header_names):
            if not has_attached_comment(lines, candidate.line_number - 1):
                failures.append(
                    f"{relative_path}:{candidate.line_number}: missing Google-style comment for public free function '{candidate.summary}'"
                )

    if failures:
        print("Google-style C++ docs check failed:")
        for failure in failures:
            print(f"  {failure}")
        return 1

    print(f"Docs check passed for {len(targets)} new C++ file(s).")
    return 0


def ensure_tool(tool_name: str) -> None:
    """Fails clearly when a required tool is unavailable."""

    if shutil.which(tool_name) is None:
        raise SystemExit(f"Required tool not found in PATH: {tool_name}")


def run_clang_format(repo_root: Path, mode: str, base_ref: str, check_only: bool) -> int:
    """Runs clang-format against style targets."""

    ensure_tool("clang-format")
    targets = style_targets(repo_root, mode, base_ref)
    if not targets:
        print("No C++ files found for clang-format.")
        return 0

    args = ["clang-format"]
    if check_only:
        args.extend(["--dry-run", "--Werror"])
    else:
        args.append("-i")
    args.extend(str(repo_root / path) for path in targets)

    completed = subprocess.run(args, cwd=repo_root, text=True)
    if completed.returncode == 0:
        verb = "checked" if check_only else "formatted"
        print(f"clang-format {verb} {len(targets)} file(s).")
    return completed.returncode


def detect_default_triplet() -> str:
    """Returns the default vcpkg triplet for the current host."""

    if sys.platform == "darwin":
        return "arm64-osx" if os.uname().machine == "arm64" else "x64-osx"
    if sys.platform.startswith("linux"):
        return "arm64-linux" if os.uname().machine == "aarch64" else "x64-linux"
    return "x64-windows"


def resolve_orbbec_sdk_dir(repo_root: Path) -> Path | None:
    """Finds an Orbbec SDK root if available."""

    env_dir = os.environ.get("ORBBEC_SDK_DIR")
    if env_dir:
        env_path = Path(env_dir)
        if (env_path / "include/libobsensor/ObSensor.hpp").exists():
            return env_path

    extracted_root = repo_root / ".devenv/sdks/orbbec/extracted"
    if (extracted_root / "include/libobsensor/ObSensor.hpp").exists():
        return extracted_root

    for header_path in extracted_root.glob("**/include/libobsensor/ObSensor.hpp"):
        return header_path.parents[2]
    return None


def resolve_vcpkg_root(repo_root: Path) -> Path | None:
    """Finds a vcpkg root if available."""

    env_dir = os.environ.get("VCPKG_ROOT")
    if env_dir:
        env_path = Path(env_dir)
        if (env_path / "scripts/buildsystems/vcpkg.cmake").exists():
            return env_path

    local_root = repo_root / ".devenv/sdks/vcpkg"
    if (local_root / "scripts/buildsystems/vcpkg.cmake").exists():
        return local_root
    return None


def configure_build_dir(repo_root: Path, build_dir: Path) -> None:
    """Configures a compile_commands.json build for clang-tidy."""

    ensure_tool("cmake")
    args = [
        "cmake",
        "-S",
        str(repo_root),
        "-B",
        str(build_dir),
        "-DCMAKE_BUILD_TYPE=Debug",
        "-DBUILD_TESTING=ON",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
    ]

    if shutil.which("ninja"):
        args.extend(["-G", "Ninja"])

    sdk_dir = resolve_orbbec_sdk_dir(repo_root)
    vcpkg_root = resolve_vcpkg_root(repo_root)
    if sdk_dir is not None:
        args.extend(
            [
                f"-DORBBEC_SDK_DIR={sdk_dir}",
                f"-DCMAKE_PREFIX_PATH={sdk_dir};{sdk_dir / 'lib'}",
            ]
        )
        if vcpkg_root is not None:
            args.extend(
                [
                    f"-DCMAKE_TOOLCHAIN_FILE={vcpkg_root / 'scripts/buildsystems/vcpkg.cmake'}",
                    "-DVCPKG_MANIFEST_MODE=ON",
                    f"-DVCPKG_TARGET_TRIPLET={os.environ.get('VCPKG_TARGET_TRIPLET', detect_default_triplet())}",
                ]
            )
    else:
        args.append("-DSTM_BUILD_APP=OFF")

    completed = subprocess.run(args, cwd=repo_root, text=True)
    if completed.returncode != 0:
        raise SystemExit("Failed to configure build directory for clang-tidy.")


def compile_db_sources(build_dir: Path) -> set[Path]:
    """Loads compiled source paths from compile_commands.json."""

    compile_commands = build_dir / "compile_commands.json"
    if not compile_commands.exists():
        raise SystemExit(f"compile_commands.json was not generated in {build_dir}")

    with compile_commands.open(encoding="utf-8") as handle:
        entries = json.load(handle)

    sources = set()
    for entry in entries:
        file_path = Path(entry["file"])
        if not file_path.is_absolute():
            file_path = (Path(entry["directory"]) / file_path).resolve()
        sources.add(file_path)
    return sources


def path_regex(path: Path) -> str:
    """Builds a cross-platform regex fragment for one absolute path."""

    fragments: list[str] = []
    for character in str(path.resolve()):
        if character in {"/", "\\"}:
            fragments.append(r"[\\/]")
        else:
            fragments.append(re.escape(character))
    return "".join(fragments)


def clang_tidy_header_filter(repo_root: Path, targets: list[Path]) -> str:
    """Builds a header filter regex limited to the selected header targets."""

    header_targets = [
        repo_root / relative_path for relative_path in targets if relative_path.suffix.lower() in HEADER_SUFFIXES
    ]
    if not header_targets:
        return r"$^"
    return f"^({'|'.join(path_regex(path) for path in header_targets)})$"


def run_clang_tidy(repo_root: Path, mode: str, base_ref: str, build_dir: Path) -> int:
    """Runs clang-tidy on selected translation units."""

    ensure_tool("clang-tidy")
    targets = style_targets(repo_root, mode, base_ref)
    source_targets = [repo_root / path for path in targets if path.suffix.lower() in SOURCE_SUFFIXES]
    if not source_targets:
        print("No translation units selected for clang-tidy.")
        return 0

    configure_build_dir(repo_root, build_dir)
    available_sources = compile_db_sources(build_dir)
    header_filter = clang_tidy_header_filter(repo_root, targets)
    missing_sources = [path for path in source_targets if path.resolve() not in available_sources]
    if missing_sources:
        print("clang-tidy targets missing from compile_commands.json:")
        for path in missing_sources:
            print(f"  {path.relative_to(repo_root)}")
        return 1

    return_code = 0
    for source_path in source_targets:
        completed = subprocess.run(
            [
                "clang-tidy",
                "-p",
                str(build_dir),
                f"-header-filter={header_filter}",
                "--warnings-as-errors=*",
                str(source_path),
            ],
            cwd=repo_root,
            text=True,
        )
        if completed.returncode != 0:
            return_code = completed.returncode
    if return_code == 0:
        print(f"clang-tidy passed for {len(source_targets)} translation unit(s).")
    return return_code


def list_targets(repo_root: Path, mode: str, base_ref: str) -> int:
    """Prints style targets for inspection."""

    targets = style_targets(repo_root, mode, base_ref)
    for target in targets:
        print(target.as_posix())
    return 0


def parse_args() -> argparse.Namespace:
    """Builds the command-line parser."""

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "command",
        choices=["list-targets", "check-docs", "format", "check-format", "tidy", "lint"],
    )
    parser.add_argument("--mode", choices=["local", "ci"], default="local")
    parser.add_argument("--base-ref", default="main")
    parser.add_argument("--repo-root", default=".")
    parser.add_argument("--build-dir", default="build-style")
    return parser.parse_args()


def main() -> int:
    """Executes the requested style command."""

    args = parse_args()
    repo_root = Path(args.repo_root).resolve()
    build_dir = (repo_root / args.build_dir).resolve()

    if args.command == "list-targets":
        return list_targets(repo_root, args.mode, args.base_ref)
    if args.command == "check-docs":
        return check_docs(repo_root, args.mode, args.base_ref)
    if args.command == "format":
        return run_clang_format(repo_root, args.mode, args.base_ref, check_only=False)
    if args.command == "check-format":
        return run_clang_format(repo_root, args.mode, args.base_ref, check_only=True)
    if args.command == "tidy":
        return run_clang_tidy(repo_root, args.mode, args.base_ref, build_dir)
    if args.command == "lint":
        if check_docs(repo_root, args.mode, args.base_ref) != 0:
            return 1
        if run_clang_format(repo_root, args.mode, args.base_ref, check_only=True) != 0:
            return 1
        return run_clang_tidy(repo_root, args.mode, args.base_ref, build_dir)
    raise SystemExit(f"Unsupported command: {args.command}")


if __name__ == "__main__":
    raise SystemExit(main())
