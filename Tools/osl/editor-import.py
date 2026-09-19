"""Editor entry point. OSL/LLVM environment changes apply only to compiler children."""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from crowny.cli import main

if __name__ == "__main__":
    raise SystemExit(main(["osl", "compile", *sys.argv[1:]]))
