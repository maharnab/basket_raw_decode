# basket_raw_decode

This repository contains code and configuration for decoding, processing, and analyzing basket data from multiple sources. The project is organized into several submodules, each with a specific focus:

## Structure

- **full_basket_root/**: Core basket decoding and event processing code.
- **full_basket_root_adcMap/**: Tools and scripts for working with ADC mapping and basket data, including JSON map files and Python utilities.
- **full_basket_timestamps/**: Utilities for handling and analyzing basket event timestamps.

Each submodule contains its own `src/` directory with C++ source files and a `Makefile` for building the code. Some modules include additional scripts or configuration files.

## Getting Started

1. Clone this repository:
   ```sh
   git clone https://github.com/maharnab/basket_raw_decode.git
   ```
2. Navigate to the desired submodule and use the provided `Makefile` to build the code:
   ```sh
   cd full_basket_root
   make
   ```
   (Repeat for other submodules as needed.)

## Submodules Overview

### full_basket_root
- C++ code for deserializing binary basket data and displaying/integrating events.
- See `full_basket_root/README.md` for details.

### full_basket_root_adcMap
- Contains ADC mapping JSON files and scripts for direct ADC map extraction.
- Python and C++ utilities for advanced basket data analysis.
- See `full_basket_root_adcMap/README.md` for more information.

### full_basket_timestamps
- Focused on timestamp extraction and event integration for basket data.
- See `full_basket_timestamps/README.md` for details.

## License
This is a private repository. Contact the owner for usage permissions.
