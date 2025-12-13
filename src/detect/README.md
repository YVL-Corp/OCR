# Image detection

> [!NOTE]
> This part was made by **Cyril Dejouhanet**.

## Overview

This is the base of the OCR extraction's script. It detects the position of a grid and its cells in an image.

## Requirements
  * GTK3 development libraries (e.g., `libgtk-3-dev` on Debian/Ubuntu)

## Building

This project uses a `Makefile`. To compile the program, run the following command from the project's root directory:

`make`

This will create an executable file named `extraction`.

## Usage

Once compiled, you can execute the program by running:

`./extraction <path_to_image>`

## Note

A file with preprocessed images is provided.
