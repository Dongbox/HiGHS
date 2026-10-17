"""Verify MindOpt's Python API, keeping license credentials out of saved output."""

import argparse
import configparser
import json
import math
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile


def solve_examples():
    import importlib.metadata
    import platform
    from datetime import datetime
    from zoneinfo import ZoneInfo

    import mindoptpy as mp
    from mindoptpy import MDO

    results = []
    env = mp.Env(empty=True)
    try:
        env.start()
        env.setParam("MaxTime", 30)
        env.setParam("NumThreads", 2)
        for kind, variable_type, expected, expected_values in [
            ("LP", MDO.CONTINUOUS, 8.5, (0.5, 3.5)),
            ("MILP", MDO.INTEGER, 8.0, (0.0, 4.0)),
        ]:
            model = mp.Model(f"installation_check_{kind}", env)
            try:
                x = model.addVar(lb=0.0, vtype=variable_type, name="x")
                y = model.addVar(lb=0.0, vtype=variable_type, name="y")
                model.setObjective(3 * x + 2 * y, MDO.MAXIMIZE)
                model.addConstr(x + y <= 4, "capacity")
                model.addConstr(2 * x + y <= 4.5, "resource")
                model.optimize()
                if model.Status != MDO.OPTIMAL:
                    raise RuntimeError(f"{kind}: expected OPTIMAL, got {model.Status}")
                values = (x.X, y.X)
                if not math.isclose(model.ObjVal, expected, abs_tol=1e-7):
                    raise RuntimeError(f"{kind}: unexpected objective {model.ObjVal}")
                if not all(math.isclose(a, b, abs_tol=1e-7)
                           for a, b in zip(values, expected_values)):
                    raise RuntimeError(f"{kind}: unexpected variables {values}")
                if values[0] + values[1] > 4 + 1e-7 or 2 * values[0] + values[1] > 4.5 + 1e-7:
                    raise RuntimeError(f"{kind}: constraint violation")
                if kind == "MILP" and any(abs(v - round(v)) > 1e-7 for v in values):
                    raise RuntimeError("MILP: noninteger solution")
                results.append({"problem": kind, "status": "OPTIMAL",
                                "objective": model.ObjVal, "x": x.X, "y": y.X})
            finally:
                model.dispose()
    finally:
        env.dispose()

    report = {
        "verified_at": datetime.now(ZoneInfo("Asia/Shanghai")).isoformat(),
        "mindoptpy": importlib.metadata.version("mindoptpy"),
        "python": platform.python_version(),
        "architecture": platform.machine(),
        "license_directory": os.environ["MINDOPT_LICENSE_PATH"],
        "results": results,
        "environment_disposed": True,
    }
    print("VERIFICATION_JSON=" + json.dumps(report), flush=True)


def main():
    if sys.argv[1:] == ["--worker"]:
        solve_examples()
        return 0

    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mindopt-root", type=Path, default=Path(os.environ.get(
        "MINDOPT_RESEARCH_ROOT", Path(__file__).resolve().parents[2].parent / "mindopt")))
    parser.add_argument("--output-dir", type=Path)
    args = parser.parse_args()
    license_dir = args.mindopt_root.expanduser().resolve()
    config = configparser.ConfigParser(interpolation=None)
    if not config.read(license_dir / "ce_license.ini"):
        raise FileNotFoundError(f"Community license missing in {license_dir}")
    secrets = [value for value in config["security"].values() if value]
    env = os.environ.copy()
    env["MINDOPT_LICENSE_PATH"] = str(license_dir)
    env["MINDOPT_HOME"] = str(license_dir / "2.3.0")
    script = Path(__file__).resolve()
    output_dir = (args.output_dir or license_dir / "verification").resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    (output_dir / "python-results.json").unlink(missing_ok=True)

    def redact(output):
        for secret in sorted(secrets, key=len, reverse=True):
            output = output.replace(secret, "[REDACTED]")
        return output

    with tempfile.TemporaryDirectory(prefix="mindopt-cli-check-") as tmp:
        cli = subprocess.run([
            str(license_dir / "2.3.0/osx64-aarch/bin/mindopt"),
            str(license_dir / "2.3.0/examples/data/afiro.mps"),
        ], env=env, cwd=tmp, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, text=True, timeout=90)
    cli_output = redact(cli.stdout)
    (output_dir / "cli-afiro.log").write_text(cli_output)
    print(cli_output, end="")
    if cli.returncode:
        return cli.returncode
    objective = re.search(r"^\s*- Objective\s*:\s*([-+\d.eE]+)", cli_output, re.MULTILINE)
    if "OPTIMAL" not in cli_output or not objective:
        raise RuntimeError("CLI: expected an optimal objective")
    cli_objective = float(objective[1])
    if not math.isclose(cli_objective, -464.753142857, abs_tol=1e-6):
        raise RuntimeError(f"CLI: unexpected AFIRO objective {cli_objective}")
    # Capture native-library output too; redact it before printing or saving.
    with tempfile.TemporaryDirectory(prefix="mindopt-python-check-") as tmp:
        proc = subprocess.run([sys.executable, str(script), "--worker"], env=env,
                              cwd=tmp, stdout=subprocess.PIPE,
                              stderr=subprocess.STDOUT, text=True, timeout=120)
    output = redact(proc.stdout)
    (output_dir / "python.log").write_text(output)
    print(output, end="")
    if proc.returncode:
        return proc.returncode
    report_line = next(line for line in output.splitlines()
                       if line.startswith("VERIFICATION_JSON="))
    report = json.loads(report_line.partition("=")[2])
    report["cli"] = {"problem": "afiro.mps", "status": "OPTIMAL",
                     "objective": cli_objective, "exit_code": cli.returncode}
    (output_dir / "python-results.json").write_text(json.dumps(report, indent=2) + "\n")
    print("PASS: CLI, LP and MILP solved to the expected optima; environment released.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
