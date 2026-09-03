import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2] / "Tools"))

from crowny.cli import main

if __name__ == "__main__":
    sys.exit(main())
