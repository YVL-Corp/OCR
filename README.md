# OCR WORD SEARCH SOLVER 

## Overview

This is the final version of YVL Corp's OCR Word Search Solver, made by:
- Cyril DEJOUHANET
- Tristan DRUART
- Maxan FOURNIER
- Martin LEMEE

## Building

This project uses a `Makefile`. To compile the program, run the following command from the `./src/` directory:

`make`

This will create an executable file named `ocr_solver`.

## Usage

Once compiled, you can execute the program by running from the `./src/` directory:

`make run`

Executing the program this way is preferred, as it hides the warnings generated when using GTK's file explorer : it will try to save the user's last used files, which is not necessary in our case.

## Note

4 PNGs are given in `./src/test_images/` to test the program.
