#!/usr/bin/env python3
"""
Download and update zashboard (WebUI) for SysProxyBar
"""

import shutil
import sys
import urllib.request
import zipfile
from pathlib import Path

ZASHBOARD_URL = "https://github.com/Zephyruso/zashboard/releases/latest/download/dist.zip"
DIST_DIR = Path("dist")
TEMP_ZIP = Path("dist.zip")


def download_file(url, dest_path):
    """Download file from URL to destination path"""
    print(f"Downloading {url}")

    def report_progress(block_num, block_size, total_size):
        downloaded = block_num * block_size
        if total_size > 0:
            percent = min(downloaded * 100 / total_size, 100)
            sys.stdout.write(f"\rProgress: {percent:.1f}% ({downloaded / (1024 * 1024):.1f}/{total_size / (1024 * 1024):.1f} MB)")
            sys.stdout.flush()

    try:
        urllib.request.urlretrieve(url, dest_path, reporthook=report_progress)
        print()
        return True
    except Exception as e:
        print(f"\nDownload failed: {e}")
        return False


def main():
    print("=" * 60)
    print("Zashboard Update Tool")
    print("=" * 60)
    print(f"Source: {ZASHBOARD_URL}")
    print(f"Target: {DIST_DIR.absolute()}")
    print()

    # Download zip
    if not download_file(ZASHBOARD_URL, TEMP_ZIP):
        return 1

    # Delete entire dist directory
    if DIST_DIR.exists():
        print("Removing old dist directory...")
        shutil.rmtree(DIST_DIR)

    # Extract zip directly to DIST_DIR
    print("Extracting...")
    try:
        with zipfile.ZipFile(TEMP_ZIP, 'r') as zip_ref:
            names = zip_ref.namelist()
            roots = set(n.split('/')[0] for n in names if n and not n.endswith('/'))
            prefix = (roots.pop() + '/') if len(roots) == 1 else ''
            for member in names:
                if not member or member.endswith('/'):
                    continue
                dest = DIST_DIR / member[len(prefix):]
                dest.parent.mkdir(parents=True, exist_ok=True)
                with zip_ref.open(member) as src, open(dest, 'wb') as dst:
                    dst.write(src.read())
        print("Extraction complete")
    except Exception as e:
        print(f"Extraction failed: {e}")
        return 1
    finally:
        TEMP_ZIP.unlink()

    # Summary
    file_count = sum(1 for f in DIST_DIR.rglob("*") if f.is_file())
    print()
    print("=" * 60)
    print("SUCCESS!")
    print("=" * 60)
    print(f"Updated zashboard in: {DIST_DIR.absolute()}")
    print(f"  Total files: {file_count}")
    print()
    print("Next steps:")
    print("  python generate_resources.py")
    print("  build_versioned.bat")
    print()

    return 0


if __name__ == "__main__":
    sys.exit(main())
