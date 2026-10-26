#!/usr/bin/env python3
"""Generate nightly release notes from the commit's Git history."""

import argparse
import html
import re
import subprocess
from pathlib import Path
from urllib.parse import urlsplit


def git(repo, *args):
    return subprocess.check_output(
        ["git", "-C", str(repo), *args], text=True, encoding="utf-8"
    ).strip()


def resolve_commit(repo, revision):
    target = git(repo, "rev-parse", "--verify", "--end-of-options", revision)
    return git(repo, "rev-list", "-1", target, "--")


def previous_nightly(repo, commit):
    """Find a correctly tagged prior nightly on the first-parent history."""
    tags = git(repo, "for-each-ref", "--format=%(refname:short) %(objectname) %(*objectname)",
               "refs/tags/nightly-*")
    published = set()
    for line in tags.splitlines():
        fields = line.split()
        if len(fields) not in (2, 3):
            continue
        tag, target = fields[0], fields[-1]
        if re.fullmatch(r"nightly-[0-9a-f]{40}", tag) and tag[8:] == target:
            published.add(target)
    if published:
        for ancestor in git(repo, "rev-list", "--first-parent", commit).splitlines()[1:]:
            if ancestor in published:
                return ancestor
    return None


def escape_subject(subject):
    text = html.escape(subject, quote=False).replace("\t", " ")
    return re.sub(r"([\\`*_{}\[\]()!])", r"\\\1", text)


def generate_notes(repo, revision="HEAD", repository_url=None, provider="github"):
    commit = resolve_commit(repo, revision)
    base = previous_nightly(repo, commit)
    if repository_url:
        if urlsplit(repository_url).scheme not in ("http", "https"):
            raise ValueError("repository URL must use HTTP or HTTPS")
        repository_url = repository_url.rstrip("/")

    def link(sha, full=False):
        label = sha if full else sha[:12]
        if repository_url:
            segment = "/-/commit/" if provider == "gitlab" else "/commit/"
            return f"[{label}]({repository_url}{segment}{sha})"
        return f"`{label}`"

    lines = [f"# Nightly {commit[:12]}", "", f"Commit: {link(commit, full=True)}", ""]
    lines.append(f"Changes since {link(base)}:" if base else "Commit history:")
    lines.append("")
    history = git(repo, "log", "--format=%H%x09%s", f"{base}..{commit}" if base else commit)
    for entry in history.splitlines():
        sha, subject = entry.split("\t", 1)
        lines.append(f"- {link(sha)} {escape_subject(subject)}")
    return "\n".join(lines) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=Path("."))
    parser.add_argument("--commit", default="HEAD")
    parser.add_argument("--repository-url")
    parser.add_argument("--provider", choices=("github", "gitlab"), default="github")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        notes = generate_notes(args.repo, args.commit, args.repository_url, args.provider)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(notes, encoding="utf-8")
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"Cannot generate nightly notes: {error}\n")


if __name__ == "__main__":
    main()
