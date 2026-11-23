#!/usr/bin/env python3
"""Builds the branch `pd`: one flat folder with everything Pd needs, for git pull.

usage: python3 common/tools/dist.py [--push]      (make dist runs make mac-zig bela-gem first)

Collects from main: every instrument's Mac package (instruments/*/bin/macos: externals as
.pd_darwin, abstractions, help patches, editors, presets), the Bela Gem externals
(build/bela-gem/*.pd_linux) and the effects (effects/vrs23). Pd takes the binary that fits:
.pd_darwin on the Mac, .pd_linux on the Bela Gem. User banks, ROMs and nvram files are not
in it and are listed in its .gitignore, so they can lie in the same folder and git pull never
touches them.

The result is committed onto the branch `pd` (worktree build/pd-branch), one commit per run
that changed something; --push pushes it.
"""
import filecmp
import os
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.normpath(os.path.join(HERE, "..", ".."))
WT = os.path.join(REPO, "build", "pd-branch")
BRANCH = "pd"
INSTRUMENTS = ["juno", "d50", "cz4", "pdoly8", "tx81z"]
SKIP = {"test", "LIESMICH.txt", "LIESMICH-juno.txt", "README.md", "cz4test.pd"}
SKIP_SUFFIX = ("user-bank.txt", "user-bank.syx", ".nvram")

IGNORE = """# your own files beside the externals: git pull never touches them
*user-bank.*
*.nvram
d50_pcm.bin
tx81z-v1.6.bin
*.rom
*.bin
.DS_Store
"""

README = """# bela-workstation for Pd

The instruments and effects of [bela-workstation](https://github.com/maltepeter2000/bela-workstation)
ready to use: one folder for Pd on the Mac (`.pd_darwin`, universal) and on the Bela Gem
(`.pd_linux`, aarch64). Made from main by `make dist`; do not edit here.

## Install

Mac:

    cd ~/Documents/Pd/externals
    git clone -b pd --single-branch https://github.com/maltepeter2000/bela-workstation.git bela-workstation

then add `~/Documents/Pd/externals/bela-workstation` once under Pd's Preferences -> Path.

Bela Gem (with network; else clone on the Mac and copy the folder, see `bela-sync.sh`):

    cd /root/Bela/projects/pd-externals
    git clone -b pd --single-branch https://github.com/maltepeter2000/bela-workstation.git bela-workstation

and in the project's `_main.pd`: `[declare -path ../pd-externals/bela-workstation]`. Ready
projects that do this are in `bela-projects/` (juno, tx81zn-pd, tx81z-pd, and rack4: juno,
d50, cz4 and pdoly8 at once on channels 1-4): copy one to
`/root/Bela/projects/` and run it (tx81z needs its ROM `tx81z-v1.6.bin` in the project folder).

## Update

    git pull

Your user banks (`*user-bank.*`), the ROMs (`d50_pcm.bin`, `tx81z-v1.6.bin`) and `*.nvram`
go into this folder too; they are not in git, so `git pull` never touches them.

## Contents

{contents}
"""

SYNC = r"""#!/bin/sh
# Copies this folder from the Mac to the Bela Gem (when the Bela has no network for git pull).
# usage: sh bela-sync.sh [host]        (default: root@bela-multi.local)
# Your files on the Bela (user banks, ROMs, nvram) stay: rsync only adds and updates.
HOST=${1:-root@bela-multi.local}
DIR=/root/Bela/projects/pd-externals/bela-workstation
cd "$(dirname "$0")" || exit 1
ssh "$HOST" "mkdir -p $DIR" || exit 1
if ssh "$HOST" "command -v rsync" >/dev/null 2>&1; then
  rsync -av --exclude .git --exclude '*.pd_darwin' ./ "$HOST:$DIR/"
else          # no rsync on the board: copy what is in git (your files there stay)
  git ls-files | grep -v '\.pd_darwin$' | tar cf - -T - | ssh "$HOST" "tar xf - -C $DIR"
fi
"""


def run(*cmd, cwd=REPO, check=True):
    return subprocess.run(cmd, cwd=cwd, check=check, capture_output=True, text=True).stdout.strip()


