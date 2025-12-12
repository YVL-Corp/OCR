#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <err.h>    
#include <ctype.h>  
#include <stddef.h> 
#include "solver.h" 

// Directions
int directions[8][2] = {
    {0, 1}, {1, 1}, {1, 0}, {1, -1},
    {0, -1}, {-1, -1}, {-1, 0}, {-1, 1}
};

Position* solver(char** grid, int rows, int cols, const char word[])
{
    Position* positions = (Position*)malloc(sizeof(Position) * 2);
    if (positions == NULL) err(1, "malloc failed");

    int len = strlen(word);
    if (len == 0) { free(positions); return NULL; }

    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            for (int d = 0; d < 8; d++) {
                int r = i, c = j, k;
                for (k = 0; k < len; k++) {
                    if (r < 0 || r >= rows || c < 0 || c >= cols || grid[r][c] != word[k]) 
                        break;
                    r += directions[d][0];
                    c += directions[d][1];
                }
                if (k == len) {
                    positions[0].x = j; positions[0].y = i;
                    positions[1].x = c - directions[d][1];
                    positions[1].y = r - directions[d][0];
                    return positions;
                }
            }
        }
    }
    free(positions);
    return NULL;
}

// --- CORRECTION ICI : Lecture Dynamique (Sans en-tête de taille) ---
char** ReadGridFromFile(const char* filename, int* rows, int* cols) {
    FILE *file = fopen(filename, "r");
    if (file == NULL) return NULL;

    char **grid = NULL;
    char buffer[2048]; // Buffer large pour lire une ligne
    *rows = 0;
    *cols = 0;

    while (fgets(buffer, sizeof(buffer), file)) {
        // Enlever le saut de ligne (\n ou \r\n)
        buffer[strcspn(buffer, "\r\n")] = '\0';
        
        int len = strlen(buffer);
        if (len == 0) continue; // Ignorer lignes vides

        // La première ligne définit le nombre de colonnes
        if (*rows == 0) {
            *cols = len;
        } 
        // Vérification de sécurité (si une ligne est plus courte/longue, on peut soit couper soit ignorer)
        else if (len != *cols) {
            // Optionnel: Gérer l'erreur ou forcer la taille. Ici on suppose que le NN est carré/rectangle.
        }

        grid = realloc(grid, (*rows + 1) * sizeof(char*));
        if (!grid) err(1, "realloc failed");

        grid[*rows] = strdup(buffer);
        (*rows)++;
    }

    fclose(file);
    return grid;
}

char** ReadWordsFromFile(const char* filename, int* count) {
    FILE* file = fopen(filename, "r");
    if (!file) return NULL;
    
    char** words = NULL;
    char buffer[256];
    *count = 0;

    while (fgets(buffer, sizeof(buffer), file)) {
        buffer[strcspn(buffer, "\r\n")] = 0; // Nettoyage \r et \n
        if (strlen(buffer) > 0) {
            words = realloc(words, sizeof(char*) * (*count + 1));
            words[*count] = strdup(buffer);
            (*count)++;
        }
    }
    fclose(file);
    return words;
}

// --- FONCTION PRINCIPALE ---
int solve_puzzle(const char *grid_file, const char *words_file, FoundLine **found_lines, int *lines_count) {
    printf("\n--- SOLVER MODULE ---\n");

    int rows = 0, cols = 0;
    char** grid = ReadGridFromFile(grid_file, &rows, &cols);

    if (grid == NULL || rows == 0) {
        fprintf(stderr, "Error loading grid from %s\n", grid_file);
        return 1;
    }

    // Mise en majuscule
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) grid[i][j] = toupper((unsigned char)grid[i][j]);
    }

    int word_count = 0;
    char** words = ReadWordsFromFile(words_file, &word_count);

    if (words == NULL) {
        fprintf(stderr, "Error loading words from %s\n", words_file);
        for (int i = 0; i < rows; i++) free(grid[i]);
        free(grid);
        return 1;
    }

    printf("Grid size: %dx%d | Words to find: %d\n", cols, rows, word_count);
    printf("--------------------------------\n");

    // Allocation des lignes trouvées pour l'interface
    *found_lines = malloc(sizeof(FoundLine) * word_count);
    *lines_count = 0;

    for (int i = 0; i < word_count; i++) {
        char* search_word = strdup(words[i]);
        for(int c=0; search_word[c]; c++) search_word[c] = toupper((unsigned char)search_word[c]);

        Position* pos = solver(grid, rows, cols, search_word);

        if (pos != NULL) {
            printf("\033[0;32mFOUND: %-15s (%d,%d) -> (%d,%d)\033[0m\n", 
                   words[i], pos[0].x, pos[0].y, pos[1].x, pos[1].y);
            
            // On sauve la ligne pour l'UI
            (*found_lines)[*lines_count].start_col = pos[0].x;
            (*found_lines)[*lines_count].start_row = pos[0].y;
            (*found_lines)[*lines_count].end_col   = pos[1].x;
            (*found_lines)[*lines_count].end_row   = pos[1].y;
            (*lines_count)++;

            free(pos);
        } else {
            printf("\033[0;31mMISSING: %s\033[0m\n", words[i]);
        }
        free(search_word);
        free(words[i]);
    }

    // Nettoyage
    free(words);
    for (int i = 0; i < rows; i++) free(grid[i]);
    free(grid);

    return 0;
}
