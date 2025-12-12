#include <stdio.h> // Pour printf, fprintf, stderr, FILE
#include <stdlib.h> // Pour free, malloc, strdup
#include <string.h> // Pour strdup, strlen, strcspn
#include <err.h>    // Pour err/errx
#include <ctype.h>  // Pour toupper
#include <stddef.h> // Pour size_t, ssize_t, NULL
#include "solver.h" // Pour Position, solve_puzzle, solver prototypes

// Directions de recherche
int directions[8][2] = {
    {0, 1}, {1, 1}, {1, 0}, {1, -1},
    {0, -1}, {-1, -1}, {-1, 0}, {-1, 1}
};

// Search a word in the grid 
Position* solver(char** grid, int rows, int cols, const char word[])
{
    Position* positions = (Position*)malloc(sizeof(Position) * 2);
    if (positions == NULL) err(1, "malloc failed");

    int len = strlen(word);
    if (len == 0) {
        free(positions);
        return NULL;
    }

    // Le solver recherche la première occurrence (sans faute ou avec 1 faute)
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            for (int d = 0; d < 8; d++) {
                int errors = 0;
                int k;
                
                if (grid[i][j] != word[0]) errors++;

                for (k = 1; k < len; k++) {
                    int newX = i + k * directions[d][0];
                    int newY = j + k * directions[d][1];
                    
                    if (newX < 0 || newX >= rows || newY < 0 || newY >= cols) break; 
                
                    if (grid[newX][newY] != word[k]) {
                        errors++; 
                        if (errors > 1) break; 
                    }
                }
                
                if (k == len && errors <= 1) {
                    positions[0].x = i;
                    positions[0].y = j;
                    positions[1].x = i + (len - 1) * directions[d][0];
                    positions[1].y = j + (len - 1) * directions[d][1];
                    return positions; 
                }
            }
        }
    }
    
    free(positions);
    return NULL; 
}

// Function to read the grid from a file
char** ReadGridFromFile(const char* filename, int* rows, int* cols) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) return NULL;

    char **grid = NULL;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    *rows = 0;
    *cols = 0;

    while ((read = getline(&line, &len, file)) != -1) {
        line[strcspn(line, "\r\n")] = '\0';
        size_t current_len = strlen(line);

        if (*rows == 0 || current_len > (size_t)*cols) {
            *cols = (int)current_len;
        }

        grid = (char**)realloc(grid, (*rows + 1) * sizeof(char*));
        if (grid == NULL) err(1, "realloc failed");

        grid[*rows] = (char*)malloc((*cols + 1) * sizeof(char));
        if (grid[*rows] == NULL) err(1, "malloc failed");
        
        strcpy(grid[*rows], line);

        for (int j = (int)current_len; j < *cols; j++) {
            grid[*rows][j] = ' ';
        }
        grid[*rows][*cols] = '\0';

        (*rows)++;
    }

    free(line);
    fclose(file);
    return grid;
}

// Fonction pour lire la liste des mots
char** ReadWordsFromFile(const char* filename, int* count) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) return NULL;

    char **words = NULL;
    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    *count = 0;

    while ((read = getline(&line, &len, file)) != -1) {
        line[strcspn(line, "\r\n")] = '\0';
        
        if (strlen(line) == 0) continue;

        words = realloc(words, (*count + 1) * sizeof(char*));
        if (!words) err(1, "realloc failed");

        words[*count] = strdup(line);
        (*count)++;
    }

    free(line);
    fclose(file);
    return words;
}


// --- Fonction principale du module (Exportée) ---

int solve_puzzle(const char *grid_file, const char *words_file) {
    printf("\n=== SOLVER MODULE ===\n");

    // 1. Charger et préparer la grille
    int rows = 0, cols = 0;
    char** grid = ReadGridFromFile(grid_file, &rows, &cols);

    if (grid == NULL || rows == 0) {
        fprintf(stderr, "Error loading grid from %s\n", grid_file);
        return 1;
    }

    // Convertir la grille en majuscules (pour comparaison insensible à la casse)
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            grid[i][j] = toupper((unsigned char)grid[i][j]);
        }
    }

    // 2. Charger les mots
    int word_count = 0;
    char** words = ReadWordsFromFile(words_file, &word_count);

    if (words == NULL) {
        fprintf(stderr, "Error loading words from %s\n", words_file);
        // Nettoyage de la grille
        for (int i = 0; i < rows; i++) free(grid[i]);
        free(grid);
        return 1;
    }

    printf("Grid size: %dx%d | Words to find: %d\n", cols, rows, word_count);
    printf("--------------------------------\n");

    // 3. Résoudre pour chaque mot
    for (int i = 0; i < word_count; i++) {
        // Préparation du mot à rechercher (copie + majuscule)
        char* search_word = strdup(words[i]);
        for(int c=0; search_word[c]; c++) search_word[c] = toupper((unsigned char)search_word[c]);

        Position* pos = solver(grid, rows, cols, search_word);

        if (pos != NULL) {
            printf("\033[0;32mFOUND\033[0m: %-15s (%d,%d) -> (%d,%d)\n", 
                   words[i], pos[0].x, pos[0].y, pos[1].x, pos[1].y);
            free(pos);
        } else {
            printf("\033[0;31mMISSING\033[0m: %-15s\n", words[i]);
        }

        free(search_word);
        free(words[i]);
    }

    // 4. Nettoyage
    free(words);
    for (int i = 0; i < rows; i++) free(grid[i]);
    free(grid);

    return 0;
}
