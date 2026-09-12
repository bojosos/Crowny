#!/usr/bin/env python3
"""Apply the vendored Catch2 exit-code contract to a completed CI test run."""

import argparse


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("status", type=int, help="The test process exit code, including timeout/crash codes.")
    parser.add_argument("--allow-all-skipped", action="store_true", help="Allow an optional isolated test to skip.")
    args = parser.parse_args()

    # Catch2/src/catch2/catch_session.hpp defines AllTestsSkippedExitCode = 4.
    # Logs can contain SKIPPED before a later crash, so they cannot grant success.
    if args.status == 0 or (args.allow_all_skipped and args.status == 4):
        return 0
    print(f"Catch2 run failed with exit code {args.status}")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
