#!/usr/bin/env python3
import os
import shutil
import subprocess
import sys

import PyInstaller.__main__
from PyInstaller.utils.hooks import get_package_paths

MAKEFILE_DIR = os.path.join(os.path.dirname(__file__), "../nbt-editor")
HOOKS_DIR = os.path.join(os.path.dirname(__file__), "hooks")
APP_PATH = os.path.join(os.path.dirname(__file__), "app.py")
ICON_PATH = os.path.join(os.path.dirname(__file__), "../icon.ico")


def compile_c_code():
    lib_path = None
    if sys.platform.startswith("darwin") or sys.platform.startswith("linux"):
        # Macos or linus - .so file
        makefile = "Makefile.nix"
        lib_path = os.path.join(MAKEFILE_DIR, "build/libmc.so")
    elif sys.platform.startswith("win"):
        # Windows - .dll file
        makefile = "Makefile.win"
        lib_path = os.path.join(MAKEFILE_DIR, "build/libmc.dll")
    else:
        print("Unsupported platform")
        sys.exit(1)

    print(f"Compiling C code using {makefile}...")

    # Clean and then compile the C code.
    try:
        subprocess.check_call(["make", "-f", makefile, "clean"], cwd=MAKEFILE_DIR)
        subprocess.check_call(["make", "-f", makefile], cwd=MAKEFILE_DIR)
    except subprocess.CalledProcessError as e:
        print("Error during C code compilation:", e)
        sys.exit(1)
    return lib_path


def run_pyinstaller():
    lib_path = compile_c_code()
    llama_lib_path = os.path.join(get_package_paths("llama_cpp")[1], "lib")

    # Clean up previous builds if they exist.
    for folder in ["build", "dist"]:
        if os.path.exists(folder):
            shutil.rmtree(folder)

    print("Running PyInstaller...")
    try:
        PyInstaller.__main__.run(
            [
                APP_PATH,
                "--onedir",
                "--windowed",
                "--noconsole",
                "--add-data",
                f"{llama_lib_path}:llama_cpp/lib",
                "--add-data",
                f"{lib_path}:nbt-editor/build",
                "--add-data",
                f"{ICON_PATH}:.",
                "--name",
                "iBuild",
                "--icon=icon.ico",
            ]
        )
    except subprocess.CalledProcessError as e:
        print("Error during PyInstaller packaging:", e)
        sys.exit(1)


if __name__ == "__main__":
    run_pyinstaller()
    print("Build complete!")
