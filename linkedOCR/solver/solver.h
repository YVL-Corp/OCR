#ifndef SOLVER_H
#define SOLVER_H

#include <stddef.h> 
#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <ctype.h> 

// Structure interne (existante)
typedef struct {
    int x; 
    int y; 
} Position;

// NOUVEAU : Structure pour renvoyer une ligne trouvée à l'interface
typedef struct {
    int start_col, start_row;
    int end_col, end_row;
} FoundLine;

Position* solver(char** grid, int rows, int cols, const char word[]);
char** ReadGridFromFile(const char* filename, int* rows, int* cols);
char** ReadWordsFromFile(const char* filename, int* count);

// MODIFIÉ : Renvoie maintenant les lignes trouvées via un pointeur
int solve_puzzle(const char *grid_file, const char *words_file, FoundLine **lines, int *line_count);

#endif
