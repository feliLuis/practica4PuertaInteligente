"""Compila y ejecuta las pruebas de sensores en modo normal y diagnóstico."""

import argparse
from pathlib import Path
import shutil
import subprocess
from tempfile import TemporaryDirectory


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cxx", nargs="+", help="Compilador y argumentos, p. ej. zig c++")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    compiler = args.cxx
    if not compiler:
        desktop = shutil.which("g++") or shutil.which("clang++")
        if desktop:
            compiler = [desktop]
        else:
            local_zig = root / ".pio/host-tools/ziglang/zig.exe"
            zig = shutil.which("zig") or (str(local_zig) if local_zig.is_file() else None)
            if not zig:
                parser.error("Se necesita g++, clang++ o Zig. Usa --cxx si no está en PATH.")
            compiler = [zig, "c++"]

    tests = root / "tests"
    support = tests / "soporte_sensores"
    firmware = root / "firmware/esp32_sensores"
    protocol = root / "shared/Protocolo/src"
    sources = [
        tests / "sensores_tests.cpp",
        support / "Falsos.cpp",
        firmware / "src/SensorPuerta.cpp",
        firmware / "src/LectorRFID.cpp",
        firmware / "src/EnlaceSerial.cpp",
        protocol / "Protocolo.cpp",
    ]
    with TemporaryDirectory() as temp:
        for diagnostic in (0, 1):
            print(f"DIAGNOSTICO_UID={diagnostic}", flush=True)
            binary = Path(temp) / f"sensores_{diagnostic}.exe"
            subprocess.run(
                [
                    *compiler, "-std=c++11", f"-DDIAGNOSTICO_UID={diagnostic}",
                    "-I", support, "-I", firmware / "include", "-I", protocol,
                    *sources, "-o", binary,
                ],
                check=True,
            )
            subprocess.run([binary], check=True)


if __name__ == "__main__":
    main()
