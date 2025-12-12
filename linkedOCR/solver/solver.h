#ifndef SOLVER_H
#define SOLVER_H

#include <stddef.h> 
#include <stdio.h> // Ajout pour NULL/fprintf dans le code métier si besoin, et pour être propre
#include <stdlib.h> // Ajout pour malloc/free/NULL
#include <string.h> // Ajout pour strdup/strlen
#include <ctype.h>  // Ajout pour toupper

// Structure to hold the position of a letter in the grid
typedef struct {
    int x; 
    int y; 
} Position;

// Prototypes fonctions for the solver and the grid reader
Position* solver(char** grid, int rows, int cols, const char word[]);
char** ReadGridFromFile(const char* filename, int* rows, int* cols);

// Nouvelle fonction pour charger la liste des mots
char** ReadWordsFromFile(const char* filename, int* count);

// Fonction principale du module Solver
int solve_puzzle(const char *grid_file, const char *words_file);

#endif
