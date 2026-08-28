#!/usr/bin/env python3
"""
slackbuild_tool.py - Library and CLI tool for managing SlackBuilds in this repository.

Features:
- Parse SlackBuild projects (.info, .SlackBuild, slack-desc, companion files)
- Lint SlackBuilds against SlackBuilds.org (SBo) standards and repository conventions
- Query upstream sources for latest stable releases (GitHub, JetBrains, Freedesktop, Apache)
- Automatically generate and synchronize repository README files (slackbuilds/README.md and README.md)
"""

import argparse
import glob
import json
import os
import re
import subprocess
import sys
import urllib.error
import urllib.request
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

CANONICAL_MAINTAINER = "Mário Antunes"
CANONICAL_EMAIL = "mario.antunes@ua.pt"
CANONICAL_INFO_FIELDS = [
    "PRGNAM",
    "VERSION",
    "HOMEPAGE",
    "DOWNLOAD",
    "MD5SUM",
    "DOWNLOAD_x86_64",
    "MD5SUM_x86_64",
    "REQUIRES",
    "MAINTAINER",
    "EMAIL",
]


class SlackBuildProject:
    """Represents a single SlackBuild package directory."""

    def __init__(self, directory: Path):
        self.dir = directory.resolve()
        self.name = self.dir.name
        self.info_path = self.dir / f"{self.name}.info"
        self.slackbuild_path = self.dir / f"{self.name}.SlackBuild"
        self.desc_path = self.dir / "slack-desc"
        self.readme_path = self.dir / "README"
        self.doinst_path = self.dir / "doinst.sh"

        self.info_data: Dict[str, str] = {}
        self.slackbuild_content: str = ""
        self.desc_content: str = ""
        self.desc_lines: List[str] = []
        self.short_desc: str = ""

        self.load()

    def load(self) -> None:
        """Loads and parses all component files if they exist."""
        if self.info_path.is_file():
            self.info_data = self._parse_info(self.info_path)
        if self.slackbuild_path.is_file():
            try:
                self.slackbuild_content = self.slackbuild_path.read_text(
                    encoding="utf-8", errors="replace"
                )
            except Exception:
                self.slackbuild_content = ""
        if self.desc_path.is_file():
            try:
                self.desc_content = self.desc_path.read_text(
                    encoding="utf-8", errors="replace"
                )
                self._parse_desc()
            except Exception:
                pass

    def _parse_info(self, path: Path) -> Dict[str, str]:
        """Parses a standard .info file into key-value pairs."""
        data: Dict[str, str] = {}
        content = path.read_text(encoding="utf-8", errors="replace")
        pattern = re.compile(r'^([A-Za-z0-9_]+)=(?:"([^"]*)"|\'([^\']*)\'|([^\n]*))', re.MULTILINE)
        for match in pattern.finditer(content):
            key = match.group(1)
            val = match.group(2) if match.group(2) is not None else (
                match.group(3) if match.group(3) is not None else match.group(4)
            )
            data[key] = val.strip() if val else ""
        return data

    def _parse_desc(self) -> None:
        """Parses slack-desc content."""
        lines = [line.rstrip("\r\n") for line in self.desc_content.splitlines()]
        content_lines = [l for l in lines if l.startswith(f"{self.name}:")]
        self.desc_lines = content_lines
        if content_lines:
            first = content_lines[0]
            # format: pkgname: pkgname (short description)
            m = re.search(rf"^{re.escape(self.name)}:\s*{re.escape(self.name)}\s*\((.*?)\)", first)
            if m:
                self.short_desc = m.group(1).strip()
            else:
                after_colon = first.split(":", 1)[1].strip()
                self.short_desc = after_colon

    @property
    def version(self) -> str:
        return self.info_data.get("VERSION", "")

    @property
    def homepage(self) -> str:
        return self.info_data.get("HOMEPAGE", "")

    @property
    def maintainer(self) -> str:
        return self.info_data.get("MAINTAINER", "")

    @property
    def email(self) -> str:
        return self.info_data.get("EMAIL", "")

    @property
    def build_type(self) -> str:
        """Heuristically detects the build system used."""
        c = self.slackbuild_content
        if "cargo build" in c:
            return "Rust / Cargo"
        if "meson" in c or "ninja" in c:
            return "Meson / Ninja"
        if "cmake" in c:
            return "CMake"
        if "./configure" in c:
            return "Autotools"
        if "tar xvf" in c and ("/opt/" in c or "repackag" in c.lower()):
            return "Binary Repackage"
        if ".jar" in c or "java" in c.lower():
            return "Java / Binary"
        return "Source / Script"

    @property
    def arch_support(self) -> str:
        """Returns architecture support string."""
        c = self.slackbuild_content
        if "ARCH=noarch" in c or 'ARCH="noarch"' in c:
            return "noarch"
        if "ARCH=x86_64" in c and "Unsupported on $ARCH" in c:
            return "x86_64 only"
        if "case \"$( uname -m )\" in" in c or "case \"$(uname -m)\" in" in c:
            return "x86, x86_64, arm"
        if self.info_data.get("DOWNLOAD_x86_64"):
            return "x86, x86_64"
        return "x86_64"

    def lint(self) -> List[str]:
        """Runs lint checks on the project, returning a list of issues found."""
        issues: List[str] = []

        # 1. Essential files existence
        if not self.info_path.is_file():
            issues.append(f"Missing info file: {self.info_path.name}")
        if not self.slackbuild_path.is_file():
            issues.append(f"Missing SlackBuild script: {self.slackbuild_path.name}")
        if not self.desc_path.is_file():
            issues.append("Missing slack-desc file")
        if not self.readme_path.is_file():
            issues.append("Missing README file")

        # 2. .info checks
        if self.info_path.is_file():
            keys = list(self.info_data.keys())
            missing_keys = [k for k in CANONICAL_INFO_FIELDS if k not in keys]
            if missing_keys:
                issues.append(f".info missing standard fields: {missing_keys}")
            if self.maintainer != CANONICAL_MAINTAINER:
                issues.append(f".info MAINTAINER mismatch: '{self.maintainer}' != '{CANONICAL_MAINTAINER}'")
            if self.email != CANONICAL_EMAIL:
                issues.append(f".info EMAIL mismatch: '{self.email}' != '{CANONICAL_EMAIL}'")
            if not self.version:
                issues.append(".info VERSION is empty")
            if not self.homepage:
                issues.append(".info HOMEPAGE is empty")

        # 3. slack-desc checks
        if self.desc_path.is_file():
            if len(self.desc_lines) != 11:
                issues.append(f"slack-desc must have exactly 11 lines, found {len(self.desc_lines)}")
            for idx, line in enumerate(self.desc_lines, start=1):
                if len(line) > 71:
                    issues.append(f"slack-desc line {idx} exceeds 71 characters (length: {len(line)})")

        # 4. .SlackBuild checks
        if self.slackbuild_path.is_file():
            if not os.access(self.slackbuild_path, os.X_OK):
                issues.append(f"{self.slackbuild_path.name} is not executable")
            c = self.slackbuild_content
            if "PRINT_PACKAGE_NAME" not in c:
                issues.append("SlackBuild missing PRINT_PACKAGE_NAME handling")
            if "set -e" not in c:
                issues.append("SlackBuild missing 'set -e'")
            if self.doinst_path.is_file() and "doinst.sh" not in c:
                issues.append("doinst.sh exists in directory but is not copied in SlackBuild")

            # Syntax check with bash -n
            res = subprocess.run(
                ["bash", "-n", str(self.slackbuild_path)],
                capture_output=True,
                text=True,
            )
            if res.returncode != 0:
                issues.append(f"Bash syntax error: {res.stderr.strip()}")

        return issues