def collect():
    """{name: source path}; the same name twice must be the same file"""
    files = {}

    def put(name, src):
        if name in files and not (os.path.isdir(src) or filecmp.cmp(files[name], src, shallow=False)):
            sys.exit(f"dist: {name} differs between {files[name]} and {src}")
        files.setdefault(name, src)
    for ins in INSTRUMENTS:
        d = os.path.join(REPO, "instruments", ins, "bin", "macos")
        for n in sorted(os.listdir(d)):
            if n in SKIP or n.endswith(SKIP_SUFFIX):
                continue
            put(n, os.path.join(d, n))
    gem = os.path.join(REPO, "build", "bela-gem")
    for n in sorted(os.listdir(gem)):
        if n.endswith(".pd_linux"):
            put(n, os.path.join(gem, n))
    fx = os.path.join(REPO, "effects", "vrs23")
    for n in sorted(os.listdir(fx)):
        if n.endswith(".pd") or n == "vrs23-clap.wav":
            put(n, os.path.join(fx, n))
    for proj in ("instruments/juno/bela/juno", "instruments/tx81z/bela/tx81zn-pd",
                 "instruments/tx81z/bela/tx81z-pd", "rack/bela/rack4"):
        src = os.path.join(REPO, proj)
        files["bela-projects/" + os.path.basename(proj)] = src
    return files


def contents(files):
    ext = sorted(n for n in files if n.endswith(".pd_darwin"))
    lines = []
    for e in ext:
        base = e[:-len(".pd_darwin")]
        lin = "yes" if base + ".pd_linux" in files else "-"
        help_ = base + "-help.pd" if base + "-help.pd" in files else "-"
        lines.append(f"| `{base}` | Mac | {'Bela Gem' if lin == 'yes' else '-'} | `{help_}` |")
    lines.append("| `vrs23~` (abstraction) | Mac | Bela Gem | `vrs23~-help.pd` |")
    return ("| Object | | | Help |\n|---|---|---|---|\n" + "\n".join(lines))


def worktree():
    if os.path.isdir(os.path.join(WT, ".git")) or os.path.isfile(os.path.join(WT, ".git")):
        return
    have = run("git", "branch", "--list", BRANCH)
    if not have:
        remote = run("git", "ls-remote", "--heads", "origin", BRANCH, check=False)
        if remote:
            run("git", "fetch", "origin", f"{BRANCH}:{BRANCH}")
            have = True
    if have:
        run("git", "worktree", "add", WT, BRANCH)
    else:
        run("git", "worktree", "add", "--detach", WT)
        run("git", "checkout", "--orphan", BRANCH, cwd=WT)
        run("git", "rm", "-rfq", ".", cwd=WT)


def main():
    files = collect()
    worktree()
    for n in os.listdir(WT):
        if n == ".git":
            continue
        p = os.path.join(WT, n)
        shutil.rmtree(p) if os.path.isdir(p) else os.remove(p)
    for n, src in files.items():
        dst = os.path.join(WT, n)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        shutil.copytree(src, dst, ignore=shutil.ignore_patterns("*.pd_linux", "*user-bank.*", "*.nvram", "*.bin")) \
            if os.path.isdir(src) else shutil.copy2(src, dst)
    open(os.path.join(WT, ".gitignore"), "w").write(IGNORE)
    open(os.path.join(WT, "README.md"), "w").write(README.replace("{contents}", contents(files)))
    open(os.path.join(WT, "bela-sync.sh"), "w").write(SYNC)
    head = run("git", "rev-parse", "--short", "HEAD")
    open(os.path.join(WT, "VERSION"), "w").write(f"main {head}\n")
    run("git", "add", "-A", cwd=WT)
    if not run("git", "status", "--porcelain", cwd=WT):
        print("dist: nothing changed")
    else:
        msg = f"pd from main {head}"
        if os.environ.get("DIST_TRAILER"):              # e.g. Co-Authored-By lines
            msg += "\n\n" + os.environ["DIST_TRAILER"]
        run("git", "commit", "-q", "-m", msg, cwd=WT)
        print("dist:", run("git", "log", "--oneline", "-1", cwd=WT), f"({len(files)} files)")
    if "--push" in sys.argv:
        run("git", "push", "-q", "origin", BRANCH, cwd=WT)
        print("dist: pushed", BRANCH)


if __name__ == "__main__":
    main()
