"""Stage an explicitly pinned Cronet DLL. A digest is not proof of provenance."""
import hashlib
import os
from pathlib import Path
import re
import shutil
import sys
import tempfile


def main():
    if len(sys.argv) != 2:
        raise ValueError("expected one destination directory")
    source = os.environ.get("CRONET_DLL_PATH", "")
    expected = os.environ.get("CRONET_DLL_SHA256", "")
    if not source or not re.fullmatch(r"[a-fA-F0-9]{64}", expected):
        raise ValueError("set CRONET_DLL_PATH and verified CRONET_DLL_SHA256")
    src = Path(source)
    if not src.is_file():
        raise ValueError("Cronet DLL does not exist")
    target_dir = Path(sys.argv[1])
    target_dir.mkdir(parents=True, exist_ok=True)
    temp_name = None
    try:
        with tempfile.NamedTemporaryFile(dir=target_dir, delete=False) as dst, src.open("rb") as inp:
            temp_name = dst.name
            shutil.copyfileobj(inp, dst)
        with open(temp_name, "rb") as f:
            digest = hashlib.file_digest(f, "sha256").hexdigest()
        if digest.lower() != expected.lower():
            raise ValueError("Cronet DLL digest mismatch")
        os.replace(temp_name, target_dir / "libcronet.dll")
        temp_name = None
    finally:
        if temp_name:
            Path(temp_name).unlink(missing_ok=True)


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError) as exc:
        print(f"Cronet staging failed: {exc}", file=sys.stderr)
        sys.exit(1)
