# full_basket_root

This module contains the core C++ code for decoding and processing basket data. It provides utilities to deserialize binary basket files, display event data, and perform event integration.

Caution: This code assumes that the order of the ADC device_id in the basket are in the same order that they appear in the raw data files. This might not always be correct. Refer `full_basket_root_adcMap` for more accurate conversion.

## Contents
- `src/deserializeBin.cpp`: Functions to read and decode binary basket data files.
- `src/displayEvents.cpp`: Code to display and analyze basket event data.
- `src/integralEvents.cpp`: Tools for integrating event data over time or other parameters. As of August 2025, event numbers are global per event (not per channel), and the program supports an optional `--max-events N` argument to limit the number of events processed.
- `Makefile`: Build instructions for compiling the C++ code in this module.

## Usage
1. Build the code:
   ```sh
   make
   ```
2. Run the executables as needed (see source code for details on usage and arguments).

## Notes
- Designed for modular use with other basket data tools in this repository.
- For more details, refer to the source code in the `src/` directory.
