"""Verify packaged Cronet digests; digest equality alone does not prove origin."""
import hashlib
import os
from pathlib import Path
import re
import sys


def verify(root: Path, architectures: tuple[str, ...]) -> None:
    for arch in architectures:
        env_name = "CRONET_DLL_SHA256" if arch == "amd64" else "CRONET_ARM64_DLL_SHA256"
        expected = os.environ.get(env_name, "")
        if not re.fullmatch(r"[0-9a-fA-F]{64}", expected):
            raise ValueError(f"{env_name} must contain a verified SHA256")
        dll = root / "deployment" / f"windows-{arch}" / "libcronet.dll"
        with dll.open("rb") as stream:
            actual = hashlib.file_digest(stream, "sha256").hexdigest()
        if actual.lower() != expected.lower():
            raise ValueError(f"{arch} Cronet digest mismatch")


if __name__ == "__main__":
    try:
        if sys.argv[1:] not in (["x64"], ["all"]):
            raise ValueError("expected x64 or all")
        verify(Path(__file__).resolve().parent.parent,
               ("amd64",) if sys.argv[1] == "x64" else ("amd64", "arm64"))
    except (OSError, ValueError) as exc:
        print(f"Installer Cronet verification failed: {exc}", file=sys.stderr)
        sys.exit(1)
