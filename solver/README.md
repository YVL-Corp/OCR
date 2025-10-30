# Image detection

> [!NOTE]
> This part was made by **Martin Lemee**.

## Overview

This part of the code will solve the grid by searching a word in the grid. The grid is a .txt file.

## Building

This project uses a `Makefile`. To compile the program, run the following command from the project's root directory:

`make`

This will create an executable file named `solver`.

## Usage

Once compiled, you can execute the program by running:

`./solver <path_to_grid> <word>`

If the word was found the process returns the coords of the first and the last letter in the grid. Otherwise, it returns: "Not Found".
