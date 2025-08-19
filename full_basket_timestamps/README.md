# full_basket_timestamps

This module provides utilities for extracting and integrating timestamp data from basket event files.

## Contents
- `src/integralEvents.cpp`: C++ code for integrating event data, with a focus on timestamp handling. As of August 2025, event numbers are global per event (not per channel), and the program supports an optional `--max-events N` argument to limit the number of events processed.
- `Makefile`: Build instructions for compiling the code in this module.

## Usage
1. Build the code:
   ```sh
   make
   ```
2. Use the resulting executable for timestamp analysis (see source code for details).

## Notes
- Complements the other modules in this repository for full basket data analysis.
- See the source code for more information on available features and usage.
