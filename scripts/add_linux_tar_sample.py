#!/usr/bin/env python3
"""Extract a small, traceable Linux source sample into the dataset bundle."""

from __future__ import annotations

import argparse
import hashlib
import json
import tarfile
from pathlib import Path


VERSIONS = ("2.6.0", "2.6.5", "2.6.9")
MEMBERS = ("COPYING", "README", "init/main.c", "kernel/Makefile", "kernel/fork.c")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--source-dir",
        default="data/tar/kruus/public/linux/raw",
        help="directory containing linux-2.6.x.tar source archives",
    )
    parser.add_argument("--output", default="data")
    args = parser.parse_args()

    source_dir = Path(args.source_dir)
    root = Path(args.output)
    manifest_path = root / "manifest-paper-categories.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    target_root = root / "tar"
    expected_paths = {
        (Path("tar") / f"linux-{version}" / member_name).as_posix()
        for version in VERSIONS
        for member_name in MEMBERS
    }
    existing_manifest_paths = {
        item["path"] for item in manifest.get("files", [])
    }
    collisions = expected_paths & existing_manifest_paths
    existing_files = [root / relative for relative in expected_paths
                      if (root / relative).exists()]
    if collisions or existing_files:
        raise FileExistsError(
            "Linux sample already exists; refusing to overwrite or duplicate "
            "manifest entries")

    for version in VERSIONS:
        archive_path = source_dir / f"linux-{version}.tar"
        if not archive_path.is_file():
            raise FileNotFoundError(archive_path)
        prefix = f"linux-{version}/"
        with tarfile.open(archive_path, "r:") as archive:
            for member_name in MEMBERS:
                archive_member = prefix + member_name
                member = archive.getmember(archive_member)
                if not member.isfile():
                    raise RuntimeError(f"not a regular source file: {archive_member}")
                source = archive.extractfile(member)
                if source is None:
                    raise RuntimeError(f"cannot read archive member: {archive_member}")
                relative = Path("tar") / f"linux-{version}" / member_name
                destination = root / relative
                destination.parent.mkdir(parents=True, exist_ok=True)
                destination.write_bytes(source.read())
                manifest["files"].append({
                    "path": relative.as_posix(),
                    "bytes": destination.stat().st_size,
                    "sha256": sha256(destination),
                    "provenance": "selected file from local public Linux source archive",
                    "source_archive": f"linux-{version}.tar",
                    "archive_member": archive_member,
                })
    manifest["total_bytes"] = sum(item["bytes"] for item in manifest["files"])
    manifest["paper_original_datasets_included"] = False
    manifest["warning"] = (
        "tar/ contains selected Linux 2.6.0, 2.6.5, and 2.6.9 source files. "
        "WEB, RDB, SYN, VMA, and VMB are synthetic proxies, not original "
        "paper data."
    )
    manifest_path.write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({
        "status": "added",
        "tar_files": len(VERSIONS) * len(MEMBERS),
        "bundle_bytes": manifest["total_bytes"],
    }, indent=2))


if __name__ == "__main__":
    main()
