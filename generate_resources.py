#!/usr/bin/env python3
"""
Generate WebUI resource files script
Embed all static resources from dist directory into Windows resource files
"""

import os
import sys
from pathlib import Path

def path_to_resource_id(relative_path):
    """
    Convert file path to resource ID
    Example: assets/index.js -> IDR_FILE_ASSETS_INDEX_JS
    """
    # Remove leading slash
    path = relative_path.lstrip('/\\')

    # Replace path separators and extension separators with underscores
    resource_id = "IDR_FILE_" + path.upper()
    resource_id = resource_id.replace('/', '_')
    resource_id = resource_id.replace('\\', '_')
    resource_id = resource_id.replace('.', '_')
    resource_id = resource_id.replace('-', '_')

    return resource_id

def generate_resource_files(dist_dir="dist", output_rc="src/webui.rc", output_h="src/webui_resource.h"):
    """
    Generate resource files
    """
    dist_path = Path(dist_dir).resolve()

    if not dist_path.exists():
        print(f"Error: Directory {dist_dir} does not exist")
        return False

    # Collect all files
    files = []
    for file_path in dist_path.rglob("*"):
        if file_path.is_file():
            relative_path = file_path.relative_to(dist_path)
            # Skip version directories (v2.2.0, v2.2.1, etc.)
            parts = relative_path.parts
            if len(parts) > 0 and parts[0].startswith('v'):
                print(f"Skipping version directory: {relative_path}")
                continue
            files.append(relative_path)

    print(f"Found {len(files)} files")

    # Generate .rc file with relative paths
    # Note: .rc file is in src/, so we need paths relative to src/
    with open(output_rc, "w", encoding="utf-8") as rc_file:
        rc_file.write("// Auto-generated WebUI Resources\n")
        rc_file.write("// DO NOT EDIT MANUALLY\n")
        rc_file.write("// Paths are relative to the .rc file location (src/)\n\n")

        for i, file_path in enumerate(files):
            resource_id_num = 1000 + i
            # Calculate path relative to src/ directory
            relative_file_path = Path("..") / dist_dir / file_path
            # Use "WEBUI" as custom resource type to avoid conflicts
            rc_file.write(f'{resource_id_num} WEBUI "{relative_file_path.as_posix()}"\n')

    print(f"Generated: {output_rc}")

    # Generate .h file (with resource mapping table)
    with open(output_h, "w", encoding="utf-8") as h_file:
        h_file.write("// Auto-generated WebUI Resource IDs\n")
        h_file.write("// DO NOT EDIT MANUALLY\n\n")
        h_file.write("#pragma once\n\n")
        h_file.write("#include <map>\n")
        h_file.write("#include <string>\n\n")
        h_file.write("static std::map<std::string, int> g_webuiResourceMap = {\n")

        for i, file_path in enumerate(files):
            resource_id_num = 1000 + i
            file_path_str = str(file_path).replace('\\', '/')
            h_file.write(f'    {{"{file_path_str}", {resource_id_num}}},\n')

        h_file.write("};\n\n")

    print(f"Generated: {output_h}")
    print(f"Total embedded files: {len(files)}")

    return True

if __name__ == "__main__":
    success = generate_resource_files()
    sys.exit(0 if success else 1)
