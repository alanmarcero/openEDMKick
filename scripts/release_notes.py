"""Build the GitHub release body for a version tag.

Use the matching CHANGELOG section when the repo has one. Otherwise use the
annotated tag message, or the commit message when the tag is lightweight.
The editor screenshot and the compare link are appended after those notes.
"""

from __future__ import annotations

import os
import re
import subprocess
import sys
from pathlib import Path

HEADING = re.compile(r"^(#{1,6})\s+(.*?)\s*$")
DATE_SUFFIX = re.compile(r"\s+[–—-]\s+\d{4}-\d{2}-\d{2}\s*$")
MARKDOWN_LINK = re.compile(r"^\[([^\]]+)\]\([^)]*\)$")
VERSION_TOKEN = re.compile(r"^v?\d+\.\d+\.\d+(?:[-+][0-9A-Za-z.-]+)?$")
TRAILER = re.compile(
    r"^(?:Co-authored-by|Signed-off-by|Reviewed-by|Acked-by|Helped-by|Tested-by|Reported-by|Suggested-by): .+$",
    re.IGNORECASE,
)
CHANGELOG_NAMES = ("CHANGELOG.md", "CHANGELOG.markdown", "CHANGELOG")


def heading_tag(title: str) -> str | None:
    title = DATE_SUFFIX.sub("", title.strip()).strip()
    link = MARKDOWN_LINK.fullmatch(title)
    if link:
        title = link.group(1).strip()
    if len(title) >= 2 and title[0] == "[" and title[-1] == "]":
        title = title[1:-1].strip()
    if not VERSION_TOKEN.fullmatch(title):
        return None
    return title if title.startswith("v") else f"v{title}"


def extract_changelog_section(text: str, tag: str) -> str:
    """Return the body of the changelog heading for tag, without the heading."""
    wanted = heading_tag(tag)
    if wanted is None or not text:
        return ""
    lines = text.replace("\r\n", "\n").replace("\r", "\n").split("\n")
    start = None
    level = None
    for index, line in enumerate(lines):
        match = HEADING.match(line)
        if match is None:
            continue
        if heading_tag(match.group(2)) != wanted:
            continue
        start = index + 1
        level = len(match.group(1))
        break
    if start is None or level is None:
        return ""
    end = len(lines)
    for index in range(start, len(lines)):
        match = HEADING.match(lines[index])
        if match is not None and len(match.group(1)) <= level:
            end = index
            break
    return "\n".join(lines[start:end]).strip()


def clean_tag_message(message: str) -> str:
    """Drop a trailing git trailer block such as Co-authored-by."""
    lines = message.replace("\r\n", "\n").replace("\r", "\n").strip().split("\n")
    while lines and not lines[-1].strip():
        lines.pop()
    end = len(lines)
    saw_trailer = False
    for index in range(len(lines) - 1, -1, -1):
        line = lines[index].strip()
        if line == "":
            if saw_trailer:
                end = index
            break
        if TRAILER.match(line):
            saw_trailer = True
            end = index
            continue
        break
    return "\n".join(lines[:end]).strip()


def notes_for(changelog_text: str, tag: str, tag_message: str) -> str:
    section = extract_changelog_section(changelog_text, tag)
    if section:
        return section
    return clean_tag_message(tag_message)


def version_key(tag: str) -> tuple[int, int, int, int, str] | None:
    match = re.fullmatch(r"v(\d+)\.(\d+)\.(\d+)(?:-([0-9A-Za-z.-]+))?", tag)
    if match is None:
        return None
    pre = match.group(4)
    return (int(match.group(1)), int(match.group(2)), int(match.group(3)), 1 if pre is None else 0, pre or "")


def previous_version(tag: str, tags: list[str]) -> str | None:
    current = version_key(tag)
    if current is None:
        return None
    earlier = [item for item in tags if item != tag and (key := version_key(item)) is not None and key < current]
    if not earlier:
        return None
    return max(earlier, key=version_key)


def render_body(notes: str, repo: str, tag: str, previous: str | None) -> str:
    screenshot = (
        f"![Native editor](https://raw.githubusercontent.com/{repo}/{tag}/docs/screenshots/preview.png)"
    )
    parts = [notes.strip(), "", screenshot]
    if previous:
        parts.extend(["", f"**Full Changelog**: https://github.com/{repo}/compare/{previous}...{tag}"])
    return "\n".join(parts) + "\n"


def _git(args: list[str]) -> str:
    result = subprocess.run(["git", *args], check=True, text=True, capture_output=True)
    return result.stdout


def read_changelog(root: Path) -> str:
    for name in CHANGELOG_NAMES:
        path = root / name
        if path.is_file():
            return path.read_text(encoding="utf-8")
    return ""


def read_tag_message(tag: str) -> str:
    message = _git(["for-each-ref", f"refs/tags/{tag}", "--format=%(contents)"])
    if message.strip():
        return message
    return _git(["log", "-1", "--format=%B", tag])


def merged_tags(tag: str) -> list[str]:
    return [line.strip() for line in _git(["tag", "--merged", tag]).splitlines() if line.strip()]


def main(argv: list[str]) -> int:
    tag = argv[1] if len(argv) > 1 else os.environ.get("GITHUB_REF_NAME", "")
    repo = os.environ.get("GITHUB_REPOSITORY", "")
    if not tag or not repo:
        print("Usage: GITHUB_REPOSITORY=owner/name python3 scripts/release_notes.py <tag>", file=sys.stderr)
        return 1
    root = Path(_git(["rev-parse", "--show-toplevel"]).strip())
    notes = notes_for(read_changelog(root), tag, read_tag_message(tag))
    if not notes.strip():
        print(
            f"No release notes for {tag}. Add a CHANGELOG section for {tag}, or annotate the tag.",
            file=sys.stderr,
        )
        return 1
    previous = previous_version(tag, merged_tags(tag))
    if previous is None and remote_has_earlier_tag(tag):
        print(
            f"No previous version tag is available locally for {tag}. Fetch tags before publishing.",
            file=sys.stderr,
        )
        return 1
    sys.stdout.write(render_body(notes, repo, tag, previous))
    return 0


def remote_has_earlier_tag(tag: str) -> bool:
    current = version_key(tag)
    if current is None:
        return False
    try:
        output = _git(["ls-remote", "--tags", "--refs", "origin"])
    except subprocess.CalledProcessError:
        return False
    for line in output.splitlines():
        name = line.rsplit("refs/tags/", 1)[-1].strip()
        key = version_key(name)
        if key is not None and name != tag and key < current:
            return True
    return False


if __name__ == "__main__":
    sys.exit(main(sys.argv))
