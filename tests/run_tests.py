"""Compila y ejecuta el C++ real con reloj, GPIO, RFID y PWM falsos. Sin hardware."""
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys

REPO = Path(__file__).resolve().parents[1]
BUILD = REPO / ".pio" / "tests"


def compilador():
    if os.environ.get("CXX"):
        return shlex.split(os.environ["CXX"], posix=os.name != "nt")
    for nombre in ("g++", "clang++"):
        ruta = shutil.which(nombre)
        if ruta:
            return [ruta]
    zig = REPO / ".pio" / "host-tools" / "ziglang" / ("zig.exe" if os.name == "nt" else "zig")
    if zig.is_file():
        return [str(zig), "c++"]
    raise SystemExit("No hay compilador C++ de escritorio. Configure CXX con g++/clang++ "
                     "o use el Zig portatil descrito en README. No se ejecutaron pruebas.")


def main():
    cxx = compilador()
    BUILD.mkdir(parents=True, exist_ok=True)
    entorno = os.environ.copy()
    # Toda la cache del compilador de pruebas queda en el proyecto e ignorada por Git.
    entorno["ZIG_GLOBAL_CACHE_DIR"] = str(BUILD / "zig-cache")
    control = "firmware/esp32_control"
    sensores = "firmware/esp32_sensores"
    comun = ["tests/stubs/Falsos.cpp", "shared/Protocolo/src/Protocolo.cpp"]
    logica = [f"{control}/src/ControladorCerradura.cpp", f"{control}/src/ListaCredenciales.cpp"]
    actuador = [f"{control}/src/ServoCerrojo.cpp"]
    lectores = [f"{sensores}/src/SensorPuerta.cpp", f"{sensores}/src/LectorRFID.cpp",
                f"{sensores}/src/EnlaceSerial.cpp"]
    trabajos = [("control", control, ["tests/control_tests.cpp"] + logica + actuador, []),
                ("sensores_normal", sensores, ["tests/sensores_tests.cpp"] + lectores, ["-DDIAGNOSTICO_UID=0"]),
                ("sensores_diagnostico", sensores, ["tests/sensores_tests.cpp"] + lectores, ["-DDIAGNOSTICO_UID=1"])]
    for nombre, simulacion, calibracion in (("normal", 0, 0), ("simulacion", 1, 0), ("calibracion", 0, 1)):
        trabajos.append(("consola_" + nombre, control, ["tests/consola_tests.cpp"] + logica +
                         ([] if simulacion else actuador),
                         [f"-DMODO_SIMULACION={simulacion}", f"-DMODO_CALIBRACION={calibracion}"]))
    print("Compilador:", " ".join(cxx), flush=True)
    subprocess.run(cxx + ["--version"], check=True, env=entorno)
    for nombre, proyecto, fuentes, flags in trabajos:
        destino = BUILD / (nombre + (".exe" if os.name == "nt" else ""))
        comando = cxx + ["-std=c++11", "-Wall", "-Wextra", "-Werror",
                         "-Itests/stubs", "-Itests", f"-I{proyecto}/include",
                         "-Ishared/Protocolo/src"] + flags + fuentes + comun + ["-o", str(destino)]
        print("\nCompilando y ejecutando", nombre, flush=True)
        subprocess.run(comando, cwd=REPO, env=entorno, check=True)
        subprocess.run([str(destino)], cwd=REPO, env=entorno, check=True)
    print("\nTodas las suites finalizaron correctamente. Son pruebas simuladas, no fisicas.", flush=True)


if __name__ == "__main__":
    try:
        main()
    except subprocess.CalledProcessError as error:
        sys.exit(error.returncode)
