#!/usr/bin/env python3
"""
Simple script to open a ROOT file with PyROOT and print the file basename and
the timestamp of the first and last events in the `events` TTree.

Usage:
  python3 scripts/print_root_timestamps.py /path/to/file.root

The script prints three fields separated by spaces:
  <basename> <first_timestamp> <last_timestamp>

If the tree is empty or missing, appropriate error messages are printed to stderr
and the script returns a non-zero exit code.
"""
import sys
import os

def gather_root_files(path):
    """Return a list of absolute paths to .root files found at path.
    If path is a file and ends with .root it is returned. If it's a directory,
    walk recursively and return all .root files.
    """
    files = []
    if os.path.isfile(path):
        if path.lower().endswith('.root'):
            files.append(os.path.abspath(path))
        return files
    for root, _, filenames in os.walk(path):
        for fn in filenames:
            if fn.lower().endswith('.root'):
                files.append(os.path.join(root, fn))
    return files


def main():
    if len(sys.argv) < 2 or len(sys.argv) > 3:
        print("Usage: {} /path/to/file_or_directory [output.csv]".format(sys.argv[0]), file=sys.stderr)
        return 2

    input_path = sys.argv[1]
    csv_path = sys.argv[2] if len(sys.argv) == 3 else os.path.join(os.getcwd(), 'root_timestamps.csv')
    if not os.path.exists(input_path):
        print(f"[ERROR] Path not found: {input_path}", file=sys.stderr)
        return 3

    root_files = gather_root_files(input_path)
    if not root_files:
        print(f"[ERROR] No .root files found under: {input_path}", file=sys.stderr)
        return 0

    # Import ROOT lazily so the script can error gracefully if ROOT is not available
    try:
        import ROOT
    except Exception as e:
        print(f"[ERROR] Failed to import ROOT: {e}", file=sys.stderr)
        return 4

    import array

    # We'll collect rows and optionally write a CSV. Also still print lines to stdout.
    exit_code = 0
    rows = []
    for rf in root_files:
        tf = ROOT.TFile.Open(rf)
        if not tf or tf.IsZombie():
            # skip files that can't be opened
            print(f"[WARN] Skipping (can't open): {rf}", file=sys.stderr)
            exit_code = 5
            continue

        tree = tf.Get('events')
        if not tree:
            # skip files without the tree
            tf.Close()
            continue

        n = int(tree.GetEntries())
        if n == 0:
            tf.Close()
            continue

        # read only the timestamp branch
        tree.SetBranchStatus('*', 0)
        tree.SetBranchStatus('timestamp', 1)
        ts_buf = array.array('L', [0])
        br = tree.GetBranch('timestamp')
        first_ts = None
        last_ts = None
        try:
            if br:
                br.SetAddress(ts_buf)
                tree.GetEntry(0)
                first_ts = int(ts_buf[0])
                tree.GetEntry(n - 1)
                last_ts = int(ts_buf[0])
            else:
                # fallback approaches
                try:
                    tree.SetBranchAddress('timestamp', ts_buf)
                    tree.GetEntry(0)
                    first_ts = int(ts_buf[0])
                    tree.GetEntry(n - 1)
                    last_ts = int(ts_buf[0])
                except Exception:
                    tree.GetEntry(0)
                    first_ts = int(getattr(tree, 'timestamp'))
                    tree.GetEntry(n - 1)
                    last_ts = int(getattr(tree, 'timestamp'))
        except Exception as e:
            print(f"[WARN] Unable to read 'timestamp' from {rf}: {e}", file=sys.stderr)
            tf.Close()
            exit_code = 8
            continue

        tf.Close()

        # Convert from nanoseconds to integer seconds
        try:
            first_ts_sec = int(first_ts // 1_000_000_000)
            last_ts_sec = int(last_ts // 1_000_000_000)
        except Exception:
            first_ts_sec = int(int(first_ts) / 1e9)
            last_ts_sec = int(int(last_ts) / 1e9)

        # compute delta in hours as float and round to 2 decimals
        delta_hours = round((last_ts_sec - first_ts_sec) / 3600.0, 2)

        # collect row: filename without extension, start, end, delta_hours, num events
        rows.append((os.path.splitext(os.path.basename(rf))[0], first_ts_sec, last_ts_sec, delta_hours, n))

    # Sort rows by numeric-aware natural key on filename (so '2' < '10')
    import re

    def natural_key(s):
        # split into text and number chunks: 'run_rc-hs2_045' -> ['run_rc-hs2_',  '045', '']
        parts = re.split(r'(\d+)', s)
        key = []
        for p in parts:
            if p.isdigit():
                key.append(int(p))
            else:
                key.append(p.lower())
        return tuple(key)

    rows_sorted = sorted(rows, key=lambda r: natural_key(r[0]))

    # Print sorted rows to stdout
    for fn, start_s, end_s, delta_h, nev in rows_sorted:
        print(f"{fn}.root {start_s} {end_s} {delta_h} {nev}")

    # Write CSV (sorted)
    try:
        import csv
        with open(csv_path, 'w', newline='') as cf:
            writer = csv.writer(cf)
            writer.writerow(['filename', 'start_s', 'end_s', 'delta_hours', 'num_events'])
            for r in rows_sorted:
                writer.writerow(r)
    except Exception as e:
        print(f"[WARN] Failed to write CSV {csv_path}: {e}", file=sys.stderr)
        exit_code = max(exit_code, 9)

    return exit_code


if __name__ == '__main__':
    sys.exit(main())
