# Image preprocessing

> [!NOTE]
> This part was made by **Maxan Fournier**.

## Overview

This project is an image preprocessing tool built in C using the GTK3 toolkit. It serves as the first step in the OCR pipeline, preparing an image for grid detection and character recognition.

Its main features include:

  * Loading standard image formats (PNG, JPEG, etc.).
  * **Dynamic Binarization:** Applies an automatic black & white filter using Otsu's method to robustly handle different lighting and contrast levels.
  * **Manual Rotation:** A slider allows for visual rotation of the image (from -180° to +180°) to correct skew.
  * **Processed Export:** Exports the *fully processed* (rotated and binarized) image as a clean PNG, ready for the next stage of the pipeline.

## Requirements
  * GTK3 development libraries (e.g., `libgtk-3-dev` on Debian/Ubuntu)

## Building

This project uses a `Makefile`. To compile the program, run the following command from the project's root directory:

```bash
make
```

This will create an executable file named `main`.

## Usage

Once compiled, you can execute the program by running:

```bash
./main
```