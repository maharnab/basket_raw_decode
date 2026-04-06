# Full Basket ADC Map Project

This project provides tools for working with ADC mapping and event data for the ECAL PAS system. It includes C++ and Python utilities for database extraction, binary deserialization, event display, and integration.

## Directory Structure

- `src/` — Source code directory
  - `get_adcMap_direct.py` — Python script to extract ADC mapping from PostgreSQL and output to `adcMap.json` and timestamped snapshot files (`adcMap_snap_YYYYMMDD_HHMMSS.json`).
  - `deserializeBin.cpp` — C++ program to deserialize binary event data
  - `displayEvents.cpp` — C++ program to display event data
  - `integralEvents.cpp` — C++ program to process/integrate event data
  - `integralEvents_v2.cpp` — C++ program to process/integrate event data into ROOT RNTuple format with per-waveform t50 timing
- `adcMap.json` — Output file containing the ADC mapping
- `Makefile` — Build instructions for all C++ programs

## Requirements

### System Packages
- C++17 compatible compiler (e.g., g++)
- [ROOT](https://root.cern/) (for event-related programs)


#### Install on Ubuntu/Debian:
```sh
sudo apt-get update
sudo apt-get install g++ make
# Install ROOT as per https://root.cern/install/
```


### Python (for get_adcMap_direct.py)
- Python 3.8+
- `psycopg2-binary` (for PostgreSQL access in the Python script)

Install with:
```sh
pip install --break-system-packages psycopg2-binary
```

## Building the Project

From the project root directory, run:
```sh
make
```
This will build all C++ executables and place them in the `bin/` directory.

To clean all build artifacts:
```sh
make clean
```

## Executables Overview



### 1. get_adcMap_direct.py (Python)
- **Purpose:** Connects to the ECAL PAS PostgreSQL database, extracts ADC mapping, and writes to `adcMap.json` and a timestamped snapshot file (e.g., `adcMap_snap_YYYYMMDD_HHMMSS.json`).
- **Usage:**
  ```sh
  python3 src/get_adcMap_direct.py
  ```
  You will be prompted for the database username and password.

  After execution, both `adcMap.json` and a snapshot file (named `adcMap_snap_YYYYMMDD_HHMMSS.json`) will be created in the working directory.

### 2. deserializeBin
- **Purpose:** Deserializes binary event data files for further analysis or conversion.
- **Usage:**
  ```sh
  ./bin/deserializeBin [options] <input_file>
  ```
  (See code or help output for specific options.)
- **Dependencies:** Requires ROOT libraries.

### 3. displayEvents
- **Purpose:** Displays event data, likely for visualization or inspection.
- **Usage:**
  ```sh
  ./bin/displayEvents [options] <input_file>
  ```
  (See code or help output for specific options.)
- **Dependencies:** Requires ROOT libraries.


### 4. integralEvents

- **Purpose:** Processes or integrates event data, possibly for summary statistics or further analysis. As of August 2025, event numbers are assigned globally per event (not per channel), and you can optionally limit the number of events processed.

- **Usage:**

  Standard mode (uses `adcMap.json`):
  ```sh
  ./integralEvents <binary_file_directory_path> <minPosition_lower> <output_base_dir> <basket_number> [--max-events N]
  ```

  With custom ADC map (e.g., a snapshot file):
  ```sh
  ./integralEvents -M <adcMap_snap_YYYYMMDD_HHMMSS.json> <binary_file_directory_path> <minPosition_lower> <output_base_dir> <basket_number> [--max-events N]
  ```

  - The optional `--max-events N` argument will process only the first N global events from the data file(s).
  - If the `-M` option is used, the ADC order will be taken from the specified snapshot file instead of the default `adcMap.json`.

- **Dependencies:** Requires ROOT libraries.

### 5. integralEvents_v2

- **Purpose:** RNTuple-based implementation of event integration. Writes `event_number`, `device_id`, `timestamp`, `channel_number`, `channel_value`, and `t50_time` (ps) for each selected waveform channel.

- **Usage:**

  Single-file mode:
  ```sh
  ./bin/integralEvents_v2 [--max-events N] [-M adcMap.json] <datafile> <minPosition_lower> <output_base_dir> <basket_number>
  ```

  File-list mode:
  ```sh
  ./bin/integralEvents_v2 [--max-events N] [-M adcMap.json] -F <file_list.txt> <minPosition_lower> <output_base_dir> <basket_number>
  ```

- **Dependencies:** Requires ROOT libraries with RNTuple support.

## Notes
- All C++ programs are built with C++17 and require the listed libraries.
- Only the Python script requires access to the ECAL PAS database at `db-nica.jinr.ru`.
- If you encounter missing library errors, ensure all dependencies are installed and available in your system's include/library paths.

## Contact
For further information, contact the project maintainer (maharnabb@jinr.ru) or refer to the code comments for details on each program's logic.