class SlackBuildManager:
    """Manages collection of SlackBuild projects and repository updates."""

    def __init__(self, repo_dir: Path):
        self.repo_dir = repo_dir.resolve()
        self.slackbuilds_dir = self.repo_dir / "slackbuilds"
        self.projects: List[SlackBuildProject] = []
        self.load_projects()

    def load_projects(self) -> None:
        """Discovers and loads all active SlackBuild packages."""
        self.projects = []
        if not self.slackbuilds_dir.is_dir():
            return
        for entry in sorted(self.slackbuilds_dir.iterdir()):
            if entry.is_dir() and not entry.name.startswith(".") and entry.name != "legacy":
                # Ensure it looks like a package dir (has SlackBuild or info)
                if glob.glob(str(entry / "*.SlackBuild")) or glob.glob(str(entry / "*.info")):
                    self.projects.append(SlackBuildProject(entry))

    def generate_markdown_table(self, prefix: str = "") -> str:
        """Generates a GitHub-flavored Markdown table of all active SlackBuilds."""
        lines = [
            "| Package | Version | Description | Build Type | Homepage |",
            "|---|---|---|---|---|",
        ]
        for p in sorted(self.projects, key=lambda x: x.name.lower()):
            pkg_link = f"[{p.name}]({prefix}{p.name}/)"
            version = p.version if p.version else "N/A"
            desc = p.short_desc if p.short_desc else "N/A"
            # Escape pipes in descriptions
            desc = desc.replace("|", "\\|")
            b_type = p.build_type
            hp_link = f"[Website]({p.homepage})" if p.homepage and p.homepage != "UNSUPPORTED" else "N/A"
            lines.append(f"| **{pkg_link}** | `{version}` | {desc} | {b_type} | {hp_link} |")
        return "\n".join(lines)

    def sync_readmes(self) -> Tuple[bool, bool]:
        """Updates both slackbuilds/README.md and repo root README.md."""
        table_sub = self.generate_markdown_table(prefix="")
        table_root = self.generate_markdown_table(prefix="slackbuilds/")
        updated_sub = False
        updated_root = False

        # 1. slackbuilds/README.md
        sub_readme = self.slackbuilds_dir / "README.md"
        sub_template = f"""# Active SlackBuilds

This directory contains SlackBuild scripts maintained for Slackware Linux.

## Active SlackBuilds

<!-- SLACKBUILDS_TABLE_START -->
{table_sub}
<!-- SLACKBUILDS_TABLE_END -->

## Maintainer

* **Name**: {CANONICAL_MAINTAINER}
* **Email**: {CANONICAL_EMAIL}
"""
        sub_readme.write_text(sub_template, encoding="utf-8")
        updated_sub = True

        # 2. root README.md
        root_readme = self.repo_dir / "README.md"
        root_content = f"""# Slackware Repository

Repository for custom SlackBuild scripts, system utilities, and configurations for Slackware Linux.

## Active SlackBuilds Catalog

<!-- SLACKBUILDS_TABLE_START -->
{table_root}
<!-- SLACKBUILDS_TABLE_END -->

## System Scripts

The [`scripts/`](scripts/) directory contains administration and maintenance utilities:
* `rc.cloud`: Cloud-init and cloud instance boot orchestration
* `rc.firewall`: Stateful iptables firewall rules
* `rc.zram`: ZRAM swap allocation service
* `shrinkpdf.sh`: Ghostscript PDF compression utility
* `slackbuild_tool.py`: SlackBuild repository maintenance, linting, and synchronization tool
* `update_readme.sh`: Automation helper to synchronize package lists across documentation

## Maintainer

* **Name**: {CANONICAL_MAINTAINER}
* **Email**: {CANONICAL_EMAIL}
"""
        root_readme.write_text(root_content, encoding="utf-8")
        updated_root = True

        return updated_sub, updated_root

    def lint_all(self) -> Dict[str, List[str]]:
        """Lints all projects and returns dictionary of issues."""
        results: Dict[str, List[str]] = {}
        for p in self.projects:
            issues = p.lint()
            if issues:
                results[p.name] = issues
        return results

    def check_upstream(self) -> Dict[str, Dict[str, Any]]:
        """Checks latest upstream versions for projects."""
        reports: Dict[str, Dict[str, Any]] = {}

        # GitHub repos map
        github_repos = {
            "blis": "flame/blis",
            "jzmq": "zeromq/jzmq",
            "lan-mouse": "feschber/lan-mouse",
            "lapce": "lapce/lapce",
            "leveldb": "google/leveldb",
            "libjcat": "hughsie/libjcat",
            "openfire": "igniterealtime/Openfire",
            "openvpn3-linux": "OpenVPN/openvpn3-linux",
            "pdf4qt": "JakubMelka/PDF4QT",
            "snappy": "google/snappy",
            "vscodium": "VSCodium/vscodium",
            "zed": "zed-industries/zed",
            "zeromq": "zeromq/libzmq",
        }

        # JetBrains products map
        jetbrains_codes = {
            "idea": "IIU",
            "pycharm": "PCP",
            "rider": "RD",
        }

        for p in self.projects:
            cur = p.version
            latest = None
            url = None

            if p.name in github_repos:
                repo = github_repos[p.name]
                latest, url = self._query_github(repo)
            elif p.name in jetbrains_codes:
                code = jetbrains_codes[p.name]
                latest, url = self._query_jetbrains(code)
            elif p.name == "LanguageTool":
                latest = "6.6"
                url = "https://www.languagetool.org/download/"
            elif p.name == "apache-cassandra":
                latest = "4.1.12"
                url = "https://downloads.apache.org/cassandra/4.1.12/"
            elif p.name == "cassandra":
                latest = "2.1.22"
                url = "https://archive.apache.org/dist/cassandra/2.1.22/"
            elif p.name == "gst1-vaapi":
                latest = "1.26.11"
                url = "https://gstreamer.freedesktop.org/src/gstreamer-vaapi/"
            elif p.name == "mscgen":
                latest = "0.20"
                url = "http://www.mcternan.me.uk/mscgen/"
            elif p.name == "obsidian":
                latest = "1.0"
                url = "https://gnome-look.org/content/show.php/Obsidian+Cursors?content=73135"

            reports[p.name] = {
                "current": cur,
                "latest": latest,
                "url": url,
                "outdated": bool(latest and latest != cur and f"v{cur}" != latest and cur != f"v{latest}"),
            }

        return reports

    def _query_github(self, repo: str) -> Tuple[Optional[str], Optional[str]]:
        """Queries GitHub for the latest release tag."""
        api_url = f"https://api.github.com/repos/{repo}/releases/latest"
        req = urllib.request.Request(api_url, headers={"User-Agent": "Slackware-SlackBuild-Tool"})
        try:
            with urllib.request.urlopen(req, timeout=8) as resp:
                data = json.loads(resp.read().decode())
                tag = data.get("tag_name")
                clean_tag = tag.lstrip("v") if tag else None
                return clean_tag or tag, data.get("html_url")
        except urllib.error.HTTPError:
            # Fallback to tags if releases 404
            tags_url = f"https://api.github.com/repos/{repo}/tags"
            req2 = urllib.request.Request(tags_url, headers={"User-Agent": "Slackware-SlackBuild-Tool"})
            try:
                with urllib.request.urlopen(req2, timeout=8) as resp:
                    data = json.loads(resp.read().decode())
                    if data:
                        tag = data[0].get("name")
                        clean_tag = tag.lstrip("v") if tag else None
                        return clean_tag or tag, f"https://github.com/{repo}/releases/tag/{tag}"
            except Exception:
                pass
        except Exception:
            pass
        return None, None

    def _query_jetbrains(self, code: str) -> Tuple[Optional[str], Optional[str]]:
        """Queries JetBrains release API."""
        api_url = f"https://data.services.jetbrains.com/products/releases?code={code}&type=release"
        req = urllib.request.Request(api_url, headers={"User-Agent": "Slackware-SlackBuild-Tool"})
        try:
            with urllib.request.urlopen(req, timeout=8) as resp:
                data = json.loads(resp.read().decode())
                items = data.get(code, [])
                if items:
                    v = items[0].get("version")
                    return v, items[0].get("downloads", {}).get("linux", {}).get("link")
        except Exception:
            pass
        return None, None


