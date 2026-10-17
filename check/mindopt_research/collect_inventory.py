"""Record SDK identities and CPU metadata without reading license credentials."""

import argparse
from datetime import datetime
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess
from zoneinfo import ZoneInfo


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mindopt-root", type=Path, default=Path(os.environ.get(
        "MINDOPT_RESEARCH_ROOT", Path(__file__).resolve().parents[2].parent / "mindopt")))
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    root = args.mindopt_root.expanduser().resolve()
    output = (args.output or root / "analysis/phase1/artifact-manifest.json").resolve()
    names = [
        "2.3.0/osx64-aarch/bin/mindopt",
        "2.3.0/osx64-aarch/lib/libmindopt.2.3.0.dylib",
        "2.3.0/osx64-aarch/lib/libmindoptmilp.2.3.0.dylib",
        "2.3.0/osx64-aarch/lib/libmindoptmiqcp.2.3.0.dylib",
        "2.3.0/osx64-aarch/lib/libmindoptpower.2.3.0.dylib",
        "2.3.0/osx64-aarch/lib/python/313/mindoptpy/mindoptpy.so",
        "artifacts/installers/mindopt-install-osx64aarch-2.3.0.sh",
    ]
    artifacts = []
    for name in names:
        path = root / name
        if not path.is_file():
            raise FileNotFoundError(path)
        digest = hashlib.sha256()
        with path.open("rb") as stream:
            for chunk in iter(lambda: stream.read(1024 * 1024), b""):
                digest.update(chunk)
        artifacts.append({"path": name, "bytes": path.stat().st_size,
                          "sha256": digest.hexdigest()})
    cpu = {}
    if platform.system() == "Darwin":
        for name in ["hw.model", "hw.physicalcpu_max", "hw.logicalcpu_max",
                     "hw.perflevel0.name", "hw.perflevel0.physicalcpu",
                     "hw.perflevel1.name", "hw.perflevel1.physicalcpu"]:
            result = subprocess.run(["/usr/sbin/sysctl", "-n", name],
                                    capture_output=True, text=True, timeout=10)
            cpu[name] = result.stdout.strip() if result.returncode == 0 else None
    manifest = {
        "schema_version": 1,
        "captured_at": datetime.now(ZoneInfo("Asia/Shanghai")).isoformat(),
        "paths_relative_to": "MINDOPT_RESEARCH_ROOT",
        "host": {"os": platform.system(), "version": platform.mac_ver()[0],
                 "architecture": platform.machine(), "cpu": cpu},
        "artifacts": artifacts,
    }
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"Recorded {len(artifacts)} artifact identities in {output}")


if __name__ == "__main__":
    main()
