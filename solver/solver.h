#ifndef SOLVER_H
#define SOLVER_H

#include <stddef.h> 

// Structure to hold the position of a letter in the grid
typedef struct {
    int x; 
    int y; 
} Position;

// Prototypes fonctions for the solver and the grid reader
Position* solver(char** grid, int rows, int cols, const char word[]);
char** ReadGridFromFile(const char* filename, int* rows, int* cols);

#endif