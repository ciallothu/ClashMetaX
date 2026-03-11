#!/usr/bin/env python3
"""
Download and prepare mihomo.exe for embedding into SysProxyBar
"""

import os
import sys
import urllib.request
import zipfile
from pathlib import Path

# Mihomo version to download
MIHOMO_VERSION = "v1.19.21"
MIHOMO_URL = f"https://github.com/MetaCubeX/mihomo/releases/download/{MIHOMO_VERSION}/mihomo-windows-amd64-v3-{MIHOMO_VERSION}.zip"
OUTPUT_DIR = Path("tools/mihomo")


def download_file(url, dest_path):
    """Download file from URL to destination path"""
    print(f"Downloading {url}")
    print(f"To: {dest_path}")

    try:
        # Create parent directory if needed
        dest_path.parent.mkdir(parents=True, exist_ok=True)

        # Download with progress indication
        def report_progress(block_num, block_size, total_size):
            downloaded = block_num * block_size
            if total_size > 0:
                percent = min(downloaded * 100 / total_size, 100)
                sys.stdout.write(f"\rProgress: {percent:.1f}% ({downloaded}/{total_size} bytes)")
                sys.stdout.flush()

        urllib.request.urlretrieve(url, dest_path, reporthook=report_progress)
        print()  # New line after progress

        return True
    except Exception as e:
        print(f"\nDownload failed: {e}")
        return False


def extract_zip(zip_path, dest_path):
    """Extract zip file to destination directory"""
    print(f"Extracting: {zip_path}")

    try:
        with zipfile.ZipFile(zip_path, 'r') as zip_ref:
            zip_ref.extractall(dest_path)

        print("Extraction complete")
        return True
    except Exception as e:
        print(f"Extraction failed: {e}")
        return False


def find_and_rename_mihomo_exe(search_dir):
    """Find mihomo.exe in the directory and rename it if needed"""
    print("Locating mihomo.exe...")

    # Search for .exe files
    exe_files = list(search_dir.rglob("*.exe"))

    if not exe_files:
        print("ERROR: No .exe file found in extracted archive")
        return False

    # Use the first .exe file found
    source_exe = exe_files[0]
    target_exe = search_dir / "mihomo.exe"

    if source_exe != target_exe:
        print(f"Renaming: {source_exe.name} -> mihomo.exe")
        source_exe.rename(target_exe)

    print(f"Mihomo executable: {target_exe}")

    # Check file size
    size_mb = target_exe.stat().st_size / (1024 * 1024)
    print(f"File size: {size_mb:.2f} MB")

    return True


def create_default_config(dest_dir):
    """Create minimal config.yaml"""
    config_path = dest_dir / "config.yaml"

    # Check if config already exists
    if config_path.exists():
        print(f"Config file already exists: {config_path}")
        return True

    print("Creating default config.yaml...")

    default_config = """# Mihomo minimal configuration for SysProxyBar
# Port settings
port: 7890
socks-port: 7891
allow-lan: true
mode: rule
log-level: info

# External controller (for WebUI)
external-controller: 127.0.0.1:9090

# DNS configuration
dns:
  enable: true
  listen: 0.0.0.0:53
  enhanced-mode: fake-ip
  fake-ip-range: 198.18.0.1/16
  nameserver:
    - 223.5.5.5
    - 119.29.29.29

# Proxy list (configure this or use external config)
proxies: []

# Proxy groups
proxy-groups:
  - name: "PROXY"
    type: select
    proxies:
      - DIRECT

# Rules
rules:
  - MATCH,PROXY
"""

    try:
        with open(config_path, 'w', encoding='utf-8') as f:
            f.write(default_config)

        print(f"Config file created: {config_path}")
        return True
    except Exception as e:
        print(f"Failed to create config: {e}")
        return False


def main():
    """Main function"""
    print("=" * 60)
    print("Mihomo Download and Embed Preparation Tool")
    print("=" * 60)
    print(f"Version: {MIHOMO_VERSION}")
    print()

    # Create output directory
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)

    # Download zip file
    zip_path = OUTPUT_DIR / "mihomo.zip"
    if not download_file(MIHOMO_URL, zip_path):
        return 1

    # Extract zip
    if not extract_zip(zip_path, OUTPUT_DIR):
        return 1

    # Find and rename exe
    if not find_and_rename_mihomo_exe(OUTPUT_DIR):
        return 1

    # Create default config
    if not create_default_config(OUTPUT_DIR):
        return 1

    # Clean up zip file
    print()
    print("Cleaning up...")
    zip_path.unlink()

    print()
    print("=" * 60)
    print("SUCCESS!")
    print("=" * 60)
    print(f"Files ready in: {OUTPUT_DIR.absolute()}")
    print("  - mihomo.exe")
    print("  - config.yaml")
    print()
    print("You can now build the project with:")
    print("  build_versioned.bat")
    print()

    return 0


if __name__ == "__main__":
    sys.exit(main())
