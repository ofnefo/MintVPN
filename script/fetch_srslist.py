"""Fetch the pinned Throne routing catalogue and verify it before use."""
from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import urllib.request

URL = "https://raw.githubusercontent.com/throneproj/routeprofiles/20ee6ea507e037ff9c45f8c883a649d36d041233/srslist.h"
SHA256 = "e74ad20cf752ba84883d40a36d363c5bbc42fefc686964eb92a77ca73dd1ed21"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("destination", type=Path, help="build directory for srslist.h")
    args = parser.parse_args()
    with urllib.request.urlopen(URL, timeout=30) as response:
        data = response.read(2_000_001)
    if len(data) > 2_000_000 or hashlib.sha256(data).hexdigest() != SHA256:
        raise SystemExit("upstream srslist.h failed size or SHA-256 verification")
    args.destination.mkdir(parents=True, exist_ok=True)
    target = args.destination / "srslist.h"
    if target.exists() and target.read_bytes() != data:
        raise SystemExit("destination already contains a different srslist.h")
    target.write_bytes(data)
    print("verified srslist.h:", SHA256)


if __name__ == "__main__":
    main()
