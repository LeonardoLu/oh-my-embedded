#!/usr/bin/env python3
"""Materialize pinned factory firmware + reviewed product patch in ignored tmp/."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

PROJECT = Path(__file__).resolve().parents[1]
REPO = PROJECT.parents[2]
LOCK = json.loads((PROJECT / "upstream.lock.json").read_text())

def run(*args, cwd=None):
    subprocess.run(args, cwd=cwd, check=True)

def fingerprint():
    digest = hashlib.sha256((PROJECT / "upstream.lock.json").read_bytes())
    for base in (PROJECT / "patches", PROJECT / "overlay"):
        for path in sorted(base.rglob("*")):
            if path.is_file():
                digest.update(str(path.relative_to(PROJECT)).encode())
                digest.update(path.read_bytes())
    return digest.hexdigest()

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--destination", type=Path, default=REPO / "tmp/mosaico/corallium-firmware")
    args = parser.parse_args()
    destination = args.destination.resolve()
    stamp = destination / ".corallium-source.json"
    signature = fingerprint()
    if destination.exists():
        if stamp.exists() and json.loads(stamp.read_text()).get("fingerprint") == signature:
            print(destination)
            return
        raise SystemExit(f"Refusing to overwrite {destination}; choose a fresh --destination (or preserve/remove that scratch tree yourself).")
    destination.mkdir(parents=True)
    run("git", "init", str(destination))
    run("git", "remote", "add", "origin", LOCK["repository"], cwd=destination)
    run("git", "fetch", "--depth=1", "origin", LOCK["revision"], cwd=destination)
    run("git", "checkout", "--detach", "FETCH_HEAD", cwd=destination)
    run("git", "submodule", "update", "--init", "--recursive", "--depth=1", cwd=destination)
    for patch in LOCK["patches"]:
        run("git", "apply", "--check", str(PROJECT / patch), cwd=destination)
        run("git", "apply", str(PROJECT / patch), cwd=destination)
    shutil.copytree(PROJECT / "overlay", destination, dirs_exist_ok=True)
    stamp.write_text(json.dumps({"fingerprint": signature, "upstream": LOCK["revision"]}, indent=2) + "\n")
    print(destination)

if __name__ == "__main__":
    main()
