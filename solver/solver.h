#ifndef SOLVER_H
#define SOLVER_H

// Définition des dimensions de la grille
#define ROWS 10
#define COLS 10


// Structure pour représenter une position dans la grille
typedef struct {
    int x;
    int y;
} Position;

// Fonction solver pour rechercher un mot dans la grille
Position* solver(char grid[ROWS][COLS], char word[]);

#endif