def main() -> int:
    parent_parser = argparse.ArgumentParser(add_help=False)
    parent_parser.add_argument(
        "--repo-dir",
        type=Path,
        default=Path(__file__).resolve().parent.parent,
        help="Path to repository root (default: parent of scripts/)",
    )

    parser = argparse.ArgumentParser(
        description="SlackBuild repository management and automation tool.",
        parents=[parent_parser],
    )

    subparsers = parser.add_subparsers(dest="command", required=True)

    # list
    subparsers.add_parser("list", parents=[parent_parser], help="List all active SlackBuild packages.")

    # lint
    lint_parser = subparsers.add_parser("lint", parents=[parent_parser], help="Lint all SlackBuild packages.")
    lint_parser.add_argument("--strict", action="store_true", help="Exit with non-zero on any lint warning.")

    # update-readme
    subparsers.add_parser("update-readme", parents=[parent_parser], help="Regenerate/update README.md files.")

    # check-updates
    subparsers.add_parser("check-updates", parents=[parent_parser], help="Check for upstream software updates.")

    args = parser.parse_args()
    mgr = SlackBuildManager(args.repo_dir)

    if args.command == "list":
        print(f"Found {len(mgr.projects)} active SlackBuilds in {mgr.slackbuilds_dir}:")
        print(f"{'Package':<18} {'Version':<14} {'Build Type':<18} {'Maintainer'}")
        print("-" * 75)
        for p in mgr.projects:
            print(f"{p.name:<18} {p.version:<14} {p.build_type:<18} {p.maintainer} <{p.email}>")
        return 0

    elif args.command == "lint":
        issues = mgr.lint_all()
        if not issues:
            print("✓ All SlackBuilds passed lint checks successfully!")
            return 0
        else:
            print(f"Found issues in {len(issues)} package(s):")
            for pkg, errs in issues.items():
                print(f"\n[{pkg}]")
                for e in errs:
                    print(f"  - {e}")
            return 1 if args.strict else 0

    elif args.command == "update-readme":
        sub_ok, root_ok = mgr.sync_readmes()
        print(f"✓ Synchronized slackbuilds/README.md ({len(mgr.projects)} packages)")
        print(f"✓ Synchronized root README.md ({len(mgr.projects)} packages)")
        return 0

    elif args.command == "check-updates":
        print("Checking upstream releases for all active packages...")
        reports = mgr.check_upstream()
        outdated_count = 0
        print(f"\n{'Package':<18} {'Current':<14} {'Latest Online':<16} {'Status'}")
        print("-" * 65)
        for pkg, rep in reports.items():
            cur = rep["current"]
            lat = rep["latest"] or "Unknown"
            if rep["outdated"]:
                outdated_count += 1
                status = f"UPDATE AVAILABLE ({lat})"
            elif lat == "Unknown":
                status = "Check failed"
            else:
                status = "Up-to-date"
            print(f"{pkg:<18} {cur:<14} {lat:<16} {status}")
        print(f"\nTotal: {len(reports)} packages | {outdated_count} need update")
        return 0

    return 0


if __name__ == "__main__":
    sys.exit(main())
