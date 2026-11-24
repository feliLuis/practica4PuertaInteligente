"""Build and run controller and console tests with simulated hardware."""
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys

REPO = Path(__file__).resolve().parents[1]
BUILD = REPO / ".pio" / "tests"


def find_compiler():
    if os.environ.get("CXX"):
        return shlex.split(os.environ["CXX"], posix=os.name != "nt")
    for name in ("g++", "clang++"):
        path = shutil.which(name)
        if path:
            return [path]
    zig = REPO / ".pio" / "host-tools" / "ziglang" / ("zig.exe" if os.name == "nt" else "zig")
    if zig.is_file():
        return [str(zig), "c++"]
    raise SystemExit("No desktop C++ compiler found. Set CXX to g++ or clang++, "
                     "or use portable Zig as described in the README. No tests were run.")


def main():
    cxx = find_compiler()
    BUILD.mkdir(parents=True, exist_ok=True)
    environment = os.environ.copy()
    # Keep the test compiler cache inside the project and ignored by Git.
    environment["ZIG_GLOBAL_CACHE_DIR"] = str(BUILD / "zig-cache")
    control = "firmware/esp32_control"
    common = ["tests/stubs/Fakes.cpp", "shared/Protocol/src/Protocol.cpp"]
    logic = [f"{control}/src/LockController.cpp", f"{control}/src/CredentialList.cpp"]
    actuator = [f"{control}/src/LockServo.cpp"]
    jobs = [("control", control, ["tests/control_tests.cpp"] + logic + actuator, [])]
    for name, simulation, calibration in (("normal", 0, 0), ("simulation", 1, 0), ("calibration", 0, 1)):
        jobs.append(("console_" + name, control, ["tests/console_tests.cpp"] + logic +
                         ([] if simulation else actuator),
                         [f"-DSIMULATION_MODE={simulation}", f"-DCALIBRATION_MODE={calibration}"]))
    print("Compiler:", " ".join(cxx), flush=True)
    subprocess.run(cxx + ["--version"], check=True, env=environment)
    for name, project, sources, flags in jobs:
        target = BUILD / (name + (".exe" if os.name == "nt" else ""))
        command = cxx + ["-std=c++11", "-Wall", "-Wextra", "-Werror",
                         "-Itests/stubs", "-Itests", f"-I{project}/include",
                         "-Ishared/Protocol/src"] + flags + sources + common + ["-o", str(target)]
        print("\nBuilding and running", name, flush=True)
        subprocess.run(command, cwd=REPO, env=environment, check=True)
        subprocess.run([str(target)], cwd=REPO, env=environment, check=True)
    print("\nAll suites passed using simulated hardware.", flush=True)


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as error:
        sys.exit(error.returncode)
