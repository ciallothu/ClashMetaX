#!/usr/bin/env python3
"""
Generate mihomo.manifest for embedding into SysProxyBar.
"""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path


DEFAULT_DIR = Path("tools/mihomo")


def compute_sha256(path: Path) -> str:
    hasher = hashlib.sha256()
    with path.open("rb") as file:
        while True:
            chunk = file.read(1024 * 1024)
            if not chunk:
                break
            hasher.update(chunk)
    return hasher.hexdigest()


def read_existing_manifest_version(dest_dir: Path) -> str:
    manifest_path = dest_dir / "mihomo.manifest"
    if not manifest_path.exists():
        return ""

    for raw_line in manifest_path.read_text(encoding="ascii").splitlines():
        line = raw_line.strip()
        if line.startswith("version="):
            return line.partition("=")[2].strip()
    return ""


def resolve_version(dest_dir: Path, version: str | None) -> str:
    if version:
        return version.strip()
    return read_existing_manifest_version(dest_dir)


def generate_manifest(dest_dir: Path, version: str | None = None) -> Path:
    exe_path = dest_dir / "mihomo.exe"
    if not exe_path.exists():
        raise FileNotFoundError(f"Missing mihomo executable: {exe_path}")

    resolved_version = resolve_version(dest_dir, version)
    size = exe_path.stat().st_size
    sha256 = compute_sha256(exe_path)

    manifest_path = dest_dir / "mihomo.manifest"
    content = "# Generated at build time. Do not edit by hand.\n"
    if resolved_version:
        content += f"version={resolved_version}\n"
    content += f"size={size}\n"
    content += f"sha256={sha256}\n"
    manifest_path.write_text(content, encoding="ascii")
    return manifest_path


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate mihomo.manifest")
    parser.add_argument("--dir", default=str(DEFAULT_DIR), help="Directory containing mihomo.exe")
    parser.add_argument("--version", default=None, help="Override manifest version")
    args = parser.parse_args()

    dest_dir = Path(args.dir)
    manifest_path = generate_manifest(dest_dir, args.version)
    print(f"Generated manifest: {manifest_path}")
    print(manifest_path.read_text(encoding='ascii').strip())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
