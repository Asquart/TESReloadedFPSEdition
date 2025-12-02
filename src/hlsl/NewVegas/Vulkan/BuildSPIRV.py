import os
import shutil
import subprocess
import sys
from pathlib import Path

# --- Figure out where this script/exe actually lives ---
if getattr(sys, "frozen", False):
    # Running as a PyInstaller-built exe
    BASE_DIR = Path(sys.executable).resolve().parent
else:
    # Running as a normal .py script
    BASE_DIR = Path(__file__).resolve().parent

# Optionally make that the working directory (helps glslangValidator.exe resolution)
os.chdir(BASE_DIR)

def main():
    print("============================================")
    print(" GLSL -> SPIR-V Batch Compiler (Python)")
    print("============================================\n")

    script_dir = BASE_DIR

    glsl_dir = os.path.join(script_dir, "glsl")
    out_dir = os.path.join(script_dir, "BuiltSPIRVs")

    print(f"GLSL DIR: {glsl_dir}")
    print(f"OUT  DIR: {out_dir}\n")

    if not os.path.isdir(glsl_dir):
        print("ERROR: 'glsl' folder not found next to this script.")
        sys.exit(1)

    # Clean output directory
    if os.path.exists(out_dir):
        print("Cleaning old output...")
        shutil.rmtree(out_dir)
    os.makedirs(out_dir, exist_ok=True)
    print()

    # Try to locate glslangValidator.exe
    validator_path = os.path.join(script_dir, "glslangValidator.exe")
    if not os.path.isfile(validator_path):
        # fallback: assume it's in PATH
        validator_path = "glslangValidator.exe"

    has_errors = False

    print('Scanning for *.glsl / *.comp.glsl (excluding any "Includes" folders)...\n')

    for root, dirs, files in os.walk(glsl_dir):
        # Skip any directory named "Includes" (case-insensitive)
        dirs[:] = [d for d in dirs if d.lower() != "includes"]

        for fname in files:
            # Only compile .glsl files (covers both *.glsl and *.comp.glsl)
            if not fname.lower().endswith(".glsl"):
                continue

            full_path = os.path.join(root, fname)

            # Relative path under glsl_dir
            rel_path = os.path.relpath(full_path, glsl_dir)

            # Split directory + base name
            rel_dir = os.path.dirname(rel_path)
            base_name, ext = os.path.splitext(os.path.basename(rel_path))  # ext should be ".glsl"

            # Output directory mirrors input subdirectory
            out_subdir = os.path.join(out_dir, rel_dir)
            os.makedirs(out_subdir, exist_ok=True)

            # Strip only ".glsl" -> .spv; keep ".comp" if present
            out_file = os.path.join(out_subdir, f"{base_name}.spv")

            print(f"Compiling:\n  {full_path}\n  -> {out_file}\n")

            cmd = [validator_path, "-V", full_path, "-o", out_file]

            try:
                result = subprocess.run(cmd, capture_output=True, text=True)
            except FileNotFoundError:
                print("ERROR: glslangValidator.exe not found.")
                has_errors = True
                break

            if result.returncode != 0:
                print("*** FAILED:")
                print(result.stdout)
                print(result.stderr)
                print()
                has_errors = True
            else:
                # Optional: print warnings if any
                if result.stdout.strip():
                    print(result.stdout.strip())
                print("OK\n")

    print("============================================")
    if has_errors:
        print("Compilation finished with ERRORS.")
    else:
        print("Compilation SUCCESS.")
    print("============================================")

    # Keep window open if double-clicked
    # if sys.stdout.isatty():
    #     input("\nPress Enter to exit...")

if __name__ == "__main__":
    main()
