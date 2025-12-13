#ifndef SOLVER_H
#define SOLVER_H

#include <stddef.h> 
#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <ctype.h> 

// pos struct
typedef struct {
    int x; 
    int y; 
} Position;

// line struct
typedef struct {
    int start_col, start_row;
    int end_col, end_row;
} FoundLine;
//prototypes functions
Position* solver(char** grid, int rows, int cols, const char word[]);
char** ReadGridFromFile(const char* filename, int* rows, int* cols);
char** ReadWordsFromFile(const char* filename, int* count);
int solve_puzzle(const char *grid_file, const char *words_file, FoundLine **lines, int *line_count);

#endif
