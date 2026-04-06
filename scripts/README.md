print_root_timestamps.py
========================

This small helper prints the basename of a ROOT file and the timestamp of the first
and last events in the `events` TTree. It uses PyROOT (the `ROOT` Python module).

Usage
-----

From the workspace root (fish shell example):

  python3 scripts/print_root_timestamps.py /full/path/to/myfile.root
  # Single file
  python3 scripts/print_root_timestamps.py /path/to/yourfile.root

  # Or a directory (recursively scans for .root files)
  python3 scripts/print_root_timestamps.py /path/to/some/directory

  # Optionally provide an output CSV path (default: ./root_timestamps.csv)
  python3 scripts/print_root_timestamps.py /path/to/some/directory /path/to/output.csv

Output
------

Prints a single line per file with five fields separated by spaces and writes a CSV
with the same information.

Line format:
  <basename> <first_timestamp_seconds> <last_timestamp_seconds> <delta_hours> <num_events>

CSV columns (written to the CSV file):
  filename (without .root), start_s, end_s, delta_hours, num_events

The CSV is sorted using a natural numeric-aware ordering on the filename so that
numeric parts are ordered numerically (e.g. '001', '002', ..., '010', '011', ...).

Timestamps are converted from nanoseconds to integer seconds. Delta is computed as
last_timestamp_seconds - first_timestamp_seconds and shown in hours as a float
rounded to two decimal places.

Notes
-----
- Make sure ROOT is installed and the `ROOT` Python module is importable.
- The script expects a TTree named `events` with a branch `timestamp`.
 - The script skips .root files that do not contain a non-empty `events` TTree.
